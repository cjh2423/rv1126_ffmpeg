# SDK 代码关联分析

本文基于本机 SDK 目录内的源码进行只读分析，目的是判断 SDK 侧默认路径/依赖是否与开发板现象有关。

## 1. 参考的 SDK 路径

- `/home/u22/sdk/rv1126_ipc_linux_release/project/app/rkipc/rkipc/common/isp/rv1126/isp.c`
- `/home/u22/sdk/rv1126_ipc_linux_release/project/app/rkipc/rkipc/src/rv1126_ipc_rockit/video/video.c`
- `/home/u22/sdk/rv1126_ipc_linux_release/project/app/rkipc/rkipc/src/rv1126_ipc_rkmedia/video/video.c`

## 2. ISP 初始化逻辑（SDK）

在 `isp.c` 中，SDK 的 ISP 初始化流程与当前项目保持一致，主要要点：

- `rk_isp_init()` 默认使用 `/etc/iqfiles`，并依赖 `rk_param` 读取 `rkipc.ini` 中的参数。
- `rk_aiq_uapi_sysctl_enumStaticMetas()` 用于枚举传感器与静态信息，依赖 media/isp 子设备正常注册。
- 通过 `rk_isp_set_from_ini()` 读取 `rkipc.ini` 中的多项 ISP 配置（曝光、白平衡、降噪、畸变等）。

这意味着：**SDK 对 ISP 子设备枚举完整性和 IQ 文件完整性强依赖**，与开发板日志中的 “entity not initialized” 等信息是高度相关的。

## 3. 视频采集链路（SDK）

在 `video.c` 中（rockit/rkmedia 两个版本一致），SDK 默认的 VI 节点来源为：

- `video.0:src_node`（默认值为 `rkispp_scale0`）
- 当启用 JPEG 或特定分辨率策略时，会切换到 `rkispp_m_bypass`

示例（rockit 版本）：

- 设定 `vi_chn_attr.stIspOpt.aEntityName = "rkispp_scale0"` 或 `"rkispp_m_bypass"`

这说明：**SDK 的默认采集链路依赖 rkispp**。如果 rkispp 在内核侧初始化失败，则 SDK 侧的视频采集逻辑也会受影响。

## 4. 与开发板日志的关联点

开发板日志中出现：

- `rkispp_hw ... can't request region`
- `rkispp_hw ... failed to get cru reset`
- `rkisp ... Entity type ... was not initialized!`

结合 SDK 代码可推断：

- SDK 默认使用 `rkispp_scale0` 路径，但 rkispp 初始化失败会导致这些节点不可用或状态异常。
- ISP 子设备枚举异常会影响 `rk_aiq_uapi_sysctl_enumStaticMetas()`，进而影响 `rk_isp_init()` 的稳定性。
- 虽然当前应用使用 `/dev/video1 (rkisp_selfpath)` 直连 ISP 输出，但 ISP 统计/降噪通路仍可能依赖 rkispp，使得运行中出现 `nr params buffer` 等错误。

## 5. 结论

SDK 侧代码显示：

- 默认设计是 **ISP + ISPP 全链路**，并由 `rkispp_scaleX` 提供视频节点输出。
- 当前开发板的 rkispp 初始化异常，直接与 SDK 默认链路冲突，属于板端驱动/设备树问题，不是应用代码逻辑问题。

因此，**SDK 的实现进一步佐证“板端驱动/设备树配置不完整”是主因**。

