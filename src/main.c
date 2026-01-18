#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "capture/video_capture.h"
#include "common/config.h"

// 声明 streamer 中的测试函数
void streamer_init_test();

// 帧回调函数
void on_frame_captured(VideoCaptureContext* ctx, VideoFrame* frame, void* user_data) {
    static int frame_count = 0;
    frame_count++;
    
    // 每 30 帧打印一次日志，避免刷屏
    if (frame_count % 30 == 0) {
        printf("[Capture] Frame %d: %dx%d, Format: %d, Size: %zu bytes, TS: %llu us\n", 
               frame_count, frame->width, frame->height, frame->format, 
               frame->length, (unsigned long long)frame->timestamp);
    }
}

int main() {
    puts("Hello World from C Language!");
    puts("This runs on RV1126 with standard libraries.");
    
    // 1. 测试 FFmpeg 集成
    streamer_init_test();
    
    // 2. 测试视频采集
    printf("\n=== Starting Video Capture Test ===\n");
    
    // 获取全局配置
    const AppConfig* app_cfg = app_config_get();
    
    // 配置采集参数
    VideoCaptureConfig cap_config = {
        .dev_path = app_cfg->capture.dev_path,
        .width = app_cfg->capture.width,
        .height = app_cfg->capture.height,
        .format = app_cfg->capture.format,
        .fps = app_cfg->capture.fps,
        .buffer_count = app_cfg->capture.buffer_count
    };

    printf("Config: %s, %dx%d @ %d fps\n", 
           cap_config.dev_path, cap_config.width, cap_config.height, cap_config.fps);

    VideoCaptureContext* cap_ctx = video_capture_create(&cap_config);
    if (!cap_ctx) {
        fprintf(stderr, "Failed to create capture context.\n");
        return -1;
    }

    // 设置回调
    video_capture_set_callback(cap_ctx, on_frame_captured, NULL);

    // 开始采集
    if (video_capture_start(cap_ctx) < 0) {
        fprintf(stderr, "Failed to start capture.\n");
        video_capture_destroy(cap_ctx);
        return -1;
    }

    printf("Capture started, running for approx 150 frames...\n");

    // 模拟主循环
    for (int i = 0; i < 150; ++i) {
        if (video_capture_process(cap_ctx) < 0) {
            fprintf(stderr, "Error during capture process.\n");
            break;
        }
    }

    printf("Stopping capture...\n");
    video_capture_stop(cap_ctx);
    video_capture_destroy(cap_ctx);
    
    printf("Test finished.\n");
    
    return 0;
}