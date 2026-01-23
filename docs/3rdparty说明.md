# � 第三方依赖库说明

本项目将所有必要的第三方库和 Rockchip SDK 核心组件本地化到了 `3rdparty/` 目录中，以便于项目管理和移植，不再依赖外部的 SDK 绝对路径。

---

## 📂 目录结构

```text
3rdparty/
├── media/                  # Rockchip SDK 核心媒体库
│   ├── include/            # SDK 头文件 (ISP, RGA, MPI, RKAIQ 等)
│   │   ├── rkaiq/          # AIQ 图像质量算法库头文件
│   │   ├── rga/            # RGA 2D 加速库头文件
│   │   └── ...             # 其他系统头文件 (rk_mpi_sys.h 等)
│   └── lib/                # SDK 预编译动态库/静态库 (.so, .a)
│
├── freetype/               # 矢量字体渲染引擎 (用于 OSD)
│   ├── include/            # FreeType 头文件 (ft2build.h 等)
│   └── lib/                # libfreetype.a 静态库
│
└── ffmpeg_install/         # FFmpeg 多媒体框架 (用于推流/封装)
    ├── include/            # FFmpeg 头文件 (libavcodec, libavformat 等)
    └── lib/                # FFmpeg 静态库/动态库
```

---

## �️ 关键库列表

### 1. Rockchip SDK 核心库 (`3rdparty/media/lib`)

| 库文件 | 说明 | 依赖关系 |
| :--- | :--- | :--- |
| `librockit.so` | **VI (视频采集)** 核心库，Rockchip 多媒体底层运行时。 | 必须 |
| `librockchip_mpp.so` | **VENC/VDEC** 硬件编解码库 (MPP)。 | 必须 |
| `librkaiq.so` | **ISP** 图像质量调优算法库，负责 3A、降噪等。 | 必须 |
| `librga.so` | **RGA** 2D 图形加速库，用于缩放、裁剪、格式转换。 | 必须 (Monitor/OSD) |
| `librtsp.a` | **RTSP** 服务库，无需 FFmpeg 即可实现 RTSP 推流。 | 可选 (当前使用) |
| `librkmuxer.so` | **RTMP** 封装库，将 H.264/H.265 封装为 FLV 推流。 | 可选 (当前使用) |
| `librksysutils.so` | 系统工具库，提供系统信息查询等辅助功能。 | 必须 |

### 2. OSD 渲染库 (`3rdparty/freetype/lib`)

| 库文件 | 说明 |
| :--- | :--- |
| `libfreetype.a` | 开源矢量字体引擎，用于解析 TrueType (.ttf) 字体并生成位图以叠加到视频上。 |

### 3. 多媒体框架 (`3rdparty/ffmpeg_install/lib`)

| 库文件 | 说明 |
| :--- | :--- |
| `libavformat` | 容器封装与解封装。 |
| `libavcodec` | 音视频编解码 (软解/软编)。 |
| `libavutil` | 基础工具库。 |
（注：当前项目主要使用 Rockchip 硬件接口，FFmpeg 仅作为辅助或特定功能补充）
（注：本项目使用 Rockchip 官方轻量级库 `librkmuxer.so` 替代 FFmpeg 进行 FLV 封装与 RTMP 推流，以避免引入庞大的 FFmpeg 依赖）

---

## ⚠️ 部署说明

### 编译时
CMake 会自动搜索这些目录下的 `include` 和 `lib`。
- 参见 `CMakeLists.txt` 中的 `link_directories` 和 `include_directories` 指令。

### 运行时
对于动态库 (`.so`)，程序运行需要加载它们。
1. **SDK 默认路径**: 开发板通常在 `/usr/lib/` 或 `/oem/usr/lib/` 内置了这些库。
2. **本地路径优先**: 如果使用了版本不一致的库，可以通过 `LD_LIBRARY_PATH` 指定加载路径：
   ```bash
   export LD_LIBRARY_PATH=$(pwd)/3rdparty/media/lib:$LD_LIBRARY_PATH
   ./rv_demo ...
   ```
   *(通常情况下，使用开发板自带的 SDK 库即可)*
