# FFmpeg 获取与交叉编译指南（RV1126）

## 方案选择

### 方案一：从 Rockchip SDK 获取（最推荐）⭐⭐⭐⭐⭐

如果您有 RV1126 的完整 SDK，通常已包含编译好的 FFmpeg 库。

```bash
# 查找 SDK 中的 FFmpeg
find /path/to/rv1126_sdk -name "libavformat.so*"
find /path/to/rv1126_sdk -name "libavcodec.so*"

# 常见位置：
# - buildroot/output/target/usr/lib/
# - external/ffmpeg/
# - out/rootfs/usr/lib/
```

找到后，复制到项目的 `3rdparty/ffmpeg/` 目录：
```bash
mkdir -p 3rdparty/ffmpeg/{lib,include}
cp -r /path/to/sdk/ffmpeg/lib/* 3rdparty/ffmpeg/lib/
cp -r /path/to/sdk/ffmpeg/include/* 3rdparty/ffmpeg/include/
```

---

### 方案二：使用 Git 下载源码并交叉编译（推荐）⭐⭐⭐⭐

#### 步骤 1：下载 FFmpeg 源码

```bash
cd /tmp
git clone https://git.ffmpeg.org/ffmpeg.git ffmpeg
cd ffmpeg

# 或使用国内镜像（更快）
git clone https://gitee.com/mirrors/ffmpeg.git ffmpeg
cd ffmpeg

# 切换到稳定版本（推荐 4.4 或 5.1）
git checkout release/5.1
```

#### 步骤 2：配置交叉编译

创建编译脚本 `build_for_rv1126.sh`：

```bash
#!/bin/bash

# 工具链路径
TOOLCHAIN=/home/u22/toolchain/arm-rockchip830-linux-gnueabihf
CROSS_PREFIX=${TOOLCHAIN}/bin/arm-rockchip830-linux-gnueabihf-

# 安装目录
INSTALL_DIR=/home/u22/My_Project/rv1126/rv_demo/3rdparty/ffmpeg

# 清理之前的配置
make clean 2>/dev/null

# 配置 FFmpeg
./configure \
    --prefix=${INSTALL_DIR} \
    --enable-cross-compile \
    --cross-prefix=${CROSS_PREFIX} \
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
    --disable-postproc \
    --disable-avdevice \
    --enable-protocol=file,rtmp,tcp,udp \
    --enable-muxer=flv,mpegts,mp4 \
    --enable-demuxer=h264,hevc,flv \
    --enable-encoder=h264 \
    --enable-decoder=h264 \
    --enable-parser=h264,hevc \
    --extra-cflags="-march=armv7-a -mtune=cortex-a7 -mfloat-abi=hard -mfpu=neon-vfpv4 -Os" \
    --extra-ldflags="-march=armv7-a"

# 编译
make -j$(nproc)

# 安装
make install

echo "FFmpeg 编译完成，已安装到: ${INSTALL_DIR}"
```

#### 步骤 3：执行编译

```bash
chmod +x build_for_rv1126.sh
./build_for_rv1126.sh
```

编译时间约 10-30 分钟，取决于 CPU 性能。

---

### 方案三：下载预编译版本（快速但需验证兼容性）⭐⭐⭐

#### 选项 A：从 GitHub 下载

```bash
# 下载适用于 ARMv7 的预编译版本
cd /tmp
wget https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/ffmpeg-n5.1-latest-linux-arm-gpl-5.1.tar.xz

# 解压
tar -xf ffmpeg-n5.1-latest-linux-arm-gpl-5.1.tar.xz

# 复制到项目
mkdir -p /home/u22/My_Project/rv1126/rv_demo/3rdparty/ffmpeg
cp -r ffmpeg-n5.1-latest-linux-arm-gpl-5.1/* /home/u22/My_Project/rv1126/rv_demo/3rdparty/ffmpeg/
```

**注意**：预编译版本可能与您的工具链不完全兼容，需要测试验证。

---

## 验证 FFmpeg 库

安装完成后，验证库文件：

```bash
cd /home/u22/My_Project/rv1126/rv_demo/3rdparty/ffmpeg

# 检查库文件
ls -lh lib/libav*.so*

# 检查头文件
ls -lh include/libavformat/
ls -lh include/libavcodec/

# 查看库的架构信息
file lib/libavformat.so
# 应该显示：ELF 32-bit LSB shared object, ARM, EABI5 version 1 (SYSV)
```

---

## 更新 CMakeLists.txt

库准备好后，在 `CMakeLists.txt` 中取消注释：

```cmake
# FFmpeg 库路径
set(FFMPEG_DIR ${PROJECT_SOURCE_DIR}/3rdparty/ffmpeg)
include_directories(${FFMPEG_DIR}/include)
link_directories(${FFMPEG_DIR}/lib)

# ...

# FFmpeg 库
target_link_libraries(rv_demo avformat avcodec avutil swscale)
```

---

## 常见问题

### Q1: 编译时提示找不到 `yasm` 或 `nasm`
```bash
sudo apt-get install yasm nasm
```

### Q2: 运行时提示找不到 `.so` 文件
需要将库文件复制到板子上，或设置 `LD_LIBRARY_PATH`：
```bash
export LD_LIBRARY_PATH=/path/to/ffmpeg/lib:$LD_LIBRARY_PATH
```

### Q3: 想要最小化的 FFmpeg
在 configure 时添加更多 `--disable-*` 选项，只保留 RTMP 推流必需的组件。

---

## 推荐流程

1. **优先尝试方案一**：检查是否有 RV1126 SDK
2. **其次方案二**：自己交叉编译（最可控）
3. **最后方案三**：使用预编译版本（需验证）

建议先尝试方案二（交叉编译），这样可以确保与您的工具链完全兼容。
