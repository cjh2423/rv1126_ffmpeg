# 🔍 Rockchip SDK 深度分析

本项目深度结合了 Rockchip RV1126 IPC SDK 的多媒体子系统，并在此基础上构建了灵活的配置方案。

---

## 🛠️ 核心开发环境

### 1. 交叉编译工具链
项目使用 SDK 预装的工具链进行构建，确保与目标板固件库的二进制兼容性。
- **路径**: `/home/u22/toolchain/arm-rockchip830-linux-gnueabihf`

### 2. 核心软件栈 (MPI)
本项目直接调用了 Rockchip MPI (Media Process Interface) 接口：

| 模块 | 关键库 | 职责 |
| :--- | :--- | :--- |
| **ISP** | `librkaiq.so` | 负责 Sensor 图像增强（3A 算法、降噪、宽动态）。 |
| **VI** | `librockit.so` | 视频输入。负责将 ISP 处理后的 YUV 数据捕获并分发。 |
| **VENC** | `librockchip_mpp.so` | 视频编码。将 YUV 原始帧压缩为 H.264/H.265。 |
| **RTSP** | `librtsp.a` | 负责将 VENC 码流打包并通过 RTSP 协议分发。 |

---

## ⚙️ 动态配置系统 (`rk_param`)

为了摆脱硬编码，本项目集成了基于 `iniparser` 的参数管理模块：

1. **逻辑抽象**: `common/param` 模块将 INI 文件的读写操作封装为简单的 `rk_param_get_int` 等接口。
2. **默认路径**: 程序优先加载 `/userdata/rkipc.ini`，如不存在则尝试从 `/tmp/rkipc-factory-config.ini` 恢复。
3. **运行时生效**: 部分参数（如 FPS）在 ISP 初始化阶段读取，实现了不改代码即可适配不同 Sensor。

---

## 📂 外部依赖整合

- **头文件**: `3rdparty/media/include` (包含 `rkaiq`, `rk_mpi`, `mpp` 等)。
- **库文件**: `3rdparty/media/lib` (所有运行时所需的 `.so` 文件)。
- **工具支持**: 依赖 `libiniparser` 进行配置文件解析。

---

## ⚙️ 二次开发建议

1. **画质调优**: 建议使用 `RKISP2.x Tuner` 工具导出 IQ XML 文件，并通过 `-a` 参数指定其目录。
2. **内存管理**: RV1126 内存有限，VENC 通道的 `u32StreamBufCnt` 不宜设得过大，以免发生 OOM。
3. **零拷贝方案**: 本项目已开启 DMABUF 模式，确保了从采集到编码的高效率，避免了 CPU 搬运数据的开销。
4. **多线程缓冲**: 推荐使用 `frame_queue` 进行线程间解耦，特别是当 RTSP 网络发送阻塞时，不会影响 VENC 的硬件编码流程。
