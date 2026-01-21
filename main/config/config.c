#include "config.h"

static const VideoConfig g_video_config = {
    .vi_dev_id = APP_VI_DEV_ID,
    .vi_pipe_id = APP_VI_PIPE_ID,
    .vi_chn_id = APP_VI_CHN_ID,
    .vi_entity_name = APP_VI_ENTITY_NAME,
    .width = APP_VIDEO_WIDTH,
    .height = APP_VIDEO_HEIGHT,
    .fps = APP_VIDEO_FPS,
    .bitrate = APP_VIDEO_BITRATE,
    .gop = APP_VIDEO_GOP,
    .codec = APP_VIDEO_CODEC,
    .output_path = APP_VIDEO_OUTPUT_PATH,
    .rtsp_url = APP_RTSP_URL,
};

const VideoConfig *app_video_config_get(void) {
    return &g_video_config;
}
