# RTSP 推流集成说明

## 当前视频流图（仅编码落盘）

```
Sensor -> ISP -> VI -> VENC -> 文件(/tmp/rv_demo.h264)
```

说明：
- 采集与编码由 Rockchip MPI 完成
- 码流直接写入文件，未做网络推流

## 目标视频流图（加入 RTSP 推流）

```
Sensor -> ISP -> VI -> VENC -> RTSP Server -> 客户端播放
                     \
                      \-> (可选) 文件落盘
```

说明：
- VENC 输出码流发送到 RTSP server
- 客户端通过 rtsp://<ip>/live/0 获取实时画面

## 计划集成点
- 在 VENC 取流线程中调用 `rkipc_rtsp_write_video_frame()`
- 初始化阶段调用 `rkipc_rtsp_init()`，释放阶段调用 `rkipc_rtsp_deinit()`
- 使用 `common/rtsp` 模块（已保留）

## 后续配置方向
- RTSP URL 建议默认：`/live/0`
- 码流类型与编码器保持一致（H.264/H.265）
