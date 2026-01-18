# FFmpeg 获取与交叉编译指南（RV1126）

本文档记录了在 RV1126 平台上获取、交叉编译并集成 FFmpeg 的完整过程。

## 1. 获取源码

在项目的 `3rdparty` 目录下克隆 FFmpeg 官方源码。建议使用稳定版本（如 release/5.1 或 release/6.0）。

```bash
cd 3rdparty
git clone https://github.com/FFmpeg/FFmpeg.git
# 注意：克隆下来的目录名为 FFmpeg (大写)
```

## 2. 交叉编译

针对 RV1126 (ARMv7-A 架构) 进行配置和编译。

### 编译脚本 (`build_ffmpeg_rv1126.sh`)

创建并运行以下脚本，将 FFmpeg 安装到独立目录 `ffmpeg_install` 中，以保持源码目录整洁。

```bash
#!/bin/bash
set -e

# 1. 定义路径
# 源码目录 (注意匹配您现有的大写 FFmpeg 目录)
SOURCE_DIR=$(pwd)/FFmpeg
# 安装目录 (编译后的库和头文件将放在这里)
INSTALL_DIR=$(pwd)/ffmpeg_install
# 工具链路径
TOOLCHAIN_BIN="/home/u22/toolchain/arm-rockchip830-linux-gnueabihf/bin"
CROSS_PREFIX="${TOOLCHAIN_BIN}/arm-rockchip830-linux-gnueabihf-"

# 检查源码目录是否存在
if [ ! -d "$SOURCE_DIR" ]; then
    echo "Error: Source directory $SOURCE_DIR not found!"
    echo "Please make sure you are running this script from the '3rdparty' directory."
    exit 1
fi

echo "=== Configuring FFmpeg ==="
echo "Source: $SOURCE_DIR"
echo "Install: $INSTALL_DIR"

cd "$SOURCE_DIR"

# 清理之前的配置
make clean 2>/dev/null || true

# 配置 FFmpeg
./configure \
    --prefix="${INSTALL_DIR}" \
    --enable-cross-compile \
    --cross-prefix="${CROSS_PREFIX}" \
    --arch=armv7-a \
    --target-os=linux \
    --enable-shared \
    --disable-static \
    --disable-doc \
    --disable-ffmpeg \
    --disable-ffplay \
    --disable-ffprobe \
    --enable-gpl \
    --enable-version3 \
    --enable-nonfree \
    --enable-small \
    --disable-debug \
    --disable-avdevice \
    --enable-protocol=file,rtmp,tcp,udp \
    --enable-muxer=flv,mpegts,mp4 \
    --enable-demuxer=h264,hevc,flv,aac \
    --enable-encoder=h264,aac \
    --enable-decoder=h264,hevc,aac \
    --enable-parser=h264,hevc,aac \
    --extra-cflags="-march=armv7-a -mtune=cortex-a7 -mfloat-abi=hard -mfpu=neon-vfpv4 -Os" \
    --extra-ldflags="-march=armv7-a"

echo "=== Building FFmpeg (This may take a while) ==="
make -j$(nproc)

echo "=== Installing FFmpeg ==="
make install

echo "=== Done! ==="
echo "Libraries installed to: ${INSTALL_DIR}/lib"
echo "Headers installed to:   ${INSTALL_DIR}/include"
```

**执行编译：**
```bash
chmod +x build_ffmpeg_rv1126.sh
./build_ffmpeg_rv1126.sh
# 过程比较久
```

## 3. 项目集成 (CMake)

在 `CMakeLists.txt` 中配置 FFmpeg 的包含路径和链接库：

```cmake
set(FFMPEG_DIR ${PROJECT_SOURCE_DIR}/3rdparty/ffmpeg_install)
include_directories(${FFMPEG_DIR}/include)
link_directories(${FFMPEG_DIR}/lib)

target_link_libraries(rv_demo avformat avcodec avutil swscale swresample)

# 设置运行时库查找路径 (可选，用于链接阶段验证)
target_link_options(rv_demo PRIVATE -Wl,-rpath-link,${FFMPEG_DIR}/lib)
```

## 4. 部署与运行

由于使用了动态链接，运行程序时必须将 `.so` 库文件同步部署到开发板。

### 4.1 拷贝库文件
将 `ffmpeg_install` 目录整体拷贝到开发板的 `/userdata/demo/`。

### 4.2 设置环境变量
在开发板上运行程序前，需指定动态库查找路径：

```bash
# 在开发板执行
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/userdata/demo/ffmpeg_install/lib
./rv_demo
```

## 5. 运行验证

程序启动后应输出类似以下内容，表示集成成功：

```text
Hello World from C Language!
This runs on RV1126 with standard libraries.
FFmpeg Integration Test:
  - AvFormat Version: 62.8.102
  - AvCodec Version:  62.23.102
FFmpeg init successful!
```
