#include "config.h"

static const VideoConfig g_video_config = {
    .vi_dev_id = APP_VI_DEV_ID,
    .vi_pipe_id = APP_VI_PIPE_ID,
    .vi_chn_id = APP_VI_CHN_ID,
    .venc_chn_id = APP_VENC_CHN_ID,
    .rtsp_id = APP_RTSP_ID,
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

static const VideoConfig g_video1_config = {
    .vi_dev_id = APP_VI_DEV_ID,
    .vi_pipe_id = APP_VI_PIPE_ID,
    .vi_chn_id = APP_VI_CHN_ID,
    .venc_chn_id = APP_VENC1_CHN_ID,
    .rtsp_id = APP_RTSP_ID_1,
    .vi_entity_name = APP_VI_ENTITY_NAME,
    .width = APP_VIDEO1_WIDTH,
    .height = APP_VIDEO1_HEIGHT,
    .fps = APP_VIDEO1_FPS,
    .bitrate = APP_VIDEO1_BITRATE,
    .gop = APP_VIDEO1_GOP,
    .codec = APP_VIDEO1_CODEC,
    .output_path = APP_VIDEO1_OUTPUT_PATH,
    .rtsp_url = APP_RTSP_URL_1,
};

const VideoConfig *app_video1_config_get(void) {
    return &g_video1_config;
}
