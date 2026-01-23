cmake_minimum_required(VERSION 3.10)

# ============================================================
# 交叉编译工具链配置
# ============================================================
# sdk外的交叉编译工具链(确保项目的可移植性)
set(CMAKE_C_COMPILER "/home/u22/toolchain/arm-rockchip830-linux-gnueabihf/bin/arm-linux-gnueabihf-gcc")
set(CMAKE_CXX_COMPILER "/home/u22/toolchain/arm-rockchip830-linux-gnueabihf/bin/arm-linux-gnueabihf-g++")

# ============================================================
# 项目定义
# ============================================================
project(rv_demo C CXX)

# ============================================================
# 编译选项
# ============================================================
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=armv7-a -mtune=cortex-a7 -mfloat-abi=hard -mfpu=neon-vfpv4")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=armv7-a -mtune=cortex-a7 -mfloat-abi=hard -mfpu=neon-vfpv4")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Os -Wall -g -ggdb")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Os -Wall -g -ggdb")

add_definitions(-DISP_HW_V20)

# ============================================================
# 头文件目录
# ============================================================
set(MEDIA_DIR ${PROJECT_SOURCE_DIR}/3rdparty/media)
set(FFMPEG_DIR ${PROJECT_SOURCE_DIR}/3rdparty/ffmpeg_install)
set(FREETYPE_DIR ${PROJECT_SOURCE_DIR}/3rdparty/freetype)
set(ALSA_DIR ${PROJECT_SOURCE_DIR}/3rdparty/alsa/lib)

include_directories(
    ${PROJECT_SOURCE_DIR}/common
    ${PROJECT_SOURCE_DIR}/common/isp/rv1126
    ${PROJECT_SOURCE_DIR}/common/param
    ${PROJECT_SOURCE_DIR}/common/system
    ${PROJECT_SOURCE_DIR}/common/rtsp
    ${PROJECT_SOURCE_DIR}/common/rtmp
    ${PROJECT_SOURCE_DIR}/common/sysutil
    ${PROJECT_SOURCE_DIR}/common/osd
    ${PROJECT_SOURCE_DIR}/main
    ${PROJECT_SOURCE_DIR}/main/video
    ${PROJECT_SOURCE_DIR}/main/config
    ${PROJECT_SOURCE_DIR}/main/monitor
    ${MEDIA_DIR}/include
    ${MEDIA_DIR}/include/rkaiq
    ${MEDIA_DIR}/include/rkaiq/uAPI
    ${MEDIA_DIR}/include/rkaiq/algos
    ${MEDIA_DIR}/include/rkaiq/common
    ${MEDIA_DIR}/include/rkaiq/xcore
    ${MEDIA_DIR}/include/rkaiq/iq_parser
    ${MEDIA_DIR}/include/rkaiq/ipc_server
    ${MEDIA_DIR}/include/rga
    ${FFMPEG_DIR}/include
    ${FREETYPE_DIR}/include
    ${FREETYPE_DIR}/include/freetype2
)

# ============================================================
# 库路径
# ============================================================
link_directories(
    ${MEDIA_DIR}/lib
    ${FFMPEG_DIR}/lib
    ${FREETYPE_DIR}/lib
)

if(EXISTS "${ALSA_DIR}/libasound.so.2.0.0")
    link_directories(${ALSA_DIR})
endif()

# ============================================================
# 源文件收集
# ============================================================
set(SRCS ${PROJECT_SOURCE_DIR}/main/main.c)
aux_source_directory(${PROJECT_SOURCE_DIR}/main/video SRCS)
aux_source_directory(${PROJECT_SOURCE_DIR}/main/config SRCS)
aux_source_directory(${PROJECT_SOURCE_DIR}/common/isp/rv1126 SRCS)
aux_source_directory(${PROJECT_SOURCE_DIR}/common SRCS)
aux_source_directory(${PROJECT_SOURCE_DIR}/common/param SRCS)
aux_source_directory(${PROJECT_SOURCE_DIR}/common/system SRCS)
aux_source_directory(${PROJECT_SOURCE_DIR}/common/rtsp SRCS)
aux_source_directory(${PROJECT_SOURCE_DIR}/common/rtmp SRCS)
aux_source_directory(${PROJECT_SOURCE_DIR}/common/sysutil SRCS)
aux_source_directory(${PROJECT_SOURCE_DIR}/common/osd SRCS)
aux_source_directory(${PROJECT_SOURCE_DIR}/main/monitor SRCS)

# ============================================================
# 可执行文件
# ============================================================
add_executable(rv_demo ${SRCS})

# ============================================================
# 链接库
# ============================================================
target_link_libraries(rv_demo
    pthread
    rockit              # Rockchip 多媒体框架库
    rkaiq               # Rockchip AIQ 库
    rtsp                # RTSP 静态库
    rockchip_mpp        # Rockchip MPP 库
    drm                 # DRM 库
    asound              # ALSA 库
    m                   # 数学库
    rksysutils          # Rockchip 系统工具库
    rga                 # Rockchip RGA 库
    rkmuxer             # Rockchip Muxer 库 (RTMP 推流)
    freetype            # FreeType 字体渲染库 (OSD)
    # FFmpeg 库 (备用)
    avformat
    avcodec
    avutil
    swresample
)

# ============================================================
# 安装
# ============================================================
install(TARGETS rv_demo RUNTIME DESTINATION bin)
install(FILES rkipc-os04a10.ini DESTINATION share)
install(FILES rkipc-imx335.ini DESTINATION share)
install(FILES rkipc-imx415.ini DESTINATION share)
install(FILES rkipc-gc2053.ini DESTINATION share)
install(PROGRAMS RkLunch.sh DESTINATION bin)
install(PROGRAMS RkLunch-stop.sh DESTINATION bin)
