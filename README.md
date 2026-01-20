# RV_Demo 项目

本项目是一个专为 **Rockchip RV1126** 平台设计的视频采集与处理演示程序。它演示了如何使用 V4L2 进行视频帧采集，集成 FFmpeg 库，并为后续的视频编码/推流任务做准备。

## 功能特性

- **视频采集：** 支持从 V4L2 设备（USB 和 MIPI 摄像头）采集视频帧。
- **FFmpeg 集成：** 链接 FFmpeg 库 (`avformat`, `avcodec`, `avutil`, `swscale`, `swresample`) 用于媒体处理。
- **交叉编译：** 配置为使用 `arm-rockchip830-linux-gnueabihf` 工具链交叉编译至 ARMv7-A (RV1126) 架构。
- **帧处理：** 包含处理采集帧的回调机制（目前的示例是将测试帧保存到磁盘）。
- **ISP 支持：** 集成 Rockchip 官方 ISP 代码，支持 MIPI 摄像头的自动曝光、亮度调节和画质增强。

## 环境依赖

在编译之前，请确保已设置以下环境：

1.  **交叉编译工具链：**
    - 项目默认工具链路径为：`/home/u22/toolchain/arm-rockchip830-linux-gnueabihf/bin/`
    - 如果您的工具链位置不同，请修改 `CMakeLists.txt` 的第 6-7 行。
2.  **第三方库：**
    - **FFmpeg：** 必须安装在 `3rdparty/ffmpeg_install`（头文件在 `include/`，库文件在 `lib/`）。
    - **Rockchip Media：** 必须位于 `3rdparty/media`，包含 rkaiq 等库和头文件。
    - 如果需要从源码构建 FFmpeg，请使用提供的 `build_ffmpeg.sh` 脚本。

## 项目结构

本项目结构遵循“业务逻辑与官方库分离”的原则：

```text
rv_demo/
├── 3rdparty/          # 外部库 (FFmpeg, Media SDK 等)
├── build.sh           # 项目主编译脚本
├── build_ffmpeg.sh    # FFmpeg 依赖构建脚本
├── run.sh             # 开发板运行脚本
├── CMakeLists.txt     # CMake 构建配置
├── docs/              # 文档文件
├── include/           # 头文件
│   ├── common/        # [官方] 模块头文件 (isp, param, sysutil)
│   ├── capture/       # 视频采集接口
│   ├── config.h       # [应用] 静态配置文件
│   ├── encoder/       # 视频编码接口
│   └── streamer/      # 推流接口
└── src/               # 源代码
    ├── main.c         # 程序入口 (负责业务逻辑调度)
    ├── config.c       # [应用] 配置实现及环境桩代码
    ├── capture/       # 视频采集实现 (V4L2)
    ├── common/        # [官方] Rockchip 公共代码 (不可随意修改)
    │   ├── isp/       # ISP 控制逻辑
    │   ├── param/     # INI 参数管理 (依赖 iniparser)
    │   └── sysutil/   # 系统工具 (sysfs, ADC 等)
    ├── encoder/       # 视频编码逻辑 (预留)
    └── streamer/      # 推流逻辑实现
```

> **注意：** `src/common` 和 `include/common` 目录下的代码完全来自 Rockchip 官方 SDK，请不要在该目录下进行任何逻辑修改，以保持后续 SDK 更新的兼容性。

## 编译指南

1.  **运行编译脚本：**
    ```bash
    ./build.sh
    ```
    该脚本会创建 `build` 目录，运行 CMake 并编译项目。
2.  **产物：**
    可执行文件 `rv_demo` 将生成在 `build/` 目录下。

## 在目标板 (RV1126) 上运行

1.  **传输文件：**
    将 `build/rv_demo` 以及 `run.sh` 复制到您的 RV1126 设备（例如通过 `adb push`）。确保同时也部署了 FFmpeg 库。

2.  **运行应用：**
    ```bash
    chmod +x run.sh
    ./run.sh
    ```

## ISP 支持 (MIPI 摄像头)

为了解决 MIPI 摄像头采集图像过暗的问题，本项目集成了 Rockchip 官方 ISP 代码。

### 功能说明
- **自动初始化：** 当检测到 MIPI 摄像头时，程序会自动初始化 ISP。
- **自动平衡：** 默认开启 ISP 自动曝光和自动增益。
- **亮度增强：** 为了进一步优化暗光下的表现，我们在 `main.c` 中通过补丁将亮度(Brightness)提升至 **70**，对比度(Contrast)提升至 **60**。

### 注意事项
1. **IQ 文件：** 基础 tuning 文件应存放在开发板的 `/etc/iqfiles` 目录下。
2. **INI 配置：** 该模块默认从 `/userdata/rkipc.ini` 读取持久化配置。

## 配置

修改 `include/config.h` 以调整：
- 摄像头设备路径（默认：`/dev/video0`）
- 分辨率（默认：1920x1080）
- 采集格式（默认：NV12）
