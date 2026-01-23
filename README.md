# 🎬 RV1126 采集 + 编码 + RTSP/RTMP 开发示例

本项目是一个基于 Rockchip RV1126 平台的全功能 **视频采集、硬件编码与网络推流** 演示工程。支持 RTSP 局域网预览和 RTMP 云端推流。

---

## 🚀 主要功能

- **双码流编码**: 同时支持主码流 (1080P) 与子码流 (VGA/720P) 并行编码。
- **RTSP 实时预览**: 局域网实时预览 (`rtsp://<ip>/live/0` 及 `/live/1`)。
- **RTMP 云端推流**: 支持推流到阿里云、腾讯云等直播平台 (基于 rkmuxer)。
- **多格式支持**: 硬件加速的 H.264 / H.265 编码，切换灵活。
- **2D 硬件加速**: 集成 RGA 模块，支持高效的图像缩放、裁剪、旋转与格式转换。
- **OSD 叠加**: 支持实时时间戳、文字、图片Logo及隐私遮挡 (基于 FreeType + RGN)。
- **性能监控**: 实时监控 CPU、内存、温度及视频编码性能。
- **参数化配置**: 支持 INI 配置文件读取与运行时动态参数管理。
- **多线程流水线架构**: 采集 -> 编码 -> 推流 分离设计，网络抖动不影响编码。
- **开发友好**: 简单的命令行参数控制，一键编译运行。

---

## 📂 项目结构

```bash
.
├── main/               # 业务逻辑
│   ├── main.c           # 程序入口 (参数解析、模块生命周期管理)
│   ├── config/          # 静态宏定义与配置模版
│   ├── video/           # 采集与编码核心 (VI -> VENC, RTSP/RTMP 封装, frame_queue, rga_utils)
│   └── monitor/         # 性能监控模块 (CPU/内存/温度)
├── common/             # 通用封装模块
│   ├── isp/             # ISP/AIQ 画质初始化
│   ├── param/           # 基于 iniparser 的参数管理 (INI 读写)
│   ├── rtsp/            # RTSP 服务与媒体流分发
│   ├── rtmp/            # RTMP 云端推流 (基于 rkmuxer)
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

1. **RTSP 主码流预览** (局域网):
   ```bash
   ffplay rtsp://<开发板IP>/live/0
   ```
2. **RTSP 子码流预览** (局域网):
   ```bash
   ffplay rtsp://<开发板IP>/live/1
   ```
3. **RTMP 云端推流** (需修改配置):
   ```c
   // 在 main/config/config.h 中配置
   #define APP_Test_RTMP  1
   #define APP_RTMP_URL   "rtmp://your-server.com/live/stream_key"
   ```

---

## 📖 核心文档导航

- [⚡ 快速开始指南](./docs/%E5%BF%AB%E9%80%9F%E5%BC%80%E5%A7%8B.md)
- [📡 RTSP 功能说明](./docs/RTSP%E9%9B%86%E6%88%90%E8%AF%B4%E6%98%8E.md)
- [☁️ RTMP 云端推流](./docs/RTMP%E4%BA%91%E7%AB%AF%E6%8E%A8%E6%B5%81.md)
- [⚙️ 项目演进路线](./docs/%E9%A1%B9%E7%9B%AE%E8%AE%A1%E5%88%92.md)
- [🔍 SDK 开发分析](./docs/SDK%E5%88%86%E6%9E%90.md)
- [🎨 RGA 硬件加速说明](./docs/RGA%E5%8A%9F%E8%83%BD%E8%AF%B4%E6%98%8E.md)
- [📟 OSD 功能说明](./docs/OSD%E5%8A%9F%E8%83%BD%E8%AF%B4%E6%98%8E.md)
- [📊 性能监控说明](./docs/%E6%80%A7%E8%83%BD%E7%9B%91%E6%8E%A7%E8%AF%B4%E6%98%8E.md)
- [🕒 RTC 时钟同步](./docs/RTC%E8%AF%B4%E6%98%8E.md)
