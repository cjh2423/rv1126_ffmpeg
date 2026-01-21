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

// 采集/编码参数（主码流）。
#define APP_VIDEO_WIDTH 1920
#define APP_VIDEO_HEIGHT 1080
#define APP_VIDEO_FPS 30
#define APP_VIDEO_BITRATE 4000000
#define APP_VIDEO_GOP 60

// 采集/编码参数（次码流）。默认与主码流一致，避免缩放依赖。
#define APP_VIDEO1_WIDTH APP_VIDEO_WIDTH
#define APP_VIDEO1_HEIGHT APP_VIDEO_HEIGHT
#define APP_VIDEO1_FPS APP_VIDEO_FPS
#define APP_VIDEO1_BITRATE APP_VIDEO_BITRATE
#define APP_VIDEO1_GOP APP_VIDEO_GOP

// 编码格式选择。
#define APP_VIDEO_CODEC_H264 0
#define APP_VIDEO_CODEC_H265 1
#define APP_VIDEO_CODEC APP_VIDEO_CODEC_H264
#define APP_VIDEO1_CODEC APP_VIDEO_CODEC

// 项目进度标志：0 关闭/1 开启。
#define APP_Test_VI                 1
#define APP_Test_RTSP               1
#define APP_Test_SAVE_FILE          0
#define APP_Test_VENC1              1


// 输出裸码流文件，便于快速验证。
#define APP_VIDEO_OUTPUT_PATH "/tmp/rv_demo.h264"
#define APP_VIDEO1_OUTPUT_PATH "/tmp/rv_demo_1.h264"

// RTSP 推流地址（路径部分）。
#define APP_RTSP_URL "/live/0"
#define APP_RTSP_URL_1 "/live/1"

// VENC 通道号与 RTSP 流 ID。
#define APP_VENC_CHN_ID 0
#define APP_VENC1_CHN_ID 1
#define APP_RTSP_ID 0
#define APP_RTSP_ID_1 1

typedef struct {
    int vi_dev_id;
    int vi_pipe_id;
    int vi_chn_id;
    int venc_chn_id;
    int rtsp_id;
    const char *vi_entity_name;
    int width;
    int height;
    int fps;
    int bitrate;
    int gop;
    int codec;
    const char *output_path;
    const char *rtsp_url;
} VideoConfig;

const VideoConfig *app_video_config_get(void);
const VideoConfig *app_video1_config_get(void);

#ifdef __cplusplus
}
#endif

#endif // CONFIG_H
