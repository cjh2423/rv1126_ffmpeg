#!/bin/bash
set -e

# 1. 定义路径 (相对于项目根目录)
# 源码目录
SOURCE_DIR=$(pwd)/3rdparty/FFmpeg
# 安装目录
INSTALL_DIR=$(pwd)/3rdparty/ffmpeg_install
# 工具链路径
TOOLCHAIN_BIN="/home/u22/toolchain/arm-rockchip830-linux-gnueabihf/bin"
CROSS_PREFIX="${TOOLCHAIN_BIN}/arm-rockchip830-linux-gnueabihf-"

# 检查源码目录是否存在
if [ ! -d "$SOURCE_DIR" ]; then
    echo "Error: Source directory $SOURCE_DIR not found!"
    echo "Please make sure you have cloned FFmpeg into 3rdparty/FFmpeg."
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
