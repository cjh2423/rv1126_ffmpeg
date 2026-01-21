# PDF 分析 (RV1126/RV1109 ISP/VI/IQ)

整理与当前问题最相关的文档要点，重点聚焦 AIQ/IQ 文件、媒体链路与可操作结论。

## 关键结论

- AIQ 在初始化/流启动阶段必须成功加载 IQ 文件，否则会出现
  `can not get first iq setting in stream on`，导致 ISP 流起不来、后续采集超时。
  - 来源：`PDF/zh/isp/Rockchip_Development_Guide_ISP20_CN_v1.7.3.pdf`
- `rk_aiq_uapi_sysctl_updateIq` 用于运行时切换 IQ 文件，**必须传入 IQ 的完整路径**。
  - 来源：`PDF/zh/isp/Rockchip_Development_Guide_ISP20_CN_v1.7.3.pdf`
- IQ 文件由构建系统通过 `RK_CAMERA_SENSOR_IQFILES` 打包进入固件，列表若未包含
  GC2053 的 IQ，刷机后 AIQ 无法找到。
  - 来源：`PDF/zh/ipc/Rockchip_RV1126_RV1109_Quick_Start_Linux_IPC_SDK_CN.pdf`
- XML/BIN 文件名必须一致；若同时存在 XML 与 BIN，AIQ 优先加载 XML。
  RV1126/RV1109 属于 32-bit 平台，BIN 需匹配该目标。
  - 来源：`PDF/zh/isp/Rockchip_IQ_Tools_Guide_ISP2x_CN_v1.3.1.pdf`
- `rkisp_selfpath`/`rkisp_mainpath` 都是 YUV 输出节点，是否可用取决于
  media graph 的 link 是否 ENABLED。
  - 来源：`PDF/zh/isp/Rockchip_Driver_Guide_VI_CN_v1.1.5.pdf`

## 对本项目的含义

- master 分支不走 AIQ，能出图但偏暗属于正常现象（无 AE/ISP 调参）。
- vi 分支引入 AIQ，若 IQ 文件匹配失败，会在 stream on 阶段卡死并产生帧超时。
- 调整 HLC/BLC 之类参数前，必须先解决 IQ 文件“可加载”的问题。

## 可落地建议

1. 检查固件构建配置：`RK_CAMERA_SENSOR_IQFILES` 必须包含 GC2053 对应 IQ。
2. 确认 IQ 文件部署路径（通常 `/etc/iqfiles` 或 `/oem/usr/share/iqfiles`）。
3. 自动匹配失败时，使用 **完整路径** 显式加载 IQ 文件（`updateIq`）。
4. 使用 `media-ctl -p` 确认 selfpath/mainpath 链路启用状态。
