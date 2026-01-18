#include "common/config.h"

// 静态全局配置实例
// 使用宏定义进行初始化，方便统一管理
static const AppConfig g_config = {
    .capture = {
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
