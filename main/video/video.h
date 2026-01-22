/**
 * @file video.h
 * @brief 视频采集与编码模块接口定义
 * 
 * 提供视频子系统的初始化与反初始化操作。
 * 
 * 多线程架构说明:
 * ┌─────────────┐     ┌─────────────┐     ┌─────────────┐
 * │     VI      │ --> │ VENC Thread │ --> │ RTSP Thread │
 * │  (Capture)  │     │  (Encode)   │     │   (Push)    │
 * └─────────────┘     └─────────────┘     └─────────────┘
 *        │                   │                   │
 *        ▼                   ▼                   ▼
 *   硬件 Bind 直连      获取编码码流        推送到 RTSP
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化视频采集与编码子系统
 * 
 * 包括：
 * - 初始化 VI 设备和通道
 * - 创建 VENC 编码通道
 * - 启动 RTSP 服务
 * - 创建并启动编码线程和推流线程
 * 
 * @return 0 成功, 非 0 失败
 */
int rk_video_init(void);

/**
 * @brief 停止视频子系统并释放资源
 * 
 * 按照初始化相反的顺序:
 * - 停止所有工作线程
 * - 关闭帧队列
 * - 解绑 VI 和 VENC
 * - 销毁通道和设备
 * - 关闭 RTSP 服务
 * 
 * @return 0 成功
 */
int rk_video_deinit(void);

#ifdef __cplusplus
}
#endif
