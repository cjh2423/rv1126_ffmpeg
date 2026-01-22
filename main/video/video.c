/**
 * @file video.c
 * @author Antigravity (Enhanced with Multi-threading)
 * @brief 视频采集与编码模块实现 - 多线程分离架构
 * 
 * 该模块采用三线程流水线架构，将视频处理流程分离为独立的阶段：
 * 
 * ┌─────────────┐     ┌─────────────┐     ┌─────────────┐
 * │  采集线程   │ --> │  编码线程   │ --> │  推流线程   │
 * │ (VI Thread) │     │(VENC Thread)│     │(RTSP Thread)│
 * └─────────────┘     └─────────────┘     └─────────────┘
 *        │                   │                   │
 *        ▼                   ▼                   ▼
 *    获取YUV帧          送帧到编码器         推送到RTSP
 *                       获取编码码流
 * 
 * 线程间通信使用线程安全的帧队列 (FrameQueue)：
 * - raw_queue:     采集线程 -> 编码线程 (传递 YUV 帧)
 * - stream_queue:  编码线程 -> 推流线程 (传递编码码流)
 * 
 * 优势：
 * 1. 解耦各处理阶段，便于独立调优和调试
 * 2. 利用多核 CPU 并行处理，提升吞吐量
 * 3. 队列缓冲可平滑各阶段的处理时间波动
 * 4. 更灵活的错误处理和资源管理
 */

#include "video.h"

#include "config.h"
#include "log.h"
#include "param.h"
#include "rtsp.h"
#include "frame_queue.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

#include <rk_mpi_mb.h>
#include <rk_mpi_sys.h>
#include <rk_mpi_vi.h>
#include <rk_mpi_venc.h>

/* =========================================================================
 *                              宏定义与常量
 * ========================================================================= */

/** @brief 原始帧队列容量 (采集 -> 编码) */
#define RAW_QUEUE_CAPACITY      4

/** @brief 编码码流队列容量 (编码 -> 推流) */
#define STREAM_QUEUE_CAPACITY   8

/** @brief 线程等待超时时间 (毫秒) */
#define THREAD_TIMEOUT_MS       1000

/* =========================================================================
 *                              全局变量与结构定义
 * ========================================================================= */

/**
 * @brief 单路视频流处理的上下文结构
 * 
 * 封装了一路视频流处理所需的所有资源，包括线程句柄、队列、配置等。
 */
typedef struct {
    const VideoConfig *cfg;      /**< 配置信息 */
    
    /* 线程句柄 */
    pthread_t vi_thread;         /**< 采集线程 */
    pthread_t venc_thread;       /**< 编码线程 */
    pthread_t rtsp_thread;       /**< 推流线程 */
    
    /* 线程有效标志 */
    int vi_thread_valid;
    int venc_thread_valid;
    int rtsp_thread_valid;
    
    /* 帧队列 */
    FrameQueue *raw_queue;       /**< 原始 YUV 帧队列 */
    FrameQueue *stream_queue;    /**< 编码码流队列 */
    
    /* RTSP 时间戳基准 */
    int64_t rtsp_base_time_us;
    int64_t rtsp_base_pts;
    
    /* 运行控制 */
    volatile int running;        /**< 线程运行标志 */
} VideoStreamContext;

/** @brief 视频流上下文 (支持两路) */
static VideoStreamContext g_stream_ctx[2];

/** @brief 全局运行标志 */
static volatile int g_video_run = 0;

/** @brief VI 源通道句柄 */
static MPP_CHN_S g_vi_chn;

/* =========================================================================
 *                              内部辅助函数
 * ========================================================================= */

/**
 * @brief 获取当前系统微秒级时间戳
 * 
 * @return int64_t 当前时间的微秒数 (Unix Epoch)
 */
static int64_t get_realtime_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

/* =========================================================================
 *                              编码线程
 * ========================================================================= */

/**
 * @brief 视频编码线程函数
 * 
 * 从 raw_queue 获取原始帧，送入 VENC 编码器，获取编码后码流并推送到 stream_queue。
 * 
 * 注意：由于使用了 Bind 模式，这里改为直接从 VENC 获取码流。
 * Non-Bind 模式需要手动调用 RK_MPI_VENC_SendFrame。
 * 
 * @param arg VideoStreamContext 指针
 */
static void *venc_encode_thread(void *arg) {
    VideoStreamContext *ctx = (VideoStreamContext *)arg;
    const VideoConfig *cfg = ctx->cfg;
    
    LOG_INFO("[VENC-%d] Encode thread started\n", cfg->venc_chn_id);
    
    VENC_STREAM_S stStream;
    memset(&stStream, 0, sizeof(stStream));
    stStream.pstPack = malloc(sizeof(VENC_PACK_S));
    if (!stStream.pstPack) {
        LOG_ERROR("[VENC-%d] Failed to allocate VENC pack memory\n", cfg->venc_chn_id);
        return NULL;
    }
    
    while (ctx->running && g_video_run) {
        // 从 VENC 获取编码后的码流
        int ret = RK_MPI_VENC_GetStream(cfg->venc_chn_id, &stStream, THREAD_TIMEOUT_MS);
        if (ret != RK_SUCCESS) {
            // 超时或缓冲区空时不打印警告日志
            if (ret != RK_ERR_VENC_BUF_EMPTY) {
                /* LOG_WARN("[VENC-%d] GetStream ret: 0x%x\n", cfg->venc_chn_id, ret); */
            }
            continue;
        }
        
        void *data = RK_MPI_MB_Handle2VirAddr(stStream.pstPack->pMbBlk);
        size_t len = stStream.pstPack->u32Len;
        
        if (data && len > 0) {
            // 封装编码帧
            FrameData stream_frame;
            memset(&stream_frame, 0, sizeof(stream_frame));
            stream_frame.type = FRAME_TYPE_ENCODED;
            stream_frame.pts = stStream.pstPack->u64PTS;
            stream_frame.size = len;
            stream_frame.is_keyframe = (stStream.pstPack->DataType.enH264EType == H264E_NALU_ISLICE ||
                                        stStream.pstPack->DataType.enH264EType == H264E_NALU_IDRSLICE ||
                                        stStream.pstPack->DataType.enH265EType == H265E_NALU_ISLICE ||
                                        stStream.pstPack->DataType.enH265EType == H265E_NALU_IDRSLICE);
            
            // 分配内存并拷贝数据 (因为码流缓冲区会被复用)
            stream_frame.data = malloc(len);
            if (stream_frame.data) {
                memcpy(stream_frame.data, data, len);
                
                // 推送到流队列
                if (frame_queue_push(ctx->stream_queue, &stream_frame, THREAD_TIMEOUT_MS) != 0) {
                    LOG_WARN("[VENC-%d] Stream queue push failed\n", cfg->venc_chn_id);
                    free(stream_frame.data);
                }
            }
        }
        
        // 释放码流资源
        RK_MPI_VENC_ReleaseStream(cfg->venc_chn_id, &stStream);
    }
    
    if (stStream.pstPack) free(stStream.pstPack);
    
    LOG_INFO("[VENC-%d] Encode thread exiting\n", cfg->venc_chn_id);
    return NULL;
}

/* =========================================================================
 *                              推流线程
 * ========================================================================= */

/**
 * @brief RTSP 推流线程函数
 * 
 * 从 stream_queue 获取编码后码流，推送到 RTSP 服务器。
 * 
 * @param arg VideoStreamContext 指针
 */
static void *rtsp_push_thread(void *arg) {
    VideoStreamContext *ctx = (VideoStreamContext *)arg;
    const VideoConfig *cfg = ctx->cfg;
    FILE *fp = NULL;
    
    LOG_INFO("[RTSP-%d] Push thread started\n", cfg->rtsp_id);
    
#if APP_Test_SAVE_FILE == 1
    if (cfg->output_path && cfg->output_path[0] != '\0') {
        fp = fopen(cfg->output_path, "wb");
        if (!fp) {
            LOG_ERROR("[RTSP-%d] Failed to open output file %s\n", cfg->rtsp_id, cfg->output_path);
        }
    }
#endif
    
    while (ctx->running && g_video_run) {
        FrameData stream_frame;
        
        // 从队列获取编码后的帧
        int ret = frame_queue_pop(ctx->stream_queue, &stream_frame, THREAD_TIMEOUT_MS);
        if (ret != 0) {
            continue;  // 超时或队列关闭
        }
        
#if APP_Test_RTSP == 1
        if (stream_frame.data && stream_frame.size > 0) {
            // 初始化 RTSP 时间戳基准（首帧逻辑）
            if (ctx->rtsp_base_time_us < 0) {
                ctx->rtsp_base_time_us = get_realtime_us();
                ctx->rtsp_base_pts = (int64_t)stream_frame.pts;
            }
            
            // 计算当前帧相对于首帧的时间偏移，转换成绝对系统时间推送给 RTSP
            int64_t pts_offset = (int64_t)stream_frame.pts - ctx->rtsp_base_pts;
            int64_t rtsp_pts = ctx->rtsp_base_time_us + pts_offset;
            
            rkipc_rtsp_write_video_frame(cfg->rtsp_id, stream_frame.data, 
                                          stream_frame.size, rtsp_pts);
        }
#endif
        
#if APP_Test_SAVE_FILE == 1
        if (fp && stream_frame.data && stream_frame.size > 0) {
            fwrite(stream_frame.data, 1, stream_frame.size, fp);
            fflush(fp);
        }
#endif
        
        // 释放帧数据内存
        if (stream_frame.data) {
            free(stream_frame.data);
        }
    }
    
    // 处理队列中剩余的帧
    FrameData stream_frame;
    while (frame_queue_try_pop(ctx->stream_queue, &stream_frame) == 0) {
        if (stream_frame.data) {
            free(stream_frame.data);
        }
    }
    
    if (fp) fclose(fp);
    
    LOG_INFO("[RTSP-%d] Push thread exiting\n", cfg->rtsp_id);
    return NULL;
}

/* =========================================================================
 *                              硬件初始化相关
 * ========================================================================= */

/**
 * @brief 初始化 VI 物理设备并绑定逻辑 Pipe
 * 
 * @param cfg 配置参数
 * @return 0 成功, 非 0 失败
 */
static int vi_dev_init(const VideoConfig *cfg) {
    VI_DEV_ATTR_S dev_attr;
    VI_DEV_BIND_PIPE_S bind_pipe;
    int ret;

    memset(&dev_attr, 0, sizeof(dev_attr));
    memset(&bind_pipe, 0, sizeof(bind_pipe));

    ret = RK_MPI_VI_GetDevAttr(cfg->vi_dev_id, &dev_attr);
    if (ret == RK_ERR_VI_NOT_CONFIG) {
        ret = RK_MPI_VI_SetDevAttr(cfg->vi_dev_id, &dev_attr);
        if (ret != RK_SUCCESS) {
            LOG_ERROR("RK_MPI_VI_SetDevAttr dev %d failed: 0x%x\n", cfg->vi_dev_id, ret);
            return ret;
        }
    }

    ret = RK_MPI_VI_GetDevIsEnable(cfg->vi_dev_id);
    if (ret != RK_SUCCESS) {
        ret = RK_MPI_VI_EnableDev(cfg->vi_dev_id);
        if (ret != RK_SUCCESS) {
            LOG_ERROR("RK_MPI_VI_EnableDev dev %d failed: 0x%x\n", cfg->vi_dev_id, ret);
            return ret;
        }
        
        bind_pipe.u32Num = 1;
        bind_pipe.PipeId[0] = cfg->vi_pipe_id;
        ret = RK_MPI_VI_SetDevBindPipe(cfg->vi_dev_id, &bind_pipe);
        if (ret != RK_SUCCESS) {
            LOG_ERROR("RK_MPI_VI_SetDevBindPipe dev %d to pipe %d failed: 0x%x\n", 
                      cfg->vi_dev_id, cfg->vi_pipe_id, ret);
            return ret;
        }
    }

    return 0;
}

/**
 * @brief 初始化输出 NV12 视频帧的 VI 通道
 * 
 * @param cfg 配置参数
 * @return 0 成功, -1 失败
 */
static int vi_chn_init(const VideoConfig *cfg) {
    VI_CHN_ATTR_S chn_attr;

    memset(&chn_attr, 0, sizeof(chn_attr));
    
    chn_attr.stIspOpt.u32BufCount = 4;
    chn_attr.stIspOpt.enMemoryType = VI_V4L2_MEMORY_TYPE_DMABUF;
    
    chn_attr.stSize.u32Width = cfg->width;
    chn_attr.stSize.u32Height = cfg->height;
    chn_attr.enPixelFormat = RK_FMT_YUV420SP;
    chn_attr.enCompressMode = COMPRESS_MODE_NONE;
    
    if (cfg->vi_entity_name) {
        strncpy(chn_attr.stIspOpt.aEntityName, cfg->vi_entity_name,
                sizeof(chn_attr.stIspOpt.aEntityName) - 1);
    }
    
    // 设置更大的深度以支持多线程缓冲
    chn_attr.u32Depth = 2;

    if (RK_MPI_VI_SetChnAttr(cfg->vi_pipe_id, cfg->vi_chn_id, &chn_attr) != RK_SUCCESS) {
        LOG_ERROR("RK_MPI_VI_SetChnAttr pipe %d chn %d failed\n", cfg->vi_pipe_id, cfg->vi_chn_id);
        return -1;
    }
    
    if (RK_MPI_VI_EnableChn(cfg->vi_pipe_id, cfg->vi_chn_id) != RK_SUCCESS) {
        LOG_ERROR("RK_MPI_VI_EnableChn pipe %d chn %d failed\n", cfg->vi_pipe_id, cfg->vi_chn_id);
        return -1;
    }

    return 0;
}

/**
 * @brief 初始化 VENC 通道
 * 
 * @param cfg 配置参数
 * @return 0 成功, -1 失败
 */
static int venc_init(const VideoConfig *cfg) {
    VENC_CHN_ATTR_S venc_attr;
    VENC_RECV_PIC_PARAM_S recv_param;

    memset(&venc_attr, 0, sizeof(venc_attr));

    if (cfg->codec == APP_VIDEO_CODEC_H265) {
        venc_attr.stVencAttr.enType = RK_VIDEO_ID_HEVC;
        venc_attr.stRcAttr.enRcMode = VENC_RC_MODE_H265CBR;
        venc_attr.stRcAttr.stH265Cbr.u32Gop = cfg->gop;
        venc_attr.stRcAttr.stH265Cbr.u32BitRate = cfg->bitrate;
        venc_attr.stRcAttr.stH265Cbr.fr32DstFrameRateNum = cfg->fps;
        venc_attr.stRcAttr.stH265Cbr.fr32DstFrameRateDen = 1;
        venc_attr.stRcAttr.stH265Cbr.u32SrcFrameRateNum = cfg->fps;
        venc_attr.stRcAttr.stH265Cbr.u32SrcFrameRateDen = 1;
    } else {
        venc_attr.stVencAttr.enType = RK_VIDEO_ID_AVC;
        venc_attr.stRcAttr.enRcMode = VENC_RC_MODE_H264CBR;
        venc_attr.stRcAttr.stH264Cbr.u32Gop = cfg->gop;
        venc_attr.stRcAttr.stH264Cbr.u32BitRate = cfg->bitrate;
        venc_attr.stRcAttr.stH264Cbr.fr32DstFrameRateNum = cfg->fps;
        venc_attr.stRcAttr.stH264Cbr.fr32DstFrameRateDen = 1;
        venc_attr.stRcAttr.stH264Cbr.u32SrcFrameRateNum = cfg->fps;
        venc_attr.stRcAttr.stH264Cbr.u32SrcFrameRateDen = 1;
    }

    venc_attr.stVencAttr.enPixelFormat = RK_FMT_YUV420SP;
    venc_attr.stVencAttr.u32PicWidth = cfg->width;
    venc_attr.stVencAttr.u32PicHeight = cfg->height;
    venc_attr.stVencAttr.u32VirWidth = cfg->width;
    venc_attr.stVencAttr.u32VirHeight = cfg->height;
    
    venc_attr.stVencAttr.u32StreamBufCnt = 5;
    venc_attr.stVencAttr.u32BufSize = cfg->width * cfg->height * 3 / 2;

    if (RK_MPI_VENC_CreateChn(cfg->venc_chn_id, &venc_attr) != RK_SUCCESS) {
        LOG_ERROR("RK_MPI_VENC_CreateChn %d failed\n", cfg->venc_chn_id);
        return -1;
    }

    memset(&recv_param, 0, sizeof(recv_param));
    recv_param.s32RecvPicNum = -1;
    if (RK_MPI_VENC_StartRecvFrame(cfg->venc_chn_id, &recv_param) != RK_SUCCESS) {
        LOG_ERROR("RK_MPI_VENC_StartRecvFrame %d failed\n", cfg->venc_chn_id);
        return -1;
    }

    return 0;
}

/**
 * @brief 初始化单路视频流处理上下文
 * 
 * @param ctx 上下文指针
 * @param cfg 配置信息
 * @param vi_chn VI 通道句柄指针
 * @return 0 成功, -1 失败
 */
static int stream_context_init(VideoStreamContext *ctx, const VideoConfig *cfg, 
                               MPP_CHN_S *vi_chn) {
    MPP_CHN_S venc_chn;
    
    memset(ctx, 0, sizeof(*ctx));
    ctx->cfg = cfg;
    ctx->rtsp_base_time_us = -1;
    ctx->rtsp_base_pts = 0;
    
    // 创建帧队列
    ctx->raw_queue = frame_queue_create(RAW_QUEUE_CAPACITY);
    ctx->stream_queue = frame_queue_create(STREAM_QUEUE_CAPACITY);
    if (!ctx->raw_queue || !ctx->stream_queue) {
        LOG_ERROR("Failed to create frame queues for chn %d\n", cfg->venc_chn_id);
        return -1;
    }
    
    // 初始化 VENC
    if (venc_init(cfg) != 0) {
        return -1;
    }
    
    // 建立 VI -> VENC 绑定 (仍使用硬件 Bind 提高效率)
    venc_chn.enModId = RK_ID_VENC;
    venc_chn.s32DevId = 0;
    venc_chn.s32ChnId = cfg->venc_chn_id;
    
    if (RK_MPI_SYS_Bind(vi_chn, &venc_chn) != RK_SUCCESS) {
        LOG_ERROR("RK_MPI_SYS_Bind VI->VENC[%d] failed\n", cfg->venc_chn_id);
        return -1;
    }
    
    ctx->running = 1;
    
    // 启动编码线程 (从 VENC 获取码流)
    if (pthread_create(&ctx->venc_thread, NULL, venc_encode_thread, ctx) != 0) {
        LOG_ERROR("Failed to create VENC thread for chn %d\n", cfg->venc_chn_id);
        ctx->running = 0;
        return -1;
    }
    ctx->venc_thread_valid = 1;
    
    // 启动推流线程
    if (pthread_create(&ctx->rtsp_thread, NULL, rtsp_push_thread, ctx) != 0) {
        LOG_ERROR("Failed to create RTSP thread for chn %d\n", cfg->venc_chn_id);
        ctx->running = 0;
        return -1;
    }
    ctx->rtsp_thread_valid = 1;
    
    LOG_INFO("Stream context for chn %d initialized (VENC thread + RTSP thread)\n", 
             cfg->venc_chn_id);
    
    return 0;
}

/**
 * @brief 销毁单路视频流处理上下文
 * 
 * @param ctx 上下文指针
 * @param vi_chn VI 通道句柄指针
 */
static void stream_context_deinit(VideoStreamContext *ctx, MPP_CHN_S *vi_chn) {
    if (!ctx || !ctx->cfg) return;
    
    MPP_CHN_S venc_chn;
    venc_chn.enModId = RK_ID_VENC;
    venc_chn.s32DevId = 0;
    venc_chn.s32ChnId = ctx->cfg->venc_chn_id;
    
    // 停止线程
    ctx->running = 0;
    
    // 关闭队列，唤醒阻塞的线程
    if (ctx->raw_queue) frame_queue_close(ctx->raw_queue);
    if (ctx->stream_queue) frame_queue_close(ctx->stream_queue);
    
    // 等待线程结束
    if (ctx->vi_thread_valid) {
        pthread_join(ctx->vi_thread, NULL);
        ctx->vi_thread_valid = 0;
    }
    if (ctx->venc_thread_valid) {
        pthread_join(ctx->venc_thread, NULL);
        ctx->venc_thread_valid = 0;
    }
    if (ctx->rtsp_thread_valid) {
        pthread_join(ctx->rtsp_thread, NULL);
        ctx->rtsp_thread_valid = 0;
    }
    
    // 解除绑定
    RK_MPI_SYS_UnBind(vi_chn, &venc_chn);
    
    // 销毁 VENC 通道
    RK_MPI_VENC_StopRecvFrame(ctx->cfg->venc_chn_id);
    RK_MPI_VENC_DestroyChn(ctx->cfg->venc_chn_id);
    
    // 销毁队列
    if (ctx->raw_queue) {
        frame_queue_destroy(ctx->raw_queue);
        ctx->raw_queue = NULL;
    }
    if (ctx->stream_queue) {
        frame_queue_destroy(ctx->stream_queue);
        ctx->stream_queue = NULL;
    }
    
    LOG_INFO("Stream context for chn %d deinitialized\n", ctx->cfg->venc_chn_id);
}

/* =========================================================================
 *                              外部接口实现
 * ========================================================================= */

/**
 * @brief 初始化视频采集与编码子系统 (多线程架构)
 * 
 * @return 0 成功, 非 0 失败
 */
int rk_video_init(void) {
    const VideoConfig *cfg = app_video_config_get();
    const VideoConfig *cfg1 = app_video1_config_get();
    int ret;

    LOG_INFO("=== Initializing video subsystem (Multi-threaded) ===\n");

    // 设置 VI 源通道信息
    g_vi_chn.enModId = RK_ID_VI;
    g_vi_chn.s32DevId = cfg->vi_dev_id;
    g_vi_chn.s32ChnId = cfg->vi_chn_id;
    
    memset(g_stream_ctx, 0, sizeof(g_stream_ctx));

    // 1. 初始化 VI 硬件
    ret = vi_dev_init(cfg);
    if (ret) return ret;
    
    ret = vi_chn_init(cfg);
    if (ret) return ret;

#if APP_Test_RTSP == 1
    // 2. 配置 RTSP 编码格式参数
    rk_param_set_string("video.0:output_data_type",
                        (cfg->codec == APP_VIDEO_CODEC_H265) ? "H.265" : "H.264");
    if (APP_Test_VENC1 == 1) {
        rk_param_set_string("video.1:output_data_type",
                            (cfg1->codec == APP_VIDEO_CODEC_H265) ? "H.265" : "H.264");
    }

    // 初始化 RTSP Server
    ret = rkipc_rtsp_init(cfg->rtsp_url, (APP_Test_VENC1 == 1) ? cfg1->rtsp_url : NULL, NULL);
    if (ret) {
        LOG_ERROR("rkipc_rtsp_init failed\n");
        return ret;
    }
#endif

    g_video_run = 1;

    // 3. 初始化主码流处理上下文
    ret = stream_context_init(&g_stream_ctx[0], cfg, &g_vi_chn);
    if (ret) {
        g_video_run = 0;
        return ret;
    }

    // 4. 初始化子码流处理上下文 (若开启)
    if (APP_Test_VENC1 == 1) {
        ret = stream_context_init(&g_stream_ctx[1], cfg1, &g_vi_chn);
        if (ret) {
            stream_context_deinit(&g_stream_ctx[0], &g_vi_chn);
            g_video_run = 0;
            return ret;
        }
    }

    LOG_INFO("=== Video subsystem initialized successfully ===\n");
    LOG_INFO("  Pipeline: [VI] --(Bind)--> [VENC Thread] --> [RTSP Thread]\n");
    LOG_INFO("  Streams: Main=%dx%d@%dfps, Sub=%s\n",
             cfg->width, cfg->height, cfg->fps,
             (APP_Test_VENC1 == 1) ? "enabled" : "disabled");
    
    return 0;
}

/**
 * @brief 停止视频子系统并释放资源
 * 
 * @return 0 成功
 */
int rk_video_deinit(void) {
    const VideoConfig *cfg = app_video_config_get();

    LOG_INFO("=== Deinitializing video subsystem ===\n");

    // 1. 停止全局运行标志
    g_video_run = 0;

    // 2. 销毁各路流上下文
    if (APP_Test_VENC1 == 1) {
        stream_context_deinit(&g_stream_ctx[1], &g_vi_chn);
    }
    stream_context_deinit(&g_stream_ctx[0], &g_vi_chn);

#if APP_Test_RTSP == 1
    // 3. 关闭 RTSP 服务
    rkipc_rtsp_deinit();
#endif

    // 4. 禁用并关闭 VI 通道与设备
    RK_MPI_VI_DisableChn(cfg->vi_pipe_id, cfg->vi_chn_id);
    RK_MPI_VI_DisableDev(cfg->vi_dev_id);

    LOG_INFO("=== Video subsystem deinitialized ===\n");
    return 0;
}
