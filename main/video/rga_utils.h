/**
 * @file rga_utils.h
 * @brief RGA (Raster Graphic Acceleration) 工具封装模块
 * 
 * 对 Rockchip RGA 的 im2d 高级 API 进行封装，提供常用的图像处理功能，包括：
 * - 图像缩放 (Resize)
 * - 图像裁剪 (Crop)
 * - 图像旋转 (Rotate)
 * - 图像翻转 (Flip)
 * - 格式转换 (Color Convert)
 * - 图像拷贝 (Copy)
 * - 图像填充 (Fill)
 * 
 * RGA 是 Rockchip 芯片上的硬件 2D 图形加速器，效率远高于 CPU 处理。
 */

#ifndef __RGA_UTILS_H__
#define __RGA_UTILS_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 *                              常量定义
 * ========================================================================= */

/**
 * @brief RGA 支持的像素格式 (与 RK_FORMAT_XXX 对应)
 */
typedef enum {
    RGA_FMT_RGBA_8888 = 0,   /**< RGBA 32位 */
    RGA_FMT_RGBX_8888,       /**< RGBX 32位 */
    RGA_FMT_RGB_888,         /**< RGB 24位 */
    RGA_FMT_BGRA_8888,       /**< BGRA 32位 */
    RGA_FMT_BGR_888,         /**< BGR 24位 */
    RGA_FMT_RGB_565,         /**< RGB 16位 */
    RGA_FMT_YUV420SP,        /**< NV12 (YUV420 Semi-Planar) */
    RGA_FMT_YUV420SP_VU,     /**< NV21 (YUV420 Semi-Planar, VU顺序) */
    RGA_FMT_YUV420P,         /**< YUV420 Planar */
    RGA_FMT_YUV422SP,        /**< NV16 (YUV422 Semi-Planar) */
    RGA_FMT_UNKNOWN,
} RgaPixelFormat;

/**
 * @brief RGA 旋转模式
 */
typedef enum {
    RGA_ROTATE_NONE = 0,     /**< 不旋转 */
    RGA_ROTATE_90,           /**< 顺时针旋转 90 度 */
    RGA_ROTATE_180,          /**< 旋转 180 度 */
    RGA_ROTATE_270,          /**< 顺时针旋转 270 度 */
} RgaRotateMode;

/**
 * @brief RGA 翻转模式
 */
typedef enum {
    RGA_FLIP_NONE = 0,       /**< 不翻转 */
    RGA_FLIP_H,              /**< 水平翻转 */
    RGA_FLIP_V,              /**< 垂直翻转 */
} RgaFlipMode;

/* =========================================================================
 *                              数据结构
 * ========================================================================= */

/**
 * @brief RGA 图像缓冲区信息
 */
typedef struct {
    void *vir_addr;          /**< 虚拟地址 (用户空间) */
    int fd;                  /**< DMA-BUF 文件描述符 (-1 表示未使用) */
    int width;               /**< 图像宽度 (像素) */
    int height;              /**< 图像高度 (像素) */
    int wstride;             /**< 行跨度 (像素, 0 表示与 width 相同) */
    int hstride;             /**< 列跨度 (像素, 0 表示与 height 相同) */
    RgaPixelFormat format;   /**< 像素格式 */
} RgaImageInfo;

/**
 * @brief RGA 矩形区域
 */
typedef struct {
    int x;                   /**< 左上角 X 坐标 */
    int y;                   /**< 左上角 Y 坐标 */
    int width;               /**< 宽度 */
    int height;              /**< 高度 */
} RgaRect;

/* =========================================================================
 *                              接口函数
 * ========================================================================= */

/**
 * @brief 初始化 RGA 模块
 * 
 * @return 0 成功, 其他失败
 */
int rga_utils_init(void);

/**
 * @brief 反初始化 RGA 模块
 */
void rga_utils_deinit(void);

/**
 * @brief 获取 RGA 版本信息
 * 
 * @param version_str 输出版本字符串的缓冲区
 * @param len 缓冲区长度
 * @return 0 成功
 */
int rga_utils_get_version(char *version_str, int len);

/**
 * @brief 图像拷贝
 * 
 * 将源图像完整拷贝到目标图像。源和目标尺寸、格式必须相同。
 * 
 * @param src 源图像信息
 * @param dst 目标图像信息
 * @return 0 成功, 其他失败
 */
int rga_utils_copy(const RgaImageInfo *src, const RgaImageInfo *dst);

/**
 * @brief 图像缩放
 * 
 * 将源图像缩放到目标图像的尺寸。
 * 
 * @param src 源图像信息
 * @param dst 目标图像信息
 * @return 0 成功, 其他失败
 */
int rga_utils_resize(const RgaImageInfo *src, const RgaImageInfo *dst);

/**
 * @brief 图像裁剪
 * 
 * 从源图像中裁剪指定区域到目标图像。
 * 
 * @param src 源图像信息
 * @param src_rect 裁剪区域
 * @param dst 目标图像信息
 * @return 0 成功, 其他失败
 */
int rga_utils_crop(const RgaImageInfo *src, const RgaRect *src_rect, 
                   const RgaImageInfo *dst);

/**
 * @brief 图像裁剪并缩放
 * 
 * 从源图像中裁剪指定区域，并缩放到目标图像。
 * 
 * @param src 源图像信息
 * @param src_rect 裁剪区域 (NULL 表示使用整个源图像)
 * @param dst 目标图像信息
 * @param dst_rect 目标区域 (NULL 表示填充整个目标图像)
 * @return 0 成功, 其他失败
 */
int rga_utils_crop_and_resize(const RgaImageInfo *src, const RgaRect *src_rect,
                               const RgaImageInfo *dst, const RgaRect *dst_rect);

/**
 * @brief 图像旋转
 * 
 * 将源图像旋转后输出到目标图像。
 * 注意：旋转 90/270 度时，目标图像的宽高需要交换。
 * 
 * @param src 源图像信息
 * @param dst 目标图像信息
 * @param rotation 旋转模式
 * @return 0 成功, 其他失败
 */
int rga_utils_rotate(const RgaImageInfo *src, const RgaImageInfo *dst,
                      RgaRotateMode rotation);

/**
 * @brief 图像翻转
 * 
 * 将源图像翻转后输出到目标图像。
 * 
 * @param src 源图像信息
 * @param dst 目标图像信息
 * @param flip 翻转模式
 * @return 0 成功, 其他失败
 */
int rga_utils_flip(const RgaImageInfo *src, const RgaImageInfo *dst,
                    RgaFlipMode flip);

/**
 * @brief 格式转换
 * 
 * 将源图像转换为目标格式。例如 NV12 -> RGB888。
 * 
 * @param src 源图像信息
 * @param dst 目标图像信息 (format 字段指定目标格式)
 * @return 0 成功, 其他失败
 */
int rga_utils_cvtcolor(const RgaImageInfo *src, const RgaImageInfo *dst);

/**
 * @brief 矩形填充
 * 
 * 用指定颜色填充目标图像的指定区域。
 * 
 * @param dst 目标图像信息
 * @param rect 填充区域 (NULL 表示填充整个图像)
 * @param color 填充颜色 (ARGB8888 格式)
 * @return 0 成功, 其他失败
 */
int rga_utils_fill(const RgaImageInfo *dst, const RgaRect *rect, uint32_t color);

/**
 * @brief 图像混合 (Alpha Blend)
 * 
 * 将前景图像叠加到背景图像上。
 * 
 * @param fg 前景图像信息
 * @param fg_rect 前景区域 (NULL 表示整个前景)
 * @param bg 背景图像信息 (也是输出)
 * @param bg_rect 背景区域 (NULL 表示整个背景)
 * @param global_alpha 全局透明度 (0-255, 255 表示完全不透明)
 * @return 0 成功, 其他失败
 */
int rga_utils_blend(const RgaImageInfo *fg, const RgaRect *fg_rect,
                     const RgaImageInfo *bg, const RgaRect *bg_rect,
                     uint8_t global_alpha);

/**
 * @brief 通用图像处理 (支持同时进行裁剪、缩放、旋转)
 * 
 * @param src 源图像信息
 * @param src_rect 源裁剪区域 (NULL 表示整个图像)
 * @param dst 目标图像信息
 * @param dst_rect 目标区域 (NULL 表示整个图像)
 * @param rotation 旋转模式
 * @param flip 翻转模式
 * @return 0 成功, 其他失败
 */
int rga_utils_process(const RgaImageInfo *src, const RgaRect *src_rect,
                       const RgaImageInfo *dst, const RgaRect *dst_rect,
                       RgaRotateMode rotation, RgaFlipMode flip);

#ifdef __cplusplus
}
#endif

#endif /* __RGA_UTILS_H__ */
