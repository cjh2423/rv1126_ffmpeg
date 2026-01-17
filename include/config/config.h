#ifndef CONFIG_H
#define CONFIG_H

// 硬件参数
#define WIDTH_1080P 1920
#define HEIGHT_1080P 1080
#define WIDTH_720P 1280
#define HEIGHT_720P 720

// 编码参数
#define CODEC_ID H264
#define GOP_SIZE 30
#define KEY_FRAME_INTERVAL 30
#define BITRATE_1080P 5000000
#define BITRATE_720P 2000000

// 网络参数
#define RTMP_URL "rtmp://your-rtmp-server/live"
#define APP_NAME "live"
#define STREAM_NAME "stream"

#endif // CONFIG_H