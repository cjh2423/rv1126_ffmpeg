# 📁 Video 模块核心解读

本目录包含 `main/video/` 的核心实现，负责从图像采集 (VI) 到硬件编码 (VENC) 再到网络推流 (RTSP) 的完整业务闭环。

---

## 🚀 核心特性

- **多线程流水线架构**: 采用编码线程 + 推流线程分离设计，解耦各处理阶段。
- **双路并行编码**: 同时维护两路 VENC 通道，支持主/子码流独立配置。
- **RTSP 自动化分发**: 编码后的每一帧通过帧队列异步推送到 RTSP 服务。
- **线程安全队列**: 使用环形缓冲区在线程间传递数据，支持阻塞与超时机制。
- **灵活的输出策略**: 可以在 `config.h` 中开关本地文件保存与网络推流功能。

---

## 🏗️ 多线程架构

```
                          ┌─────────────────────────────────────────┐
                          │           Stream Context (per chn)      │
                          ├─────────────────────────────────────────┤
┌─────────┐  Hardware     │  ┌─────────────┐      ┌─────────────┐  │
│   VI    │ ─── Bind ────►│  │ VENC Thread │ ──►  │ RTSP Thread │  │
│(Capture)│               │  └─────────────┘      └─────────────┘  │
└─────────┘               │         │                    │         │
                          │         ▼                    ▼         │
                          │   Get Encoded        Push to RTSP      │
                          │   Stream             or Save File      │
                          │                                        │
                          │  [stream_queue: FrameData ring buffer] │
                          └─────────────────────────────────────────┘
```

### 线程职责

| 线程 | 功能 | 数据流向 |
|------|------|----------|
| **VENC Thread** | 从硬件编码器获取码流，封装后放入队列 | `VENC → stream_queue` |
| **RTSP Thread** | 从队列获取码流，推送到 RTSP 服务器 | `stream_queue → RTSP` |

### 设计决策

1. **保留 VI→VENC 硬件 Bind**
   - RV1126 的 Bind 机制在内核态完成零拷贝的帧传递，效率最高。
   - 解绑 VI 和 VENC 会引入不必要的用户态拷贝开销。

2. **分离编码获取与推流**
   - VENC 的 `GetStream` 和 RTSP 的网络 I/O 相互解耦。
   - 网络抖动不会阻塞编码器的输出缓冲区。
   - 队列缓冲可以平滑各阶段的处理时间波动。

3. **使用 FrameQueue 传递数据**
   - 线程安全的阻塞队列，避免忙轮询。
   - 支持超时和非阻塞操作，便于优雅退出。

---

## 🛠️ 代码结构拆解

### 1. 帧队列 (`frame_queue.h/.c`)

```c
// 创建与销毁
FrameQueue *frame_queue_create(int capacity);
void frame_queue_destroy(FrameQueue *queue);

// 阻塞式推送/弹出
int frame_queue_push(queue, frame, timeout_ms);
int frame_queue_pop(queue, frame, timeout_ms);

// 非阻塞尝试
int frame_queue_try_push(queue, frame);
int frame_queue_try_pop(queue, frame);

// 关闭队列 (唤醒所有等待线程)
void frame_queue_close(queue);
```

### 2. 视频流上下文 (`VideoStreamContext`)

每路视频流独立管理自己的资源：

```c
typedef struct {
    const VideoConfig *cfg;      // 配置信息
    
    pthread_t venc_thread;       // 编码线程
    pthread_t rtsp_thread;       // 推流线程
    
    FrameQueue *raw_queue;       // 原始帧队列 (预留)
    FrameQueue *stream_queue;    // 编码码流队列
    
    volatile int running;        // 运行控制标志
} VideoStreamContext;
```

### 3. 资源生命周期

- **`rk_video_init`**:
  1. 初始化 VI 设备与管道
  2. 创建 VI→VENC 硬件 Bind
  3. 创建帧队列
  4. 启动 VENC 编码线程和 RTSP 推流线程
  5. 配置并启动 RTSP 服务

- **`rk_video_deinit`**:
  1. 设置停止标志 `g_video_run = 0`
  2. 关闭帧队列，唤醒阻塞线程
  3. `pthread_join` 等待所有线程退出
  4. 解绑 VI→VENC
  5. 销毁 VENC 通道和 VI 设备
  6. 关闭 RTSP 服务

---

## ⚙️ INI 配置绑定

本模块已接入 `common/param` 系统，以下参数可通过 `rkipc.ini` 动态修改：

- `[video.0]` & `[video.1]`:
  - `width` / `height` / `fps`
  - `bitrate` / `gop`
  - `output_data_type` (H.264/H.265)

---

## 📝 开发备注

- **内存安全**: 
  - 编码线程获取的码流需要 `malloc` 拷贝后推入队列，因为 SDK 的码流缓冲区会被复用。
  - 推流线程处理完后负责 `free` 帧数据内存。
  
- **线程同步**:
  - 使用 `pthread_cond` 实现高效的生产者-消费者模式。
  - 队列关闭时会广播唤醒所有等待线程。

- **性能考量**:
  - 双路 1080P@30fps 作为性能上限 (受 ISP 吞吐量限制)。
  - 队列容量可调节以平衡延迟和吞吐量。

- **扩展预留**:
  - `raw_queue` 可用于未来实现 Non-Bind 模式（手动 VI→VENC 送帧）。
  - `vi_thread` 预留位置可用于添加采集线程。
