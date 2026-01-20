#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "capture/video_capture.h"
#include "config.h"
#include "common.h"
#include "param.h"
#include "isp.h" // Rockchip ISP 接口

// 声明 streamer 中的测试函数
void streamer_init_test();

// 帧回调函数
void on_frame_captured(VideoCaptureContext* ctx, VideoFrame* frame, void* user_data) {
    static int frame_count = 0;
    frame_count++;
    
    // 4. 保存第 60 帧用于测试 (避开前几帧可能的不稳定)
    if (frame_count == 60) {
        FILE* fp = fopen("test_frame_1920x1080.nv12", "wb");
        if (fp) {
            fwrite(frame->start, 1, frame->length, fp);
            fclose(fp);
            printf("[Capture] Saved frame %d to 'test_frame_1920x1080.nv12' (%zu bytes)\n", frame_count, frame->length);
        } else {
            perror("Failed to save debug frame");
        }
    }

    // 每 30 帧打印一次日志
    if (frame_count % 30 == 0) {
        printf("[Capture] Frame %d: %dx%d, Format: %d, Size: %zu bytes, TS: %llu us\n", 
               frame_count, frame->width, frame->height, frame->format, 
               frame->length, (unsigned long long)frame->timestamp);
    }
}

int main() {
    // 打印版本信息
    rkipc_version_dump();
    
    // 初始化参数管理模块 (默认路径 /userdata/rkipc.ini)
    if (rk_param_init(NULL) < 0) {
        printf("[Param] Warning: Failed to init param system, using defaults.\n");
    }

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
        .type = app_cfg->capture.type, // 传递摄像头类型
        .dev_path = app_cfg->capture.dev_path,
        .width = app_cfg->capture.width,
        .height = app_cfg->capture.height,
        .format = app_cfg->capture.format,
        .fps = app_cfg->capture.fps,
        .buffer_count = app_cfg->capture.buffer_count
    };

    printf("Config: %s (%s), %dx%d @ %d fps\n", 
           cap_config.dev_path, 
           cap_config.type == VIDEO_TYPE_MIPI ? "MIPI" : "USB",
           cap_config.width, cap_config.height, cap_config.fps);

    // ==========================================
    // ISP 初始化 (针对 MIPI 摄像头)
    // ==========================================
    if (cap_config.type == VIDEO_TYPE_MIPI) {
        printf("[ISP] Initializing ISP for Cam 0...\n");
        // 初始化 ISP，使用默认 IQ 文件路径 /etc/iqfiles
        if (rk_isp_init(0, NULL) < 0) {
            fprintf(stderr, "[ISP] Failed to init ISP! Ensure /etc/iqfiles exists.\n");
        } else {
            printf("[ISP] Init success. Configuring Exposure...\n");
            
            // 1. 设置自动曝光 (Auto Exposure)
            rk_isp_set_exposure_mode(0, "auto");
            rk_isp_set_gain_mode(0, "auto");
            
            // 2. 提高亮度和对比度 (解决太暗的问题)
            // 默认值通常为 50，设置为 60-70 可以显著提亮
            rk_isp_set_brightness(0, 70); 
            rk_isp_set_contrast(0, 60);
            
            // 3. (可选) 开启暗区增强
            // rk_isp_set_dark_boost_level(0, 10);
            
            printf("[ISP] Settings applied: Brightness=70, Contrast=60, AE=Auto\n");
        }
    }

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
    
    if (cap_config.type == VIDEO_TYPE_MIPI) {
        printf("[ISP] Deinit...\n");
        rk_isp_deinit(0);
    }
    
    rk_param_deinit();
    printf("Test finished.\n");
    
    return 0;
}