/**
 * @file video.c
 * @author Antigravity (Enhanced)
 * @brief 视频采集与编码模块实现
 * 
 * 该模块负责从 Rockchip VI (Video Input) 获取视频数据，并通过 VENC (Video Encoder) 进行编码。
 * 支持多路码流编码（如主/副码流）、RTSP 推流以及本地文件保存。
 * 
 * 核心流程:
 * 1. 初始化 VI 设备与通道 (NV12 格式)
 * 2. 初始化 VENC 编码器 (H.264/H.265 CBR 模式)
 * 3. 使用 MPP 系统的 Bind 机制连接 VI -> VENC
 * 4. 创建工作线程循环获取编码后的码流数据
 * 5. 将码流推送至 RTSP 服务器或存入本地文件
 */

#include "video.h"

#include "config.h"
#include "log.h"
#include "param.h"
#include "rtsp.h"

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
 *                              全局变量与结构定义
 * ========================================================================= */

/** @brief 编码数据提取线程句柄 */
static pthread_t g_venc_thread[2];
/** @brief 线程是否正在运行的标志位 */
static int g_venc_thread_valid[2];
/** @brief 全局运行控制开关，置为 0 时正常退出线程 */
static int g_video_run = 0;

/** @brief VI 源通道句柄 */
static MPP_CHN_S g_vi_chn;
/** @brief VENC 目标通道句柄（支持两路） */
static MPP_CHN_S g_venc_chn[2];

/** @brief RTSP 相对时间戳计算基准：系统实时时间 (us) */
static int64_t g_rtsp_base_time_us[2] = {-1, -1};
/** @brief RTSP 相对时间戳计算基准：MPP 内部 PTS */
static int64_t g_rtsp_base_pts[2] = {0, 0};

/**
 * @brief 传递给编码工作线程的参数
 */
typedef struct {
    const VideoConfig *cfg;  /**< 指向当前通道的配置信息 */
} VencThreadArgs;

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
 *                              线程工作函数
 * ========================================================================= */

/**
 * @brief 编码码流获取线程
 * 
 * 在循环中调用 RK_MPI_VENC_GetStream 获取视频包，并分发到 RTSP 或 文件。
 * 该线程由 rk_video_init 创建，并在 rk_video_deinit 中通过 join 回收。
 * 
 * @param arg 指向 VencThreadArgs 的指针（在线程结束时释放）
 */
static void *venc_get_stream_thread(void *arg) {
    VencThreadArgs *args = (VencThreadArgs *)arg;
    const VideoConfig *cfg = args->cfg;
    FILE *fp = NULL;

    LOG_INFO("VENC thread for chn %d started\n", cfg->venc_chn_id);

#if APP_Test_SAVE_FILE == 1
    // 如果启用本地存图功能，尝试打开文件
    if (cfg->output_path && cfg->output_path[0] != '\0') {
        fp = fopen(cfg->output_path, "wb");
        if (!fp) {
            LOG_ERROR("failed to open output file %s\n", cfg->output_path);
        }
    }
#endif

    VENC_STREAM_S stStream;
    memset(&stStream, 0, sizeof(stStream));
    // 预分配包描述符内存
    stStream.pstPack = malloc(sizeof(VENC_PACK_S));
    if (!stStream.pstPack) {
        LOG_ERROR("failed to allocate VENC pack memory\n");
        if (fp) fclose(fp);
        free(args);
        return NULL;
    }

    while (g_video_run) {
        // 非阻塞/超时模式获取码流 (1000ms 超时)
        int ret = RK_MPI_VENC_GetStream(cfg->venc_chn_id, &stStream, 1000);
        if (ret == RK_SUCCESS) {
            // 获取码流映射后的虚拟地址
            void *data = RK_MPI_MB_Handle2VirAddr(stStream.pstPack->pMbBlk);
            
#if APP_Test_RTSP == 1
            if (data && stStream.pstPack->u32Len > 0) {
                // 初始化 RTSP 时间戳基准（首帧逻辑）
                if (g_rtsp_base_time_us[cfg->rtsp_id] < 0) {
                    g_rtsp_base_time_us[cfg->rtsp_id] = get_realtime_us();
                    g_rtsp_base_pts[cfg->rtsp_id] = (int64_t)stStream.pstPack->u64PTS;
                }
                
                // 计算当前帧相对于首帧的时间偏移，转换成绝对系统时间推送给 RTSP
                int64_t pts_offset = (int64_t)stStream.pstPack->u64PTS -
                                     g_rtsp_base_pts[cfg->rtsp_id];
                int64_t rtsp_pts = g_rtsp_base_time_us[cfg->rtsp_id] + pts_offset;
                
                rkipc_rtsp_write_video_frame(cfg->rtsp_id, data, stStream.pstPack->u32Len,
                                             rtsp_pts);
            }
#endif

#if APP_Test_SAVE_FILE == 1
            // 写入本地调试文件
            if (fp && data && stStream.pstPack->u32Len > 0) {
                fwrite(data, 1, stStream.pstPack->u32Len, fp);
                fflush(fp);
            }
#endif
            // 释放码流资源
            RK_MPI_VENC_ReleaseStream(cfg->venc_chn_id, &stStream);
        }
    }

    LOG_INFO("VENC thread for chn %d exiting...\n", cfg->venc_chn_id);

    // 资源清理
    if (stStream.pstPack) free(stStream.pstPack);
    if (fp) fclose(fp);
    free(args);
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

    // 获取并设置设备属性（若未配置则初始化）
    ret = RK_MPI_VI_GetDevAttr(cfg->vi_dev_id, &dev_attr);
    if (ret == RK_ERR_VI_NOT_CONFIG) {
        ret = RK_MPI_VI_SetDevAttr(cfg->vi_dev_id, &dev_attr);
        if (ret != RK_SUCCESS) {
            LOG_ERROR("RK_MPI_VI_SetDevAttr dev %d failed: 0x%x\n", cfg->vi_dev_id, ret);
            return ret;
        }
    }

    // 启用设备并绑定 Pipe（关联物理传感器数据流到逻辑处理单元）
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
    
    // 配置 V4L2 相关的采集选项
    chn_attr.stIspOpt.u32BufCount = 4;
    chn_attr.stIspOpt.enMemoryType = VI_V4L2_MEMORY_TYPE_DMABUF; // 使用 DMABUF 零拷贝模式
    
    // 设置采集图像分辨率与格式
    chn_attr.stSize.u32Width = cfg->width;
    chn_attr.stSize.u32Height = cfg->height;
    chn_attr.enPixelFormat = RK_FMT_YUV420SP;      // 通常为 NV12
    chn_attr.enCompressMode = COMPRESS_MODE_NONE;  // 不使用硬件码流压缩
    
    // 若指定了实体名称（如 rkisp_mainpath），则进行设置
    if (cfg->vi_entity_name) {
        strncpy(chn_attr.stIspOpt.aEntityName, cfg->vi_entity_name,
                sizeof(chn_attr.stIspOpt.aEntityName) - 1);
    }
    
    // 深度缓存 1，保证流畅性的同时降低延迟
    chn_attr.u32Depth = 1;

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
 * @brief 初始化 VENC 通道，设置码率控制 (RC) 与编码协议 (H.264/H.265)
 * 
 * @param cfg 配置参数
 * @return 0 成功, -1 失败
 */
static int venc_init(const VideoConfig *cfg) {
    VENC_CHN_ATTR_S venc_attr;
    VENC_RECV_PIC_PARAM_S recv_param;

    memset(&venc_attr, 0, sizeof(venc_attr));

    // 根据配置选择编码协议并设置 CBR (固定码率) 模式
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

    // 设置输入视频属性
    venc_attr.stVencAttr.enPixelFormat = RK_FMT_YUV420SP;
    venc_attr.stVencAttr.u32PicWidth = cfg->width;
    venc_attr.stVencAttr.u32PicHeight = cfg->height;
    venc_attr.stVencAttr.u32VirWidth = cfg->width;
    venc_attr.stVencAttr.u32VirHeight = cfg->height;
    
    // 设置码流缓冲区配置
    venc_attr.stVencAttr.u32StreamBufCnt = 5;
    venc_attr.stVencAttr.u32BufSize = cfg->width * cfg->height * 3 / 2;

    // 创建编码通道
    if (RK_MPI_VENC_CreateChn(cfg->venc_chn_id, &venc_attr) != RK_SUCCESS) {
        LOG_ERROR("RK_MPI_VENC_CreateChn %d failed\n", cfg->venc_chn_id);
        return -1;
    }

    // 开始接收输入图像并编码
    memset(&recv_param, 0, sizeof(recv_param));
    recv_param.s32RecvPicNum = -1; // -1 表示无限循环接收
    if (RK_MPI_VENC_StartRecvFrame(cfg->venc_chn_id, &recv_param) != RK_SUCCESS) {
        LOG_ERROR("RK_MPI_VENC_StartRecvFrame %d failed\n", cfg->venc_chn_id);
        return -1;
    }

    // 缓存当前的 VENC 通道句柄，供 Bind 使用
    g_venc_chn[cfg->rtsp_id].enModId = RK_ID_VENC;
    g_venc_chn[cfg->rtsp_id].s32DevId = 0;
    g_venc_chn[cfg->rtsp_id].s32ChnId = cfg->venc_chn_id;

    return 0;
}

/* =========================================================================
 *                              外部接口实现
 * ========================================================================= */

/**
 * @brief 初始化视频采集与编码子系统
 * 
 * 包括：从配置获取参数、初始化硬件通路、启动 RTSP、绑定 VI 到 VENC、启动抓取线程。
 * 
 * @return 0 成功, 非 0 失败
 */
int rk_video_init(void) {
    const VideoConfig *cfg = app_video_config_get();
    const VideoConfig *cfg1 = app_video1_config_get();
    int ret;

    LOG_INFO("Initializing video subsystem...\n");

    // 设置基础 VI 源信息
    g_vi_chn.enModId = RK_ID_VI;
    g_vi_chn.s32DevId = cfg->vi_dev_id;
    g_vi_chn.s32ChnId = cfg->vi_chn_id;
    
    g_venc_thread_valid[0] = 0;
    g_venc_thread_valid[1] = 0;

    // 1. 初始化第一路（主流）硬件链路
    ret = vi_dev_init(cfg);
    if (ret) return ret;
    
    ret = vi_chn_init(cfg);
    if (ret) return ret;
    
    ret = venc_init(cfg);
    if (ret) return ret;

#if APP_Test_RTSP == 1
    // 2. 配置并开启 RTSP 服务
    // 更新参数系统以便 RTSP 层知道正确的编码格式
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

    // 3. 建立数据绑路：VI 通道 -> VENC 通道 (0 路)
    if (RK_MPI_SYS_Bind(&g_vi_chn, &g_venc_chn[0]) != RK_SUCCESS) {
        LOG_ERROR("RK_MPI_SYS_Bind VI->VENC[0] failed\n");
        return -1;
    }

    // 4. 开启抓流线程 (0 路)
    g_video_run = 1;
    VencThreadArgs *args0 = malloc(sizeof(*args0));
    if (!args0) {
        LOG_ERROR("failed to alloc thread args for chn 0\n");
        g_video_run = 0;
        return -1;
    }
    args0->cfg = cfg;
    if (pthread_create(&g_venc_thread[0], NULL, venc_get_stream_thread, args0) != 0) {
        LOG_ERROR("failed to create venc[0] thread\n");
        g_video_run = 0;
        free(args0);
        return -1;
    }
    g_venc_thread_valid[0] = 1;

    // 5. 初始化第二路（子流）链路（若开启）
    if (APP_Test_VENC1 == 1) {
        ret = venc_init(cfg1);
        if (ret) return ret;
        
        // 绑定同一 VI 通道到不同的 VENC 通道进行不同配置的编码
        if (RK_MPI_SYS_Bind(&g_vi_chn, &g_venc_chn[cfg1->rtsp_id]) != RK_SUCCESS) {
            LOG_ERROR("RK_MPI_SYS_Bind VI->VENC[1] failed\n");
            return -1;
        }
        
        VencThreadArgs *args1 = malloc(sizeof(*args1));
        if (!args1) {
            LOG_ERROR("failed to alloc thread args for chn 1\n");
            g_video_run = 0;
            return -1;
        }
        args1->cfg = cfg1;
        if (pthread_create(&g_venc_thread[1], NULL, venc_get_stream_thread, args1) != 0) {
            LOG_ERROR("failed to create venc[1] thread\n");
            g_video_run = 0;
            free(args1);
            return -1;
        }
        g_venc_thread_valid[1] = 1;
    }

    LOG_INFO("Video subsystem initialized successfully.\n");
    return 0;
}

/**
 * @brief 停止视频子系统并关闭硬件
 * 
 * 按照初始化相反的顺序优雅地停止线程、解绑通路、销毁通道。
 * 
 * @return 0 成功
 */
int rk_video_deinit(void) {
    const VideoConfig *cfg = app_video_config_get();
    const VideoConfig *cfg1 = app_video1_config_get();

    LOG_INFO("Deinitializing video subsystem...\n");

    // 1. 停止工作线程
    g_video_run = 0;
    if (g_venc_thread_valid[0]) {
        pthread_join(g_venc_thread[0], NULL);
        g_venc_thread_valid[0] = 0;
    }
    if (g_venc_thread_valid[1]) {
        pthread_join(g_venc_thread[1], NULL);
        g_venc_thread_valid[1] = 0;
    }

    // 2. 解除 MPP 绑定关系
    RK_MPI_SYS_UnBind(&g_vi_chn, &g_venc_chn[0]);
    if (APP_Test_VENC1 == 1)
        RK_MPI_SYS_UnBind(&g_vi_chn, &g_venc_chn[1]);

#if APP_Test_RTSP == 1
    // 3. 关闭 RTSP 服务
    rkipc_rtsp_deinit();
#endif

    // 4. 销毁 VENC 通道资源
    RK_MPI_VENC_StopRecvFrame(cfg->venc_chn_id);
    RK_MPI_VENC_DestroyChn(cfg->venc_chn_id);
    if (APP_Test_VENC1 == 1) {
        RK_MPI_VENC_StopRecvFrame(cfg1->venc_chn_id);
        RK_MPI_VENC_DestroyChn(cfg1->venc_chn_id);
    }

    // 5. 禁用并关闭 VI 通道与设备
    RK_MPI_VI_DisableChn(cfg->vi_pipe_id, cfg->vi_chn_id);
    RK_MPI_VI_DisableDev(cfg->vi_dev_id);

    LOG_INFO("Video subsystem deinitialized.\n");
    return 0;
}
