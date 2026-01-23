/**
 * @file video_osd.c
 * @brief 视频 OSD (On-Screen Display) 集成模块实现
 * 
 * 该模块实现 OSD 组件与 VENC 的 Region 功能的对接：
 * - 使用 RK_MPI_RGN_* 系列接口创建和管理叠加区域
 * - 将 OSD 组件生成的 ARGB8888 位图设置到 Region 上
 * - 支持实时时间戳更新
 */

#include "video_osd.h"
#include "config.h"
#include "log.h"
#include "osd.h"

#include <string.h>

#include <rk_mpi_rgn.h>
#include <rk_mpi_sys.h>

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "video_osd"

/* OSD 相关配置 */
#define OSD_MAX_REGION_NUM  8           /* 最大 Region 数量 */
#define OSD_RGN_HANDLE_BASE 0           /* Region Handle 起始值 */

/* 全局状态 */
static int g_venc_chn_id = 0;           /* 绑定的 VENC 通道 */
static int g_osd_initialized = 0;       /* 初始化标志 */

/* =========================================================================
 *                              OSD 回调函数实现
 * ========================================================================= */

/**
 * @brief 创建 OSD Bitmap 区域 (用于时间戳、文字、Logo 等)
 */
static int osd_bmp_create_callback(int osd_id, osd_data_s *data) {
    if (!data || osd_id >= OSD_MAX_REGION_NUM) {
        LOG_ERROR("Invalid osd_bmp_create params\n");
        return -1;
    }
    
    LOG_INFO("Creating OSD region %d, size=%dx%d, pos=(%d,%d)\n",
             osd_id, data->width, data->height, data->origin_x, data->origin_y);
    
    RGN_HANDLE handle = OSD_RGN_HANDLE_BASE + osd_id;
    RGN_ATTR_S rgn_attr;
    RGN_CHN_ATTR_S rgn_chn_attr;
    MPP_CHN_S mpp_chn;
    int ret;
    
    /* 1. 创建 Region */
    memset(&rgn_attr, 0, sizeof(rgn_attr));
    rgn_attr.enType = OVERLAY_RGN;
    rgn_attr.unAttr.stOverlay.enPixelFmt = RK_FMT_BGRA8888;
    rgn_attr.unAttr.stOverlay.stSize.u32Width = data->width;
    rgn_attr.unAttr.stOverlay.stSize.u32Height = data->height;
    
    ret = RK_MPI_RGN_Create(handle, &rgn_attr);
    if (ret != RK_SUCCESS) {
        LOG_ERROR("RK_MPI_RGN_Create(%d) failed: 0x%x\n", handle, ret);
        return -1;
    }
    
    /* 2. 设置 Bitmap 数据 */
    if (data->buffer && data->size > 0) {
        BITMAP_S bitmap;
        memset(&bitmap, 0, sizeof(bitmap));
        bitmap.enPixelFormat = RK_FMT_BGRA8888;
        bitmap.u32Width = data->width;
        bitmap.u32Height = data->height;
        bitmap.pData = data->buffer;
        
        ret = RK_MPI_RGN_SetBitMap(handle, &bitmap);
        if (ret != RK_SUCCESS) {
            LOG_WARN("RK_MPI_RGN_SetBitMap(%d) failed: 0x%x\n", handle, ret);
        }
    }
    
    /* 3. 绑定到 VENC 通道 */
    memset(&mpp_chn, 0, sizeof(mpp_chn));
    mpp_chn.enModId = RK_ID_VENC;
    mpp_chn.s32DevId = 0;
    mpp_chn.s32ChnId = g_venc_chn_id;
    
    memset(&rgn_chn_attr, 0, sizeof(rgn_chn_attr));
    rgn_chn_attr.bShow = data->enable ? RK_TRUE : RK_FALSE;
    rgn_chn_attr.enType = OVERLAY_RGN;
    rgn_chn_attr.unChnAttr.stOverlayChn.stPoint.s32X = data->origin_x;
    rgn_chn_attr.unChnAttr.stOverlayChn.stPoint.s32Y = data->origin_y;
    rgn_chn_attr.unChnAttr.stOverlayChn.u32BgAlpha = 0;      /* 背景透明 */
    rgn_chn_attr.unChnAttr.stOverlayChn.u32FgAlpha = 255;    /* 前景完全不透明 */
    rgn_chn_attr.unChnAttr.stOverlayChn.u32Layer = osd_id;
    
    ret = RK_MPI_RGN_AttachToChn(handle, &mpp_chn, &rgn_chn_attr);
    if (ret != RK_SUCCESS) {
        LOG_ERROR("RK_MPI_RGN_AttachToChn(%d) to VENC[%d] failed: 0x%x\n", 
                  handle, g_venc_chn_id, ret);
        RK_MPI_RGN_Destroy(handle);
        return -1;
    }
    
    LOG_INFO("OSD region %d created and attached to VENC[%d]\n", osd_id, g_venc_chn_id);
    return 0;
}

/**
 * @brief 销毁 OSD Bitmap 区域
 */
static int osd_bmp_destroy_callback(int osd_id) {
    if (osd_id >= OSD_MAX_REGION_NUM) {
        return -1;
    }
    
    RGN_HANDLE handle = OSD_RGN_HANDLE_BASE + osd_id;
    MPP_CHN_S mpp_chn;
    
    memset(&mpp_chn, 0, sizeof(mpp_chn));
    mpp_chn.enModId = RK_ID_VENC;
    mpp_chn.s32DevId = 0;
    mpp_chn.s32ChnId = g_venc_chn_id;
    
    RK_MPI_RGN_DetachFromChn(handle, &mpp_chn);
    RK_MPI_RGN_Destroy(handle);
    
    LOG_INFO("OSD region %d destroyed\n", osd_id);
    return 0;
}

/**
 * @brief 更新 OSD Bitmap 数据 (用于时间戳实时刷新)
 */
static int osd_bmp_change_callback(int osd_id, osd_data_s *data) {
    if (!data || osd_id >= OSD_MAX_REGION_NUM) {
        return -1;
    }
    
    RGN_HANDLE handle = OSD_RGN_HANDLE_BASE + osd_id;
    
    if (data->buffer && data->size > 0) {
        BITMAP_S bitmap;
        memset(&bitmap, 0, sizeof(bitmap));
        bitmap.enPixelFormat = RK_FMT_BGRA8888;
        bitmap.u32Width = data->width;
        bitmap.u32Height = data->height;
        bitmap.pData = data->buffer;
        
        int ret = RK_MPI_RGN_SetBitMap(handle, &bitmap);
        if (ret != RK_SUCCESS) {
            LOG_WARN("RK_MPI_RGN_SetBitMap(%d) update failed: 0x%x\n", handle, ret);
            return -1;
        }
    }
    
    return 0;
}

/**
 * @brief 创建遮挡区域 (Cover)
 */
static int osd_cover_create_callback(int osd_id, osd_data_s *data) {
    if (!data || osd_id >= OSD_MAX_REGION_NUM) {
        return -1;
    }
    
    LOG_INFO("Creating Cover region %d\n", osd_id);
    
    RGN_HANDLE handle = OSD_RGN_HANDLE_BASE + osd_id;
    RGN_ATTR_S rgn_attr;
    RGN_CHN_ATTR_S rgn_chn_attr;
    MPP_CHN_S mpp_chn;
    int ret;
    
    memset(&rgn_attr, 0, sizeof(rgn_attr));
    rgn_attr.enType = COVER_RGN;
    
    ret = RK_MPI_RGN_Create(handle, &rgn_attr);
    if (ret != RK_SUCCESS) {
        LOG_ERROR("RK_MPI_RGN_Create COVER(%d) failed: 0x%x\n", handle, ret);
        return -1;
    }
    
    memset(&mpp_chn, 0, sizeof(mpp_chn));
    mpp_chn.enModId = RK_ID_VENC;
    mpp_chn.s32DevId = 0;
    mpp_chn.s32ChnId = g_venc_chn_id;
    
    memset(&rgn_chn_attr, 0, sizeof(rgn_chn_attr));
    rgn_chn_attr.bShow = data->enable ? RK_TRUE : RK_FALSE;
    rgn_chn_attr.enType = COVER_RGN;
    rgn_chn_attr.unChnAttr.stCoverChn.stRect.s32X = data->origin_x;
    rgn_chn_attr.unChnAttr.stCoverChn.stRect.s32Y = data->origin_y;
    rgn_chn_attr.unChnAttr.stCoverChn.stRect.u32Width = data->width;
    rgn_chn_attr.unChnAttr.stCoverChn.stRect.u32Height = data->height;
    rgn_chn_attr.unChnAttr.stCoverChn.u32Color = 0x000000;  /* 黑色遮挡 */
    rgn_chn_attr.unChnAttr.stCoverChn.u32Layer = osd_id;
    
    ret = RK_MPI_RGN_AttachToChn(handle, &mpp_chn, &rgn_chn_attr);
    if (ret != RK_SUCCESS) {
        LOG_ERROR("RK_MPI_RGN_AttachToChn COVER(%d) failed: 0x%x\n", handle, ret);
        RK_MPI_RGN_Destroy(handle);
        return -1;
    }
    
    return 0;
}

/**
 * @brief 销毁遮挡区域
 */
static int osd_cover_destroy_callback(int osd_id) {
    return osd_bmp_destroy_callback(osd_id);  /* 复用销毁逻辑 */
}

/**
 * @brief 创建马赛克区域
 */
static int osd_mosaic_create_callback(int osd_id, osd_data_s *data) {
    if (!data || osd_id >= OSD_MAX_REGION_NUM) {
        return -1;
    }
    
    LOG_INFO("Creating Mosaic region %d\n", osd_id);
    
    RGN_HANDLE handle = OSD_RGN_HANDLE_BASE + osd_id;
    RGN_ATTR_S rgn_attr;
    RGN_CHN_ATTR_S rgn_chn_attr;
    MPP_CHN_S mpp_chn;
    int ret;
    
    memset(&rgn_attr, 0, sizeof(rgn_attr));
    rgn_attr.enType = MOSAIC_RGN;
    
    ret = RK_MPI_RGN_Create(handle, &rgn_attr);
    if (ret != RK_SUCCESS) {
        LOG_ERROR("RK_MPI_RGN_Create MOSAIC(%d) failed: 0x%x\n", handle, ret);
        return -1;
    }
    
    memset(&mpp_chn, 0, sizeof(mpp_chn));
    mpp_chn.enModId = RK_ID_VENC;
    mpp_chn.s32DevId = 0;
    mpp_chn.s32ChnId = g_venc_chn_id;
    
    memset(&rgn_chn_attr, 0, sizeof(rgn_chn_attr));
    rgn_chn_attr.bShow = data->enable ? RK_TRUE : RK_FALSE;
    rgn_chn_attr.enType = MOSAIC_RGN;
    rgn_chn_attr.unChnAttr.stMosaicChn.stRect.s32X = data->origin_x;
    rgn_chn_attr.unChnAttr.stMosaicChn.stRect.s32Y = data->origin_y;
    rgn_chn_attr.unChnAttr.stMosaicChn.stRect.u32Width = data->width;
    rgn_chn_attr.unChnAttr.stMosaicChn.stRect.u32Height = data->height;
    rgn_chn_attr.unChnAttr.stMosaicChn.enBlkSize = MOSAIC_BLK_SIZE_16;
    rgn_chn_attr.unChnAttr.stMosaicChn.u32Layer = osd_id;
    
    ret = RK_MPI_RGN_AttachToChn(handle, &mpp_chn, &rgn_chn_attr);
    if (ret != RK_SUCCESS) {
        LOG_ERROR("RK_MPI_RGN_AttachToChn MOSAIC(%d) failed: 0x%x\n", handle, ret);
        RK_MPI_RGN_Destroy(handle);
        return -1;
    }
    
    return 0;
}

/**
 * @brief 销毁马赛克区域
 */
static int osd_mosaic_destroy_callback(int osd_id) {
    return osd_bmp_destroy_callback(osd_id);
}

/* =========================================================================
 *                              外部接口实现
 * ========================================================================= */

int video_osd_init(int venc_chn_id) {
    if (g_osd_initialized) {
        LOG_WARN("OSD already initialized\n");
        return 0;
    }
    
    LOG_INFO("Initializing video OSD for VENC[%d]\n", venc_chn_id);
    
    g_venc_chn_id = venc_chn_id;
    
    /* 注册 OSD 回调函数 */
    rk_osd_bmp_create_callback_register(osd_bmp_create_callback);
    rk_osd_bmp_destroy_callback_register(osd_bmp_destroy_callback);
    rk_osd_bmp_change_callback_register(osd_bmp_change_callback);
    rk_osd_cover_create_callback_register(osd_cover_create_callback);
    rk_osd_cover_destroy_callback_register(osd_cover_destroy_callback);
    rk_osd_mosaic_create_callback_register(osd_mosaic_create_callback);
    rk_osd_mosaic_destroy_callback_register(osd_mosaic_destroy_callback);
    
    /* 初始化 OSD 模块 (会读取 INI 配置并创建 OSD) */
    int ret = rk_osd_init();
    if (ret != 0) {
        LOG_ERROR("rk_osd_init failed: %d\n", ret);
        return ret;
    }
    
    g_osd_initialized = 1;
    LOG_INFO("Video OSD initialized successfully\n");
    
    return 0;
}

int video_osd_deinit(void) {
    if (!g_osd_initialized) {
        return 0;
    }
    
    LOG_INFO("Deinitializing video OSD\n");
    
    rk_osd_deinit();
    
    g_osd_initialized = 0;
    return 0;
}
