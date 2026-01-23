/**
 * @file video_osd.h
 * @brief 视频 OSD (On-Screen Display) 集成模块
 * 
 * 该模块负责将 OSD 组件与视频编码通道绑定，实现时间戳叠加等功能。
 */

#ifndef __VIDEO_OSD_H__
#define __VIDEO_OSD_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化视频 OSD 模块
 * 
 * 注册 OSD 回调函数并初始化 OSD 服务。
 * 必须在 VENC 通道创建之后调用。
 * 
 * @param venc_chn_id 要绑定 OSD 的 VENC 通道 ID
 * @return 0 成功, 非 0 失败
 */
int video_osd_init(int venc_chn_id);

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
