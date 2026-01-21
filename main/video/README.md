# 与 SDK 官方 video 模块的差异说明

本文对比 SDK `rv1126_ipc_rockit/video` 与当前工程 `main/video`。

## 当前版本保留的功能
- 仅保留 VI->VENC 最小链路
- 支持 H.264 / H.265 CBR 编码
- 可选将裸码流写入文件（`/tmp/rv_demo.h264`）

## 被移除/未接入的功能
- 多路编码（主/次/三码流）
- JPEG 抓拍与周期抓拍
- RTSP/RTMP 推流
- OSD 叠加、ROI、区域裁剪
- 录像存储与文件分段
- VO 显示输出
- NPU/IVA/算法结果绘制
- ISP group、多 sensor、多管线配置
- 复杂码控（VBR/SmartP/TSVC4 等）
- 运行时参数系统（`rk_param_*`）

## 结构性差异
- 官方：大量模块化函数（pipe0/pipe1/pipe2、osd/roi/rtsp/rtmp 等）
- 当前：单一 `rk_video_init/rk_video_deinit`，配置硬编码于 `main/config/config.h`

## 主要代码改动点
- `main/video/video.c` 由“全功能模块”改为“最小采集+编码”实现
- 去掉 `color_table/osd/roi/storage/rtmp/rockiva` 相关依赖
- 去掉 `rk_param_*` 配置读取，改为宏配置

## 如需恢复官方功能
建议按模块逐步补回：
1. RTSP 推流（`common/rtsp` + `rkipc_rtsp_*`）
2. 多码流与 JPEG（pipe1/pipe2、jpeg 线程）
3. OSD/ROI/Region Clip
4. 存储/RTMP/AI/NPU
