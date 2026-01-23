# 🖼️ RGA 硬件图形加速模块

本项目集成了 Rockchip RGA (Raster Graphic Acceleration) 2D 图形加速硬件的封装库，用于高效处理图像的缩放、裁剪、旋转、格式转换和 OSD 叠加，释放 CPU 算力。

---

## 🚀 模块简介

- **源文件**: `main/video/rga_utils.c`, `main/video/rga_utils.h`
- **底层依赖**: `librga.so`, `im2d_api` (Rockchip 官方高级封装接口)
- **主要优势**:
    - **零拷贝**: 支持 DMA-BUF (fd) 方式直接操作硬件缓冲区。
    - **高性能**: 独立于 CPU 的硬件流水线，支持多任务并发。
    - **易用性**: 封装了统一的 `RgaImageInfo` 结构体和简洁的 C 接口。

---

## 🛠️ 支持的功能

封装库 (`rga_utils`) 提供了以下核心 API：

| 功能 | API 函数 | 说明 |
|------|----------|------|
| **初始化** | `rga_utils_init` | 初始化 RGA 上下文 |
| **反初始化** | `rga_utils_deinit` | 释放资源 |
| **图像拷贝** | `rga_utils_copy` | 快速内存拷贝 (DMA) |
| **图像缩放** | `rga_utils_resize` | 硬件双线性插值缩放 |
| **图像裁剪** | `rga_utils_crop` | ROI 区域提取 |
| **裁减缩放** | `rga_utils_crop_and_resize` | 同时进行裁剪和缩放 |
| **图像旋转** | `rga_utils_rotate` | 90°/180°/270° 旋转 |
| **图像翻转** | `rga_utils_flip` | 水平/垂直翻转 |
| **格式转换** | `rga_utils_cvtcolor` | CSC (Color Space Conversion) 转换，如 NV12 -> RGB888 |
| **图像混合** | `rga_utils_blend` | Alpha 混合与叠加 |
| **矩形填充** | `rga_utils_fill` | 单色填充区域 (可用于遮挡或背景绘制) |

---

## 💻 使用示例

### 1. 结构体定义

所有操作基于 `RgaImageInfo` 结构体，支持虚拟地址 (`vir_addr`) 和 DMA 文件句柄 (`fd`) 两种模式：

```c
typedef struct {
    void *vir_addr;          // 虚拟地址 (malloc/mmap)
    int fd;                  // DMA-BUF fd (MPI/V4L2 buffer)
    int width;               // 宽
    int height;              // 高
    RgaPixelFormat format;   // 像素格式 (如 RGA_FMT_YUV420SP)
    // ...
} RgaImageInfo;
```

### 2. 初始化

在视频子系统初始化时调用：

```c
// 在 video.c 的 rk_video_init 中
rga_utils_init();
```

### 3. 调用示例：YUV缩放 (Resize)

```c
#include "rga_utils.h"

// 定义源 (1080P NV12)
RgaImageInfo src = {0};
src.fd = src_mb_fd; // 从 MPI 获取的 DMA fd
src.width = 1920;
src.height = 1080;
src.format = RGA_FMT_YUV420SP;

// 定义目标 (720P NV12)
RgaImageInfo dst = {0};
dst.fd = dst_mb_fd; // 预分配的目标 buffer
dst.width = 1280;
dst.height = 720;
dst.format = RGA_FMT_YUV420SP;

// 执行硬件缩放
int ret = rga_utils_resize(&src, &dst);
if (ret != 0) {
    LOG_ERROR("RGA resize failed!\n");
}
```

### 4. 调用示例：格式转换 (NV12 转 RGB)

常用于 AI 推理前的预处理。

```c
// 定义目标为 RGB888
dst.format = RGA_FMT_RGB_888;

// 执行转换
rga_utils_cvtcolor(&src, &dst);
```

---

## ⚠️ 注意事项

1. **对齐限制**: 虽然 RGA 支持任意分辨率，但为了最佳性能，建议宽高最好 4 字节或 16 字节对齐。
2. **虚拟地址支持**: 如果使用 `vir_addr`，必须确保内存是物理连续的或者驱动支持 IOMMU（RV1126 支持 IOMMU，普通 malloc 内存通常可用，但效率略低于 DMA fd）。
3. **错误处理**: 请务必检查 API 返回值，非 0 表示失败，通常会在终端打印具体的 im2d 错误码。
