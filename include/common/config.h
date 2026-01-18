#ifndef CONFIG_H
#define CONFIG_H

#include "capture/video_capture.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// 摄像头类型定义 (对应 VideoCaptureType)
// ============================================================
#define CAMERA_TYPE_USB  0
#define CAMERA_TYPE_MIPI 1

/**
 * @brief 当前摄像头类型选择
 * 根据实际硬件修改此宏
 */
#define APP_CONFIG_CAMERA_TYPE CAMERA_TYPE_MIPI

// ============================================================
// 默认参数宏定义
// ============================================================

// 视频采集参数
#define APP_CONFIG_CAPTURE_WIDTH     1920
#define APP_CONFIG_CAPTURE_HEIGHT    1080
#define APP_CONFIG_CAPTURE_FPS       30
#define APP_CONFIG_CAPTURE_BUF_COUNT 4

// 根据摄像头类型自动配置默认参数
#if APP_CONFIG_CAMERA_TYPE == CAMERA_TYPE_MIPI
    // MIPI (Rockchip ISP) 最佳实践
    #define APP_CONFIG_CAPTURE_DEV_PATH  "/dev/video0" // 通常是 rkisp_mainpath
    #define APP_CONFIG_CAPTURE_FMT       VIDEO_FMT_NV12 // ISP 硬件输出 NV12 效率最高
    #define APP_CONFIG_CAPTURE_TYPE      VIDEO_TYPE_MIPI
#else
    // USB (UVC) 最佳实践
    #define APP_CONFIG_CAPTURE_DEV_PATH  "/dev/video10" // 具体取决于插入顺序
    #define APP_CONFIG_CAPTURE_FMT       VIDEO_FMT_YUYV // 大多数 USB 摄像头支持 YUYV 或 MJPEG
    #define APP_CONFIG_CAPTURE_TYPE      VIDEO_TYPE_USB
#endif

// 视频编码参数
#define APP_CONFIG_ENCODER_BITRATE   4000000 // 4 Mbps
#define APP_CONFIG_ENCODER_GOP       60
#define APP_CONFIG_ENCODER_CODEC     "h264"

// 推流参数
#define APP_CONFIG_STREAMER_URL      "rtmp://127.0.0.1/live/test"

// ============================================================
// 运行时配置结构体
// ============================================================

typedef struct {
    // 视频采集模块配置
    struct {
        VideoCaptureType type;
        const char* dev_path;
        int width;
        int height;
        VideoPixelFormat format;
        int fps;
        int buffer_count;
    } capture;

    // 视频编码模块配置
    struct {
        int bitrate;
        int gop;
        const char* codec_name;
    } encoder;

    // 推流模块配置
    struct {
        const char* url;
    } streamer;

} AppConfig;

/**
 * @brief 获取全局配置单例
 * @return const AppConfig* 指向只读配置的指针
 */
const AppConfig* app_config_get(void);

#ifdef __cplusplus
}
#endif

#endif // CONFIG_H
