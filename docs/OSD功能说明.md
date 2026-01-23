# 📟 OSD (On-Screen Display) 功能说明

本项目集成了 OSD 功能，支持在编码前的视频帧上叠加实时时间戳、自定义文字、马赛克和图片Logo。OSD 模块基于 Rockchip RGN (Region) 硬件机制实现，效率极高。

---

## 🚀 模块简介

- **源文件**: `main/video/video_osd.c`, `common/osd/osd.c`
- **底层依赖**: `Rockchip MPI RGN` (硬件叠加), `FreeType` (字体渲染)
- **核心功能**:
    - **实时时间戳**: 显示日期、时间及星期，秒级自动刷新。
    - **文本叠加**: 支持中英文自定义文字显示。
    - **图片叠加**: 支持 BMP 格式图片 (如 Logo) 叠加。
    - **马赛克/隐私遮挡**: 支持矩形区域的马赛克处理或纯色遮挡。

---

## 🛠️ 配置说明 (INI)

OSD 功能完全通过 INI 配置文件驱动。请编辑板端的 `/userdata/rkipc.ini` (或项目提供的 `rkipc-demo.ini`) 进行配置。

### 1. 通用配置 `[osd.common]`

| 配置项 | 说明 | 示例 |
| :--- | :--- | :--- |
| `is_presistent_text` | 文本是否常驻 | 1 |
| `font_path` | 字体文件路径 | `/oem/usr/share/simsun_cn.ttf` |
| `font_size` | 字体大小 (像素) | 32 |
| `font_color` | 字体颜色 (RGB Hex) | `ffffff` (白色) |
| `normalized_screen_width` | 归一化坐标系宽度 | 704 (不用改) |
| `normalized_screen_height` | 归一化坐标系高度 | 480 (不用改) |

### 2. Region 配置 `[osd.N]`

支持最多 8 个区域 (Region)，`N` 为 0-7。

#### 🕒 时间戳配置 (Type: dateTime)

```ini
[osd.0]
type = dateTime
enabled = 1
position_x = 16            ; X 坐标
position_y = 16            ; Y 坐标
display_week_enabled = 1   ; 显示星期
date_style = YYYY-MM-DD    ; 日期格式
time_style = 24hour        ; 时间格式
```

#### 📝 文字配置 (Type: character / channelName)

```ini
[osd.1]
type = channelName
enabled = 1
position_x = 16
position_y = 450
display_text = RV1126 Demo Cam
```

#### 🖼️ 图片配置 (Type: image)

```ini
[osd.2]
type = image
enabled = 1
position_x = 600
position_y = 16
image_path = /oem/usr/share/logo.bmp
```

#### ⬛ 隐私遮挡 (Type: privacyMask)

```ini
[osd.3]
type = privacyMask
enabled = 1
position_x = 100
position_y = 100
width = 200
height = 200
style = mosaic             ; mosaic (马赛克) 或 cover (纯色遮挡)
```

---

## 💻 开发指南

### 代码集成

OSD 模块已封装在 `video_osd.h` 中，初始化极为简单：

```c
#include "video_osd.h"

// 收集所有活跃的 VENC 通道 ID
int chns[] = {0, 1}; // 例如主流和子流
int count = 2;

// 初始化 OSD 模块 (同时叠加到多个通道)
video_osd_init(chns, count);
```

### 部署要求

1. **字体文件**: 确保板端存在字体文件 (默认为 `/oem/usr/share/simsun_cn.ttf`)。如果路径不同，请修改 INI 中的 `font_path`。
2. **FreeType**: 依赖 `libfreetype.so`，编译时已链接静态库或需确保板端有动态库。

---

## ⚠️ 注意事项

1. **坐标对齐**:为了保证硬件效率，OSD 的坐标 (X, Y) 和宽高建议为 2 或 16 的倍数 (系统会自动按 16 对齐)。
2. **Z-order**: `osd.0` 到 `osd.7` 的图层顺序由 `RGN_HANDLE` 决定，通常 ID 越大越靠上层。
3. **中文支持**: 需要字体文件支持中文字符集 (如 simsun)，且 INI 文件必须为 UTF-8 编码。
