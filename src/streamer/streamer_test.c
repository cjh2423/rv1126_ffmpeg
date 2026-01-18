#include <stdio.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>

/**
 * @brief 初始化 FFmpeg 并打印版本信息
 * 用于验证库是否正确链接
 */
void streamer_init_test() {
    // 在旧版本 FFmpeg 中可能需要 av_register_all()，但在 5.x 版本中已弃用且不需要
    // av_register_all();

    unsigned version = avformat_version();
    printf("FFmpeg Integration Test:\n");
    printf("  - AvFormat Version: %d.%d.%d\n", 
           AV_VERSION_MAJOR(version), 
           AV_VERSION_MINOR(version), 
           AV_VERSION_MICRO(version));
    
    version = avcodec_version();
    printf("  - AvCodec Version:  %d.%d.%d\n", 
           AV_VERSION_MAJOR(version), 
           AV_VERSION_MINOR(version), 
           AV_VERSION_MICRO(version));

    printf("FFmpeg init successful!\n");
}

