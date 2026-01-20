# 3rdparty 依赖说明

本项目依赖以下第三方库和 SDK，它们应当放置在 `3rdparty/` 目录下。为了保证项目的可迁移性，我们将交叉编译产物提取到了本项目中。

## 1. Rockchip Media SDK (3rdparty/media)

本项目使用了 Rockchip 提供的硬件加速库和多媒体组件。这些文件是从原始 SDK 中提取并打包的。

### 1.1 对应 SDK 原始路径
如果您需要从 SDK 中同步更新代码，请参考以下原始路径：

- **公共代码模块 (common)**:
  - 路径：`project/app/rkipc/rkipc/common/`
  - 包含内容：`common.c`, `param/`, `sysutil/` 等。
- **ISP 核心逻辑 (isp)**:
  - 路径：`project/app/rkipc/rkipc/common/isp/rv1126/`
  - 包含内容：`isp.c`, `rk_gpio.c` 等。
- **底层加速库产物 (Libraries)**:
  - SDK 原始输出路径：`/home/u22/sdk/rv1126_ipc_linux_release/output/out/media_out`
  - 包含内容：编译好的动态库 (`lib/*.so`) 与配套头文件 (`include/`)。本项目 `3rdparty/media` 下的内容均提取自该路径。

### 1.2 本项目目录结构要求
- `3rdparty/media/include/`: 必须包含各模块头文件。
- `3rdparty/media/lib/`: 存放对应的 `.so` 动态库文件。

---

## 2. FFmpeg 获取与交叉编译

FFmpeg 用于视频流封装、RTMP 推流以及部分软件格式转换。

### 2.1 获取源码
在项目的 `3rdparty` 目录下克隆 FFmpeg 官方源码：
```bash
cd 3rdparty
git clone https://github.com/FFmpeg/FFmpeg.git
# 注意：克隆下来的目录名为 FFmpeg (大写)
```

### 2.2 执行交叉编译
项目根目录提供了一个自动化的编译脚本 `build_ffmpeg.sh`。该脚本针对 RV1126 (ARMv7-A) 进行了环境适配。

**操作步骤**：
1. 确保交叉编译工具链路径已在 `build_ffmpeg.sh` 中设置正确。
2. 运行脚本：
   ```bash
   chmod +x build_ffmpeg.sh
   ./build_ffmpeg.sh
   ```
3. 产物将自动安装到 `3rdparty/ffmpeg_install`。

### 2.3 项目集成 (CMake)
`CMakeLists.txt` 会自动寻找并将以下路径加入构建系统：
- 头文件：`3rdparty/ffmpeg_install/include`
- 库文件：`3rdparty/ffmpeg_install/lib`

---

## 3. 部署说明 (目标系统)

由于使用了动态链接，在开发板运行程序时：
1. 必须将 `3rdparty/ffmpeg_install/lib/` 和 `3rdparty/media/lib/` 下的 `.so` 文件部署到板子上（如 `/usr/lib` 或应用同级的 `lib` 目录）。
2. 在运行程序前，通过环境变量指定库路径：
   ```bash
   export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/path/to/your/libs
   ```

具体部署细节请参考 **[快速开始.md](快速开始.md)**。
