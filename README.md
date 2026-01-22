# 🎥 RV1126 采集 + 编码 + RTSP 开发示例

本项目是一个基于 Rockchip RV1126 平台的全功能 视频采集、硬件编码与 RTSP 推流 演示工程。它提供了从底层驱动调用到上层协议对接的完整实现。

---

## 🚀 主要功能

- **双码流编码**: 同时支持主码流 (1080P) 与子码流 (VGA/720P) 并行编码。
- **RTSP 实时推流**: 集成 RTSP 服务端，支持客户端实时预览 (`rtsp://<ip>/live/0` 及 `/live/1`)。
- **多格式支持**: 硬件加速的 H.264 / H.265 编码，切换灵活。
- **参数化配置**: 支持 INI 配置文件读取与运行时动态参数管理。
- **完善的初始化**: 包含 ISP、VI、VENC、SYS 的标准初始化流程与资源回收。
- **多线程流水线架构**: 采用 采集 -> 编码 -> 推流 分离的流水线设计，提升系统吞吐量。
- **开发友好**: 提供简单的命令行参数控制，支持一键编译运行。

---

## 📂 项目结构

```bash
.
├── main/               # 业务逻辑
│   ├── main.c           # 程序入口 (参数解析、模块生命周期管理)
│   ├── config/          # 静态宏定义与配置模版
│   └── video/           # 采集与编码核心 (VI -> VENC, RTSP 封装, frame_queue)
├── common/             # 通用封装模块
│   ├── isp/             # ISP/AIQ 画质初始化
│   ├── param/           # 基于 iniparser 的参数管理 (INI 读写)
│   ├── rtsp/            # RTSP 服务与媒体流分发
│   └── sysutil/         # 系统工具 (时间戳、内存操作等)
├── docs/               # 详细开发文档
├── 3rdparty/media/    # Rockchip SDK 媒体库 (头文件与库)
├── build.sh            # 一键编译脚本
└── run.sh              # 板端运行启动脚本
```

---

## 🛠️ 运行说明

### 1. 编译
```bash
./build.sh
```

### 2. 运行参数
程序支持多个命令行参数，灵活适配不同环境：
```bash
./rv_demo -c /userdata/rkipc.ini -a /etc/iqfiles -l 2
```
- `-c`: 指定 INI 配置文件路径 (默认 `/userdata/rkipc.ini`)。
- `-a`: 指定 ISP IQ 文件存放目录 (默认 `/etc/iqfiles`)。
- `-l`: 设置日志级别 (0:ERROR, 1:WARN, 2:INFO, 3:DEBUG)。

---

## 📽️ 取回与实时预览

1. **主码流预览**:
   ```bash
   ffplay rtsp://<开发板IP>/live/0
   ```
2. **子码流预览**:
   ```bash
   ffplay rtsp://<开发板IP>/live/1
   ```

---

## 📖 核心文档导航

- [⚡ 快速开始指南](./docs/%E5%BF%AB%E9%80%9F%E5%BC%80%E5%A7%8B.md)
- [📡 RTSP 功能说明](./docs/RTSP%E9%9B%86%E6%88%90%E8%AF%B4%E6%98%8E.md)
- [⚙️ 项目演进路线](./docs/%E9%A1%B9%E7%9B%AE%E8%AE%A1%E5%88%92.md)
- [🔍 SDK 开发分析](./docs/SDK%E5%88%86%E6%9E%90.md)
- [🕒 RTC 时钟同步](./docs/RTC%E8%AF%B4%E6%98%8E.md)
