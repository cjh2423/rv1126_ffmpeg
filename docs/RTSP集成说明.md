# 📡 RTSP 推流实现说明

本项目已完整集成 RTSP 服务端功能，支持通过网络实时预览视频流。

---

## 🏗️ 架构实现

### 1. 数据流向
```mermaid
graph LR
    Sensor --> ISP --> VI --> VENC
    VENC -- 获取码流 --> VencThread
    VencThread -- 推送数据 --> RTSP_Server
    RTSP_Server -- 网络传输 --> Player(VLC/ffplay)
```

### 2. 核心组件
- **服务基础**: 基于 `common/rtsp` 模块封装。
- **并发处理**: 采用多线程模型，每个编码通道对应一个分发队列。
- **协议支持**: 支持 H.264 与 H.265 自动识别与打包传输。

---

## 🛠️ 当前配置

本项目默认开启双路 RTSP 推流：

| 资源路径 | 默认分辨率 | 编码格式 | 预览命令 |
| :--- | :--- | :--- | :--- |
| `/live/0` | 1920x1080 | H.264/H.265 | `ffplay rtsp://<IP>/live/0` |
| `/live/1` | 1920x1080 | H.264/H.265 | `ffplay rtsp://<IP>/live/1` |

---

## 💻 代码集成逻辑

### 1. 初始化
在 `rk_video_init` 中自动根据配置启动 RTSP 服务：
```c
rkipc_rtsp_init(cfg->rtsp_url, cfg1->rtsp_url, NULL);
```

### 2. 帧数据的实时推送
在编码线程 `venc_get_stream_thread` 中，从 VENC 获取码流后立即推送到 RTSP 缓存空间：
```c
// 计算相对于基准时间的 PTS
int64_t rtsp_pts = g_rtsp_base_time_us + pts_offset;
// 推送到 RTSP 通道
rkipc_rtsp_write_video_frame(cfg->rtsp_id, data, stStream.pstPack->u32Len, rtsp_pts);
```

---

## 📌 进阶调试

- **延迟优化**: 如果预览延迟过高，建议在 `rkipc.ini` 中调整 `GOP` 大小（如设为与 FPS 一致）并开启 `CBR` 模式。
- **并发链接**: 当前 RTSP 服务器支持 10 个以上并发客户端链接。
- **网络质量**: 建议在 100M/1000M 网卡下使用，确保码流带宽充足。
