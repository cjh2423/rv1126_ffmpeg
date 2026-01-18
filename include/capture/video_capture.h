#ifndef VIDEO_CAPTURE_H
#define VIDEO_CAPTURE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 摄像头类型枚举
 */
typedef enum {
    VIDEO_TYPE_USB = 0, // USB UVC 摄像头
    VIDEO_TYPE_MIPI     // MIPI CSI 摄像头 (通常经过 ISP)
} VideoCaptureType;

/**
 * @brief 视频像素格式枚举
 * 目前主要支持常用格式，后续可扩展
 */
typedef enum {
    VIDEO_FMT_UNKNOWN = 0,
    VIDEO_FMT_YUYV,    // YUYV 4:2:2 (Packed)
    VIDEO_FMT_MJPEG,   // Motion-JPEG
    VIDEO_FMT_H264,    // H.264
    VIDEO_FMT_NV12,    // YUV 4:2:0 (Semi-Planar, NV12)
    VIDEO_FMT_NV21     // YUV 4:2:0 (Semi-Planar, NV21)
} VideoPixelFormat;

/**
 * @brief 视频帧数据结构
 */
typedef struct {
    void* start;            // 数据缓冲区起始地址
    size_t length;          // 数据长度
    size_t index;           // 缓冲区索引 (V4L2 内部使用)
    uint64_t timestamp;     // 时间戳 (微秒)
    VideoPixelFormat format;// 像素格式
    int width;              // 宽
    int height;             // 高
} VideoFrame;

/**
 * @brief 视频采集配置结构体
 */
typedef struct {
    VideoCaptureType type;      // 摄像头类型
    const char* dev_path;       // 设备路径，例如 "/dev/video0"
    int width;                  // 期望宽度
    int height;                 // 期望高度
    VideoPixelFormat format;    // 期望像素格式
    int fps;                    // 期望帧率
    int buffer_count;           // 申请的缓冲区数量 (通常 3-5)
} VideoCaptureConfig;

// 前向声明不透明的句柄
typedef struct VideoCaptureContext VideoCaptureContext;

/**
 * @brief 数据帧回调函数类型定义
 * @param ctx 采集句柄
 * @param frame 视频帧数据
 * @param user_data 用户私有数据
 */
typedef void (*OnFrameCallback)(VideoCaptureContext* ctx, VideoFrame* frame, void* user_data);

/**
 * @brief 创建并初始化视频采集上下文
 * 
 * @param config 配置参数指针
 * @return VideoCaptureContext* 成功返回句柄，失败返回 NULL
 */
VideoCaptureContext* video_capture_create(const VideoCaptureConfig* config);

/**
 * @brief 销毁视频采集上下文，释放资源
 * 
 * @param ctx 采集句柄
 */
void video_capture_destroy(VideoCaptureContext* ctx);

/**
 * @brief 设置帧回调函数
 * 
 * @param ctx 采集句柄
 * @param callback 回调函数
 * @param user_data 用户私有数据
 */
void video_capture_set_callback(VideoCaptureContext* ctx, OnFrameCallback callback, void* user_data);

/**
 * @brief 开始采集
 * 
 * @param ctx 采集句柄
 * @return int 0 成功，< 0 失败
 */
int video_capture_start(VideoCaptureContext* ctx);

/**
 * @brief 停止采集
 * 
 * @param ctx 采集句柄
 * @return int 0 成功，< 0 失败
 */
int video_capture_stop(VideoCaptureContext* ctx);

/**
 * @brief 执行单次帧捕获循环 (通常在线程循环中调用)
 * 
 * 该函数会阻塞等待一帧数据，读取后调用回调函数，然后将缓冲区放回队列。
 * 建议在独立线程中循环调用此函数。
 * 
 * @param ctx 采集句柄
 * @return int 0 成功，< 0 失败或超时
 */
int video_capture_process(VideoCaptureContext* ctx);

#ifdef __cplusplus
}
#endif

#endif // VIDEO_CAPTURE_H