# FFmpeg 获取与交叉编译指南（RV1126）

本文档记录了在 RV1126 平台上获取、交叉编译并集成 FFmpeg 的完整过程。

## 1. 获取源码

在项目的 `3rdparty` 目录下克隆 FFmpeg 官方源码。

```bash
cd 3rdparty
git clone https://github.com/FFmpeg/FFmpeg.git
# 注意：克隆下来的目录名为 FFmpeg (大写)
```

## 2. 交叉编译

针对 RV1126 (ARMv7-A 架构) 进行配置和编译。

### 编译脚本 (`build_ffmpeg.sh`)

请使用根目录下的 `build_ffmpeg.sh` 脚本进行编译。该脚本会自动将 FFmpeg 安装到 `3rdparty/ffmpeg_install` 目录。

```bash
chmod +x build_ffmpeg.sh
./build_ffmpeg.sh
```

编译过程可能需要几分钟。

## 3. 项目集成 (CMake)

项目的 `CMakeLists.txt` 已配置好指向 `3rdparty/ffmpeg_install`：

```cmake
set(FFMPEG_DIR ${PROJECT_SOURCE_DIR}/3rdparty/ffmpeg_install)
include_directories(${FFMPEG_DIR}/include)
link_directories(${FFMPEG_DIR}/lib)
target_link_libraries(rv_demo avformat avcodec avutil swscale swresample)
```

## 4. 部署说明

由于使用了动态链接，运行程序时必须将 `.so` 库文件同步部署到开发板。

详细的部署步骤请参考 **[快速开始.md](快速开始.md)**。