/**
 * @file rga_utils.c
 * @brief RGA (Raster Graphic Acceleration) 工具封装模块实现
 * 
 * 基于 Rockchip im2d API 实现常用的 RGA 图像处理功能。
 */

#include "rga_utils.h"
#include "log.h"

#include <stdio.h>
#include <string.h>

/* RGA im2d API 头文件 */
#include "rga/im2d.h"
#include "rga/rga.h"

/* 日志标签 */
#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "rga_utils"

/* =========================================================================
 *                              内部辅助函数
 * ========================================================================= */

/**
 * @brief 将 RgaPixelFormat 转换为 RGA 内部格式
 */
static int rga_format_to_im2d(RgaPixelFormat fmt) {
    switch (fmt) {
        case RGA_FMT_RGBA_8888:     return RK_FORMAT_RGBA_8888;
        case RGA_FMT_RGBX_8888:     return RK_FORMAT_RGBX_8888;
        case RGA_FMT_RGB_888:       return RK_FORMAT_RGB_888;
        case RGA_FMT_BGRA_8888:     return RK_FORMAT_BGRA_8888;
        case RGA_FMT_BGR_888:       return RK_FORMAT_BGR_888;
        case RGA_FMT_RGB_565:       return RK_FORMAT_RGB_565;
        case RGA_FMT_YUV420SP:      return RK_FORMAT_YCbCr_420_SP;
        case RGA_FMT_YUV420SP_VU:   return RK_FORMAT_YCrCb_420_SP;
        case RGA_FMT_YUV420P:       return RK_FORMAT_YCbCr_420_P;
        case RGA_FMT_YUV422SP:      return RK_FORMAT_YCbCr_422_SP;
        default:
            LOG_WARN("Unknown RGA format: %d, using NV12\n", fmt);
            return RK_FORMAT_YCbCr_420_SP;
    }
}

/**
 * @brief 将 RgaImageInfo 转换为 rga_buffer_t
 * 
 * 根据提供的缓冲区信息（fd 或 虚拟地址）创建 im2d 使用的缓冲区结构。
 */
static rga_buffer_t rga_image_to_buffer(const RgaImageInfo *img) {
    rga_buffer_t buf;
    memset(&buf, 0, sizeof(buf));
    
    int w = img->width;
    int h = img->height;
    int ws = (img->wstride > 0) ? img->wstride : w;
    int hs = (img->hstride > 0) ? img->hstride : h;
    int fmt = rga_format_to_im2d(img->format);
    
    if (img->fd >= 0) {
        /* 使用 DMA-BUF fd */
        buf = wrapbuffer_fd_t(img->fd, w, h, ws, hs, fmt);
    } else if (img->vir_addr != NULL) {
        /* 使用虚拟地址 */
        buf = wrapbuffer_virtualaddr_t(img->vir_addr, w, h, ws, hs, fmt);
    } else {
        LOG_ERROR("Invalid RGA image: no fd or vir_addr\n");
    }
    
    return buf;
}

/**
 * @brief 将 RgaRect 转换为 im_rect
 */
static im_rect rga_rect_to_im(const RgaRect *rect) {
    im_rect r;
    if (rect) {
        r.x = rect->x;
        r.y = rect->y;
        r.width = rect->width;
        r.height = rect->height;
    } else {
        memset(&r, 0, sizeof(r));
    }
    return r;
}

/**
 * @brief 将 RgaRotateMode 转换为 im2d 旋转标志
 */
static int rga_rotate_to_im(RgaRotateMode rotation) {
    switch (rotation) {
        case RGA_ROTATE_90:  return IM_HAL_TRANSFORM_ROT_90;
        case RGA_ROTATE_180: return IM_HAL_TRANSFORM_ROT_180;
        case RGA_ROTATE_270: return IM_HAL_TRANSFORM_ROT_270;
        default:             return 0;
    }
}

/**
 * @brief 将 RgaFlipMode 转换为 im2d 翻转标志
 */
static int rga_flip_to_im(RgaFlipMode flip) {
    switch (flip) {
        case RGA_FLIP_H: return IM_HAL_TRANSFORM_FLIP_H;
        case RGA_FLIP_V: return IM_HAL_TRANSFORM_FLIP_V;
        default:         return 0;
    }
}

/**
 * @brief 检查 IM_STATUS 并打印日志
 */
static int check_status(IM_STATUS status, const char *op_name) {
    if (status == IM_STATUS_SUCCESS || status == IM_STATUS_NOERROR) {
        return 0;
    }
    LOG_ERROR("RGA %s failed, status: %d\n", op_name, status);
    return -1;
}

/* =========================================================================
 *                              接口实现
 * ========================================================================= */

int rga_utils_init(void) {
    /* im2d API 不需要显式初始化，驱动会自动加载 */
    LOG_INFO("RGA utils initialized\n");
    return 0;
}

void rga_utils_deinit(void) {
    LOG_INFO("RGA utils deinitialized\n");
}

int rga_utils_get_version(char *version_str, int len) {
    if (!version_str || len <= 0) {
        return -1;
    }
    
    /* 使用 querystring 获取 RGA 版本信息 */
    const char *info = querystring(RGA_VERSION);
    if (info) {
        strncpy(version_str, info, len - 1);
        version_str[len - 1] = '\0';
    } else {
        snprintf(version_str, len, "Unknown");
    }
    return 0;
}

int rga_utils_copy(const RgaImageInfo *src, const RgaImageInfo *dst) {
    if (!src || !dst) {
        LOG_ERROR("RGA copy: invalid parameter\n");
        return -1;
    }
    
    rga_buffer_t src_buf = rga_image_to_buffer(src);
    rga_buffer_t dst_buf = rga_image_to_buffer(dst);
    
    IM_STATUS status = imcopy_t(src_buf, dst_buf, 1);
    return check_status(status, "copy");
}

int rga_utils_resize(const RgaImageInfo *src, const RgaImageInfo *dst) {
    if (!src || !dst) {
        LOG_ERROR("RGA resize: invalid parameter\n");
        return -1;
    }
    
    rga_buffer_t src_buf = rga_image_to_buffer(src);
    rga_buffer_t dst_buf = rga_image_to_buffer(dst);
    
    IM_STATUS status = imresize_t(src_buf, dst_buf, 0, 0, INTER_LINEAR, 1);
    return check_status(status, "resize");
}

int rga_utils_crop(const RgaImageInfo *src, const RgaRect *src_rect, 
                   const RgaImageInfo *dst) {
    if (!src || !src_rect || !dst) {
        LOG_ERROR("RGA crop: invalid parameter\n");
        return -1;
    }
    
    rga_buffer_t src_buf = rga_image_to_buffer(src);
    rga_buffer_t dst_buf = rga_image_to_buffer(dst);
    im_rect rect = rga_rect_to_im(src_rect);
    
    IM_STATUS status = imcrop_t(src_buf, dst_buf, rect, 1);
    return check_status(status, "crop");
}

int rga_utils_crop_and_resize(const RgaImageInfo *src, const RgaRect *src_rect,
                               const RgaImageInfo *dst, const RgaRect *dst_rect) {
    if (!src || !dst) {
        LOG_ERROR("RGA crop_and_resize: invalid parameter\n");
        return -1;
    }
    
    rga_buffer_t src_buf = rga_image_to_buffer(src);
    rga_buffer_t dst_buf = rga_image_to_buffer(dst);
    rga_buffer_t pat_buf;  /* 不使用 */
    memset(&pat_buf, 0, sizeof(pat_buf));
    
    im_rect srect = src_rect ? rga_rect_to_im(src_rect) : (im_rect){0, 0, src->width, src->height};
    im_rect drect = dst_rect ? rga_rect_to_im(dst_rect) : (im_rect){0, 0, dst->width, dst->height};
    im_rect prect = {0};
    
    /* 使用 improcess 进行裁剪+缩放 */
    IM_STATUS status = improcess(src_buf, dst_buf, pat_buf, srect, drect, prect, 0);
    return check_status(status, "crop_and_resize");
}

int rga_utils_rotate(const RgaImageInfo *src, const RgaImageInfo *dst,
                      RgaRotateMode rotation) {
    if (!src || !dst) {
        LOG_ERROR("RGA rotate: invalid parameter\n");
        return -1;
    }
    
    if (rotation == RGA_ROTATE_NONE) {
        return rga_utils_copy(src, dst);
    }
    
    rga_buffer_t src_buf = rga_image_to_buffer(src);
    rga_buffer_t dst_buf = rga_image_to_buffer(dst);
    int rot = rga_rotate_to_im(rotation);
    
    IM_STATUS status = imrotate_t(src_buf, dst_buf, rot, 1);
    return check_status(status, "rotate");
}

int rga_utils_flip(const RgaImageInfo *src, const RgaImageInfo *dst,
                    RgaFlipMode flip) {
    if (!src || !dst) {
        LOG_ERROR("RGA flip: invalid parameter\n");
        return -1;
    }
    
    if (flip == RGA_FLIP_NONE) {
        return rga_utils_copy(src, dst);
    }
    
    rga_buffer_t src_buf = rga_image_to_buffer(src);
    rga_buffer_t dst_buf = rga_image_to_buffer(dst);
    int mode = rga_flip_to_im(flip);
    
    IM_STATUS status = imflip_t(src_buf, dst_buf, mode, 1);
    return check_status(status, "flip");
}

int rga_utils_cvtcolor(const RgaImageInfo *src, const RgaImageInfo *dst) {
    if (!src || !dst) {
        LOG_ERROR("RGA cvtcolor: invalid parameter\n");
        return -1;
    }
    
    rga_buffer_t src_buf = rga_image_to_buffer(src);
    rga_buffer_t dst_buf = rga_image_to_buffer(dst);
    
    int sfmt = rga_format_to_im2d(src->format);
    int dfmt = rga_format_to_im2d(dst->format);
    
    IM_STATUS status = imcvtcolor_t(src_buf, dst_buf, sfmt, dfmt, 
                                     IM_COLOR_SPACE_DEFAULT, 1);
    return check_status(status, "cvtcolor");
}

int rga_utils_fill(const RgaImageInfo *dst, const RgaRect *rect, uint32_t color) {
    if (!dst) {
        LOG_ERROR("RGA fill: invalid parameter\n");
        return -1;
    }
    
    rga_buffer_t dst_buf = rga_image_to_buffer(dst);
    im_rect fill_rect;
    
    if (rect) {
        fill_rect = rga_rect_to_im(rect);
    } else {
        fill_rect.x = 0;
        fill_rect.y = 0;
        fill_rect.width = dst->width;
        fill_rect.height = dst->height;
    }
    
    IM_STATUS status = imfill_t(dst_buf, fill_rect, (int)color, 1);
    return check_status(status, "fill");
}

int rga_utils_blend(const RgaImageInfo *fg, const RgaRect *fg_rect,
                     const RgaImageInfo *bg, const RgaRect *bg_rect,
                     uint8_t global_alpha) {
    if (!fg || !bg) {
        LOG_ERROR("RGA blend: invalid parameter\n");
        return -1;
    }
    
    rga_buffer_t fg_buf = rga_image_to_buffer(fg);
    rga_buffer_t bg_buf = rga_image_to_buffer(bg);
    rga_buffer_t dst_buf;
    memset(&dst_buf, 0, sizeof(dst_buf));
    
    /* 设置全局透明度 */
    fg_buf.global_alpha = global_alpha;
    
    (void)fg_rect;  /* 简化实现，忽略区域参数 */
    (void)bg_rect;
    
    IM_STATUS status = imblend_t(fg_buf, dst_buf, bg_buf, 
                                  IM_ALPHA_BLEND_SRC_OVER, 1);
    return check_status(status, "blend");
}

int rga_utils_process(const RgaImageInfo *src, const RgaRect *src_rect,
                       const RgaImageInfo *dst, const RgaRect *dst_rect,
                       RgaRotateMode rotation, RgaFlipMode flip) {
    if (!src || !dst) {
        LOG_ERROR("RGA process: invalid parameter\n");
        return -1;
    }
    
    rga_buffer_t src_buf = rga_image_to_buffer(src);
    rga_buffer_t dst_buf = rga_image_to_buffer(dst);
    rga_buffer_t pat_buf;
    memset(&pat_buf, 0, sizeof(pat_buf));
    
    im_rect srect = src_rect ? rga_rect_to_im(src_rect) : (im_rect){0, 0, src->width, src->height};
    im_rect drect = dst_rect ? rga_rect_to_im(dst_rect) : (im_rect){0, 0, dst->width, dst->height};
    im_rect prect = {0};
    
    /* 组合 usage 标志 */
    int usage = 0;
    if (rotation != RGA_ROTATE_NONE) {
        usage |= rga_rotate_to_im(rotation);
    }
    if (flip != RGA_FLIP_NONE) {
        usage |= rga_flip_to_im(flip);
    }
    
    IM_STATUS status = improcess(src_buf, dst_buf, pat_buf, srect, drect, prect, usage);
    return check_status(status, "process");
}
