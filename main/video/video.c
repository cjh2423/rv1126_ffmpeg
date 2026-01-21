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

#include <rk_mpi_mb.h>
#include <rk_mpi_sys.h>
#include <rk_mpi_vi.h>
#include <rk_mpi_venc.h>

// 编码线程与通道句柄。
static pthread_t g_venc_thread[2];
static int g_venc_thread_valid[2];
static int g_video_run = 0;
static MPP_CHN_S g_vi_chn;
static MPP_CHN_S g_venc_chn[2];
// 线程参数结构体。
typedef struct {
    const VideoConfig *cfg;
} VencThreadArgs;

// 从 VENC 拉取码流，可选写入文件。
static void *venc_get_stream_thread(void *arg) {
    VencThreadArgs *args = (VencThreadArgs *)arg;
    const VideoConfig *cfg = args->cfg;
    FILE *fp = NULL;

#if APP_Test_SAVE_FILE == 1
    if (cfg->output_path && cfg->output_path[0] != '\0') {
        fp = fopen(cfg->output_path, "wb");
        if (!fp) {
            LOG_ERROR("failed to open output file %s\n", cfg->output_path);
        }
    }
#endif

    VENC_STREAM_S stStream;
    memset(&stStream, 0, sizeof(stStream));
    stStream.pstPack = malloc(sizeof(VENC_PACK_S));
    if (!stStream.pstPack) {
        LOG_ERROR("failed to allocate VENC pack\n");
        if (fp)
            fclose(fp);
        return NULL;
    }

    while (g_video_run) {
        int ret = RK_MPI_VENC_GetStream(cfg->venc_chn_id, &stStream, 1000);
        if (ret == RK_SUCCESS) {
            void *data = RK_MPI_MB_Handle2VirAddr(stStream.pstPack->pMbBlk);
#if APP_Test_RTSP == 1
            if (data && stStream.pstPack->u32Len > 0) {
                // 推送码流到 RTSP。
                rkipc_rtsp_write_video_frame(cfg->rtsp_id, data, stStream.pstPack->u32Len,
                                             stStream.pstPack->u64PTS);
            }
#endif
#if APP_Test_SAVE_FILE == 1
            if (fp && data && stStream.pstPack->u32Len > 0) {
                fwrite(data, 1, stStream.pstPack->u32Len, fp);
                fflush(fp);
            }
#endif
            RK_MPI_VENC_ReleaseStream(cfg->venc_chn_id, &stStream);
        }
    }

    free(stStream.pstPack);
    if (fp)
        fclose(fp);

    free(args);
    return NULL;
}

// 配置并启用 VI 设备，并绑定到 Pipe。
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
            LOG_ERROR("RK_MPI_VI_SetDevAttr failed: %x\n", ret);
            return ret;
        }
    }

    ret = RK_MPI_VI_GetDevIsEnable(cfg->vi_dev_id);
    if (ret != RK_SUCCESS) {
        ret = RK_MPI_VI_EnableDev(cfg->vi_dev_id);
        if (ret != RK_SUCCESS) {
            LOG_ERROR("RK_MPI_VI_EnableDev failed: %x\n", ret);
            return ret;
        }
        bind_pipe.u32Num = 1;
        bind_pipe.PipeId[0] = cfg->vi_pipe_id;
        ret = RK_MPI_VI_SetDevBindPipe(cfg->vi_dev_id, &bind_pipe);
        if (ret != RK_SUCCESS) {
            LOG_ERROR("RK_MPI_VI_SetDevBindPipe failed: %x\n", ret);
            return ret;
        }
    }

    return 0;
}

// 配置并启用 VI 通道，输出 NV12 帧。
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
    chn_attr.u32Depth = 1;

    if (RK_MPI_VI_SetChnAttr(cfg->vi_pipe_id, cfg->vi_chn_id, &chn_attr) != RK_SUCCESS) {
        LOG_ERROR("RK_MPI_VI_SetChnAttr failed\n");
        return -1;
    }
    if (RK_MPI_VI_EnableChn(cfg->vi_pipe_id, cfg->vi_chn_id) != RK_SUCCESS) {
        LOG_ERROR("RK_MPI_VI_EnableChn failed\n");
        return -1;
    }

    return 0;
}

// 配置 VENC 通道，使用 CBR 码控。
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
        LOG_ERROR("RK_MPI_VENC_CreateChn failed\n");
        return -1;
    }

    memset(&recv_param, 0, sizeof(recv_param));
    recv_param.s32RecvPicNum = -1;
    if (RK_MPI_VENC_StartRecvFrame(cfg->venc_chn_id, &recv_param) != RK_SUCCESS) {
        LOG_ERROR("RK_MPI_VENC_StartRecvFrame failed\n");
        return -1;
    }

    g_venc_chn[cfg->rtsp_id].enModId = RK_ID_VENC;
    g_venc_chn[cfg->rtsp_id].s32DevId = 0;
    g_venc_chn[cfg->rtsp_id].s32ChnId = cfg->venc_chn_id;

    return 0;
}

// 最小 VI->VENC 采集编码链路初始化。
int rk_video_init(void) {
    const VideoConfig *cfg = app_video_config_get();
    const VideoConfig *cfg1 = app_video1_config_get();
    int ret;

    g_vi_chn.enModId = RK_ID_VI;
    g_vi_chn.s32DevId = cfg->vi_dev_id;
    g_vi_chn.s32ChnId = cfg->vi_chn_id;
    g_venc_thread_valid[0] = 0;
    g_venc_thread_valid[1] = 0;

    ret = vi_dev_init(cfg);
    if (ret)
        return ret;
    ret = vi_chn_init(cfg);
    if (ret)
        return ret;
    ret = venc_init(cfg);
    if (ret)
        return ret;

#if APP_Test_RTSP == 1
    // 设置 RTSP 输出码流类型，确保与编码格式一致。
    rk_param_set_string("video.0:output_data_type",
                        (cfg->codec == APP_VIDEO_CODEC_H265) ? "H.265" : "H.264");
    if (APP_Test_VENC1 == 1) {
        rk_param_set_string("video.1:output_data_type",
                            (cfg1->codec == APP_VIDEO_CODEC_H265) ? "H.265" : "H.264");
    }

    // 初始化 RTSP 服务。
    ret = rkipc_rtsp_init(cfg->rtsp_url, (APP_Test_VENC1 == 1) ? cfg1->rtsp_url : NULL, NULL);
    if (ret) {
        LOG_ERROR("rkipc_rtsp_init failed\n");
        return ret;
    }
#endif

    // 绑定 VI 到 VENC，直接推送帧给编码器。
    if (RK_MPI_SYS_Bind(&g_vi_chn, &g_venc_chn[0]) != RK_SUCCESS) {
        LOG_ERROR("RK_MPI_SYS_Bind VI->VENC failed\n");
        return -1;
    }

    g_video_run = 1;
    VencThreadArgs *args0 = malloc(sizeof(*args0));
    if (!args0) {
        LOG_ERROR("failed to alloc thread args\n");
        g_video_run = 0;
        return -1;
    }
    args0->cfg = cfg;
    if (pthread_create(&g_venc_thread[0], NULL, venc_get_stream_thread, args0) != 0) {
        LOG_ERROR("failed to create venc thread\n");
        g_video_run = 0;
        return -1;
    }
    g_venc_thread_valid[0] = 1;

    if (APP_Test_VENC1 == 1) {
        ret = venc_init(cfg1);
        if (ret)
            return ret;
        if (RK_MPI_SYS_Bind(&g_vi_chn, &g_venc_chn[cfg1->rtsp_id]) != RK_SUCCESS) {
            LOG_ERROR("RK_MPI_SYS_Bind VI->VENC1 failed\n");
            return -1;
        }
        VencThreadArgs *args1 = malloc(sizeof(*args1));
        if (!args1) {
            LOG_ERROR("failed to alloc thread args\n");
            g_video_run = 0;
            return -1;
        }
        args1->cfg = cfg1;
        if (pthread_create(&g_venc_thread[1], NULL, venc_get_stream_thread, args1) != 0) {
            LOG_ERROR("failed to create venc1 thread\n");
            g_video_run = 0;
            return -1;
        }
        g_venc_thread_valid[1] = 1;
    }

    return 0;
}

// 停止编码线程并释放 VI/VENC 资源。
int rk_video_deinit(void) {
    const VideoConfig *cfg = app_video_config_get();
    const VideoConfig *cfg1 = app_video1_config_get();

    g_video_run = 0;
    if (g_venc_thread_valid[0])
        pthread_join(g_venc_thread[0], NULL);
    if (g_venc_thread_valid[1])
        pthread_join(g_venc_thread[1], NULL);

    RK_MPI_SYS_UnBind(&g_vi_chn, &g_venc_chn[0]);
    if (APP_Test_VENC1 == 1)
        RK_MPI_SYS_UnBind(&g_vi_chn, &g_venc_chn[1]);
#if APP_Test_RTSP == 1
    rkipc_rtsp_deinit();
#endif
    RK_MPI_VENC_StopRecvFrame(cfg->venc_chn_id);
    RK_MPI_VENC_DestroyChn(cfg->venc_chn_id);
    if (APP_Test_VENC1 == 1) {
        RK_MPI_VENC_StopRecvFrame(cfg1->venc_chn_id);
        RK_MPI_VENC_DestroyChn(cfg1->venc_chn_id);
    }
    RK_MPI_VI_DisableChn(cfg->vi_pipe_id, cfg->vi_chn_id);
    RK_MPI_VI_DisableDev(cfg->vi_dev_id);

    return 0;
}
