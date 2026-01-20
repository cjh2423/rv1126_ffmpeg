cmake_minimum_required(VERSION 3.10)

# ============================================================
# 交叉编译工具链配置
# ============================================================
set(CMAKE_C_COMPILER "/home/u22/toolchain/arm-rockchip830-linux-gnueabihf/bin/arm-rockchip830-linux-gnueabihf-gcc")
set(CMAKE_CXX_COMPILER "/home/u22/toolchain/arm-rockchip830-linux-gnueabihf/bin/arm-rockchip830-linux-gnueabihf-g++")

# ============================================================
# 项目定义
# ============================================================
project(RV_Demo C CXX)

# ============================================================
# 编译选项
# ============================================================
# ARMv7-A 架构 (RV1126)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=armv7-a -mtune=cortex-a7 -mfloat-abi=hard -mfpu=neon-vfpv4")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=armv7-a -mtune=cortex-a7 -mfloat-abi=hard -mfpu=neon-vfpv4")

# 优化选项
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Os -Wall")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Os -Wall")

# Debug 符号（方便调试，Release 时可移除）
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")

# ============================================================
# 头文件目录
# ============================================================
include_directories(
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/common
    ${PROJECT_SOURCE_DIR}/include/common/isp
    ${PROJECT_SOURCE_DIR}/include/common/param
    ${PROJECT_SOURCE_DIR}/include/common/sysutil
    ${PROJECT_SOURCE_DIR}/include/capture
    ${PROJECT_SOURCE_DIR}/include/encoder
    ${PROJECT_SOURCE_DIR}/include/streamer
)

# ============================================================
# 第三方库路径（预留，后续添加）
# ============================================================
# FFmpeg 库路径
set(FFMPEG_DIR ${PROJECT_SOURCE_DIR}/3rdparty/ffmpeg_install)
include_directories(${FFMPEG_DIR}/include)
link_directories(${FFMPEG_DIR}/lib)

# media库路径
set(MEDIA_DIR ${PROJECT_SOURCE_DIR}/3rdparty/media)
include_directories(
    ${MEDIA_DIR}/include
    ${MEDIA_DIR}/include/rkaiq
    ${MEDIA_DIR}/include/rkaiq/uAPI
    ${MEDIA_DIR}/include/rkaiq/common
    ${MEDIA_DIR}/include/rkaiq/xcore
    ${MEDIA_DIR}/include/rkaiq/algos
    ${MEDIA_DIR}/include/rkaiq/iq_parser
    ${MEDIA_DIR}/include/rkaiq/ipc_server
    ${MEDIA_DIR}/include/libdrm
)
link_directories(${MEDIA_DIR}/lib)

# ============================================================
# 源文件收集
# ============================================================
# 主程序
set(MAIN_SRC src/main.c)

# 应用配置
set(APP_SRC src/config.c)

# 官方公共模块 (包含 param, isp, sysutil)
file(GLOB COMMON_SRC src/common/*.c)
file(GLOB PARAM_SRC src/common/param/*.c)
file(GLOB ISP_SRC src/common/isp/*.c)
file(GLOB SYSUTIL_SRC src/common/sysutil/*.c)

# 视频采集模块
file(GLOB CAPTURE_SRC src/capture/*.c)

# 视频编码模块
file(GLOB ENCODER_SRC src/encoder/*.c)

# 推流模块
file(GLOB STREAMER_SRC src/streamer/*.c)

# 汇总所有源文件
set(ALL_SOURCES
    ${MAIN_SRC}
    ${APP_SRC}
    ${COMMON_SRC}
    ${PARAM_SRC}
    ${ISP_SRC}
    ${SYSUTIL_SRC}
    ${CAPTURE_SRC}
    ${ENCODER_SRC}
    ${STREAMER_SRC}
)

# ============================================================
# 可执行文件
# ============================================================
add_executable(rv_demo ${ALL_SOURCES})

# ============================================================
# 链接库
# ============================================================
# 线程库（pthread）
target_link_libraries(rv_demo pthread)

# 数学库（可选）
target_link_libraries(rv_demo m)

# FFmpeg 库（后续取消注释）
target_link_libraries(rv_demo avformat avcodec avutil swscale swresample)

# Rockchip 媒体库
target_link_libraries(rv_demo rkaiq rockchip_mpp rga drm rksysutils)

# ============================================================
# 链接选项
# ============================================================
# 静态链接（可选，根据需要启用）
# target_link_options(rv_demo PRIVATE -static)

# 动态链接（推荐，减小可执行文件大小）
target_link_options(rv_demo PRIVATE -Wl,-rpath-link,${PROJECT_SOURCE_DIR}/3rdparty/ffmpeg_install/lib:${MEDIA_DIR}/lib)