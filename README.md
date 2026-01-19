# RV_Demo 项目

本项目是一个专为 **Rockchip RV1126** 平台设计的视频采集与处理演示程序。它演示了如何使用 V4L2 进行视频帧采集，集成 FFmpeg 库，并未后续的视频编码/推流任务做准备。

## 功能特性

- **视频采集：** 支持从 V4L2 设备（USB 和 MIPI 摄像头）采集视频帧。
- **FFmpeg 集成：** 链接 FFmpeg 库 (`avformat`, `avcodec`, `avutil`, `swscale`, `swresample`) 用于媒体处理。
- **交叉编译：** 配置为使用 `arm-rockchip830-linux-gnueabihf` 工具链交叉编译至 ARMv7-A (RV1126) 架构。
- **帧处理：** 包含处理采集帧的回调机制（目前的示例是将测试帧保存到磁盘）。

## 环境依赖

在编译之前，请确保已设置以下环境：

1.  **交叉编译工具链：**
    - 项目默认工具链路径为：`/home/u22/toolchain/arm-rockchip830-linux-gnueabihf/bin/`
    - 如果您的工具链位置不同，请修改 `CMakeLists.txt` 的第 6-7 行。
2.  **第三方库：**
    - **FFmpeg：** 必须安装在 `3rdparty/ffmpeg_install`（头文件在 `include/`，库文件在 `lib/`）。
    - 如果需要从源码构建 FFmpeg，请使用提供的 `build_ffmpeg.sh` 脚本。

## 项目结构

```text
rv_demo/
├── 3rdparty/          # 外部库 (FFmpeg, MPP 等)
├── build.sh           # 项目主编译脚本
├── build_ffmpeg.sh    # FFmpeg 依赖构建脚本
├── run.sh             # 开发板运行脚本
├── CMakeLists.txt     # CMake 构建配置
├── docs/              # 文档文件
├── include/           # 头文件
│   ├── capture/
│   ├── common/
│   ├── encoder/
│   └── streamer/
└── src/               # 源代码
    ├── main.c         # 程序入口
    ├── capture/       # 视频采集实现
    ├── common/        # 通用工具
    ├── encoder/       # 视频编码逻辑
    └── streamer/      # 推流逻辑
```

## 编译指南

1.  **克隆仓库**（如果适用）。
2.  **运行编译脚本：**
    ```bash
    ./build.sh
    ```
    该脚本会创建 `build` 目录，运行 CMake 并编译项目。
3.  **产物：**
    可执行文件 `rv_demo` 将生成在 `build/` 目录下。

## 在目标板 (RV1126) 上运行

1.  **传输文件：**
    将 `build/rv_demo` 以及 `run.sh` 复制到您的 RV1126 设备（例如通过 `adb push`，`scp` 或 NFS）。确保同时部署了 FFmpeg 库。
    
    假设您将文件部署在 `/userdata/demo/`：
    ```bash
    adb push build/rv_demo /userdata/demo/
    adb push run.sh /userdata/demo/
    # 还需要确保 FFmpeg 库在 path/to/ffmpeg_install/lib
    ```

2.  **运行应用：**
    在开发板上执行提供的运行脚本：
    ```bash
    chmod +x run.sh
    ./run.sh
    ```
    
    `run.sh` 脚本的内容如下，它会自动设置库路径并运行程序：
    ```bash
    export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/userdata/demo/ffmpeg_install/lib
    chmod 777 rv_demo
    ./rv_demo
    ```

## 功能详情

- **启动：** 应用程序初始化日志和配置。
- **采集：** 尝试打开配置的视频设备（默认 `/dev/video0`）并采集帧。
- **测试输出：**
    - 每 30 帧打印一次帧元数据（大小、时间戳）。
    - 将 **第 60 帧** 保存为当前目录下的 `test_frame_1920x1080.nv12` 以供验证。
- **关闭：** 采集大约 150 帧后自动清理并退出。

## 配置

修改 `src/common/config.h`（或相应的配置文件）以调整：
- 摄像头设备路径（默认：`/dev/video0`）
- 分辨率（默认：1920x1080）
- 帧率
- 采集格式（例如：NV12）
