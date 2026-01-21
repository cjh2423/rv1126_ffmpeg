#ifndef CONFIG_H
#define CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

// VI 设备/管线/通道选择，对应当前 ISP 管线。
#define APP_VI_DEV_ID 0
#define APP_VI_PIPE_ID 0
#define APP_VI_CHN_ID 0
// ISP 实体名，需要与 media-ctl 图一致。
#define APP_VI_ENTITY_NAME "rkispp_scale0"

// 采集/编码参数。
#define APP_VIDEO_WIDTH 1920
#define APP_VIDEO_HEIGHT 1080
#define APP_VIDEO_FPS 30
#define APP_VIDEO_BITRATE 4000000
#define APP_VIDEO_GOP 60

// 编码格式选择。
#define APP_VIDEO_CODEC_H264 0
#define APP_VIDEO_CODEC_H265 1
#define APP_VIDEO_CODEC APP_VIDEO_CODEC_H264

// 输出裸码流文件，便于快速验证。
#define APP_VIDEO_SAVE_FILE 1
#define APP_VIDEO_OUTPUT_PATH "/tmp/rv_demo.h264"

typedef struct {
    int vi_dev_id;
    int vi_pipe_id;
    int vi_chn_id;
    const char *vi_entity_name;
    int width;
    int height;
    int fps;
    int bitrate;
    int gop;
    int codec;
    int save_file;
    const char *output_path;
} VideoConfig;

const VideoConfig *app_video_config_get(void);

#ifdef __cplusplus
}
#endif

#endif // CONFIG_H
