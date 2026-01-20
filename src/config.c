#include "config.h"
#include <stdio.h>

// Global variables for official Rockchip log.h
int enable_minilog = 0;
int rkipc_log_level = 2; // LOG_LEVEL_INFO

// Weak stubs for RKMedia MPI functions (if not provided by libraries)
int __attribute__((weak)) RK_MPI_VI_PauseChn(int pipeId, int chnId) { return 0; }
int __attribute__((weak)) RK_MPI_VI_ResumeChn(int pipeId, int chnId) { return 0; }
 
 // 静态全局配置实例
// 使用宏定义进行初始化，方便统一管理
static const AppConfig g_config = {
    .capture = {
        .type = APP_CONFIG_CAPTURE_TYPE,
        .dev_path = APP_CONFIG_CAPTURE_DEV_PATH,
        .width = APP_CONFIG_CAPTURE_WIDTH,
        .height = APP_CONFIG_CAPTURE_HEIGHT,
        .format = APP_CONFIG_CAPTURE_FMT,
        .fps = APP_CONFIG_CAPTURE_FPS,
        .buffer_count = APP_CONFIG_CAPTURE_BUF_COUNT
    },
    .encoder = {
        .bitrate = APP_CONFIG_ENCODER_BITRATE,
        .gop = APP_CONFIG_ENCODER_GOP,
        .codec_name = APP_CONFIG_ENCODER_CODEC
    },
    .streamer = {
        .url = APP_CONFIG_STREAMER_URL
    }
};

const AppConfig* app_config_get(void) {
    return &g_config;
}