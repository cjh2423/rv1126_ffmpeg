# RV1126 采集+编码示例

这是一个最小化的 RV1126 视频采集与编码工程：
- 只保留 ISP/VI/VENC 相关流程
- 启动后将裸码流写入文件（默认 `/tmp/rv_demo.h264`）
- 不包含音频、RTSP、OSD、存储、AI 等功能

## 目录结构
- `main/` 入口与视频模块
- `main/config/` 统一宏配置（硬编码）
- `main/video/` VI->VENC 采集与编码
- `common/` 精简公共模块（仅 ISP/param/system/sysutil/rtsp）
- `3rdparty/media/` SDK 媒体库

## 编译
使用 SDK 内置交叉编译工具链：

```bash
./build.sh
```

生成文件：`build/rv_demo`

## 运行
在板端执行：

```bash
./run.sh
```

默认输出裸码流：`/tmp/rv_demo.h264`

## 配置（硬编码）
修改 `main/config/config.h` 中的宏：
- 分辨率、帧率、码率、GOP、编码格式
- VI 设备/管线/通道与实体名
- 输出文件路径

## 取回与播放
拉取码流：

```bash
adb pull /tmp/rv_demo.h264 ./
```

直接播放裸流：

```bash
ffplay -f h264 -framerate 30 rv_demo.h264
```

或封装为 MP4：

```bash
ffmpeg -framerate 30 -i rv_demo.h264 -c copy rv_demo.mp4
```
