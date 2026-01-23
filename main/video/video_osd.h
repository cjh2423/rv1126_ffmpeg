/**
 * @file video_osd.h
 * @brief 视频 OSD (On-Screen Display) 集成模块
 * 
 * 该模块负责将 OSD 组件与视频编码通道绑定，实现时间戳叠加等功能。
 * 支持同时绑定多个 VENC 通道（如主码流和子码流）。
 */

#ifndef __VIDEO_OSD_H__
#define __VIDEO_OSD_H__

#ifdef __cplusplus
extern "C" {
#endif

/* 最大支持的 VENC 通道数量 */
#define VIDEO_OSD_MAX_CHN   4

/**
 * @brief 初始化视频 OSD 模块 (支持多通道)
 * 
 * 注册 OSD 回调函数并初始化 OSD 服务。
 * 必须在所有 VENC 通道创建之后调用。
 * OSD 将同时显示在所有指定的通道上。
 * 
 * @param venc_chn_ids 要绑定 OSD 的 VENC 通道 ID 数组
 * @param chn_count 通道数量
 * @return 0 成功, 非 0 失败
 */
int video_osd_init(const int *venc_chn_ids, int chn_count);

/**
 * @brief 反初始化视频 OSD 模块
 * 
 * @return 0 成功
 */
int video_osd_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* __VIDEO_OSD_H__ */
