#include "capture/video_capture.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <linux/videodev2.h>

// ============================================================ 
// 内部数据结构定义
// ============================================================ 

typedef struct { 
    void* start;
    size_t length;
} BufferInfo;

struct VideoCaptureContext {
    VideoCaptureType type;
    char dev_path[64];
    int fd;    
    int width;
    int height;
    VideoPixelFormat format;
    int fps;
    
    BufferInfo* buffers;
    int buffer_count;
    int is_streaming;

    OnFrameCallback callback;
    void* user_data;
};

// ============================================================ 
// 辅助函数
// ============================================================ 

static int xioctl(int fh, int request, void *arg) { 
    int r;
    do { 
        r = ioctl(fh, request, arg);
    } while (-1 == r && EINTR == errno);
    return r;
}

static uint32_t fmt_to_v4l2(VideoPixelFormat fmt) { 
    switch (fmt) { 
        case VIDEO_FMT_YUYV:  return V4L2_PIX_FMT_YUYV;
        case VIDEO_FMT_MJPEG: return V4L2_PIX_FMT_MJPEG;
        case VIDEO_FMT_H264:  return V4L2_PIX_FMT_H264;
        case VIDEO_FMT_NV12:  return V4L2_PIX_FMT_NV12;
        case VIDEO_FMT_NV21:  return V4L2_PIX_FMT_NV21;
        default:              return V4L2_PIX_FMT_YUYV; // Default
    }
}

// ============================================================ 
// 接口实现
// ============================================================ 

VideoCaptureContext* video_capture_create(const VideoCaptureConfig* config) { 
    if (!config || !config->dev_path) return NULL;

    VideoCaptureContext* ctx = (VideoCaptureContext*)calloc(1, sizeof(VideoCaptureContext));
    if (!ctx) return NULL;

    ctx->type = config->type;
    strncpy(ctx->dev_path, config->dev_path, sizeof(ctx->dev_path) - 1);
    ctx->width = config->width;
    ctx->height = config->height;
    ctx->format = config->format;
    ctx->fps = config->fps;
    ctx->buffer_count = config->buffer_count > 0 ? config->buffer_count : 4;
    ctx->fd = -1;

    printf("[VideoCapture] Init: Type=%s, Device=%s\n", 
           ctx->type == VIDEO_TYPE_MIPI ? "MIPI (ISP)" : "USB (UVC)", 
           ctx->dev_path);

    // 针对 MIPI 摄像头的特定检查
    if (ctx->type == VIDEO_TYPE_MIPI) {
        if (ctx->buffer_count < 4) {
            fprintf(stderr, "[VideoCapture] Warning: MIPI cameras often require >= 4 buffers. Current: %d\n", ctx->buffer_count);
        }
        if (ctx->format != VIDEO_FMT_NV12) {
            fprintf(stderr, "[VideoCapture] Warning: MIPI ISP works best with NV12. Current: %d\n", ctx->format);
        }
    }

    // 1. 打开设备
    ctx->fd = open(ctx->dev_path, O_RDWR | O_NONBLOCK, 0);
    if (ctx->fd == -1) { 
        perror("Opening video device");
        free(ctx);
        return NULL;
    }

    // 2. 检查设备能力
    struct v4l2_capability cap;
    if (xioctl(ctx->fd, VIDIOC_QUERYCAP, &cap) == -1) { 
        perror("Querying capabilities");
        goto error;
    }
    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) { 
        fprintf(stderr, "%s is no video capture device\n", ctx->dev_path);
        goto error;
    }
    // 很多现代 SOC 的 ISP 驱动（如 Rockchip）使用 V4L2_CAP_VIDEO_CAPTURE_MPLANE
    // 如果上面的检查失败，可能需要检查 MPLANE。但在 RV1126 上，video0 节点通常兼容单平面 API。
    // 如果后续遇到问题，这里需要增加对 MPLANE 的支持。

    if (!(cap.capabilities & V4L2_CAP_STREAMING)) { 
        fprintf(stderr, "%s does not support streaming i/o\n", ctx->dev_path);
        goto error;
    }

    // 3. 设置格式
    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = ctx->width;
    fmt.fmt.pix.height = ctx->height;
    fmt.fmt.pix.pixelformat = fmt_to_v4l2(ctx->format);
    fmt.fmt.pix.field = V4L2_FIELD_ANY; 

    if (xioctl(ctx->fd, VIDIOC_S_FMT, &fmt) == -1) { 
        perror("Setting Pixel Format");
        goto error;
    }

    // 更新实际协商的宽高
    if (ctx->width != fmt.fmt.pix.width || ctx->height != fmt.fmt.pix.height) {
        printf("[VideoCapture] Resolution adjusted by driver: %dx%d -> %dx%d\n", 
               ctx->width, ctx->height, fmt.fmt.pix.width, fmt.fmt.pix.height);
        ctx->width = fmt.fmt.pix.width;
        ctx->height = fmt.fmt.pix.height;
    }

    // 4. 设置帧率
    struct v4l2_streamparm streamparm;
    memset(&streamparm, 0, sizeof(streamparm));
    streamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    streamparm.parm.capture.timeperframe.numerator = 1;
    streamparm.parm.capture.timeperframe.denominator = ctx->fps;
    if (xioctl(ctx->fd, VIDIOC_S_PARM, &streamparm) == -1) { 
        // perror("Setting FPS");
    }

    // 5. 申请缓冲区 (MMAP)
    struct v4l2_requestbuffers req;
    memset(&req, 0, sizeof(req));
    req.count = ctx->buffer_count;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (xioctl(ctx->fd, VIDIOC_REQBUFS, &req) == -1) { 
        perror("Requesting Buffer");
        goto error;
    }
    if (req.count < 2) { 
        fprintf(stderr, "Insufficient buffer memory on %s\n", ctx->dev_path);
        goto error;
    }

    ctx->buffers = calloc(req.count, sizeof(BufferInfo));
    if (!ctx->buffers) goto error;

    for (size_t i = 0; i < req.count; ++i) { 
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        if (xioctl(ctx->fd, VIDIOC_QUERYBUF, &buf) == -1) { 
            perror("Querying Buffer");
            goto error;
        }

        ctx->buffers[i].length = buf.length;
        ctx->buffers[i].start = mmap(NULL, buf.length, 
                                     PROT_READ | PROT_WRITE, MAP_SHARED, 
                                     ctx->fd, buf.m.offset);

        if (ctx->buffers[i].start == MAP_FAILED) { 
            perror("mmap");
            goto error;
        }
    }
    ctx->buffer_count = req.count;

    printf("[VideoCapture] Init Success: %dx%d @ %d fps\n", ctx->width, ctx->height, ctx->fps);

    return ctx;

error:
    video_capture_destroy(ctx);
    return NULL;
}

void video_capture_destroy(VideoCaptureContext* ctx) { 
    if (!ctx) return;

    video_capture_stop(ctx);

    if (ctx->buffers) { 
        for (int i = 0; i < ctx->buffer_count; ++i) { 
            if (ctx->buffers[i].start && ctx->buffers[i].start != MAP_FAILED) { 
                munmap(ctx->buffers[i].start, ctx->buffers[i].length);
            }
        }
        free(ctx->buffers);
    }

    if (ctx->fd != -1) { 
        close(ctx->fd);
    }

    free(ctx);
}

void video_capture_set_callback(VideoCaptureContext* ctx, OnFrameCallback callback, void* user_data) { 
    if (ctx) { 
        ctx->callback = callback;
        ctx->user_data = user_data;
    }
}

int video_capture_start(VideoCaptureContext* ctx) { 
    if (!ctx || ctx->is_streaming) return -1;

    for (int i = 0; i < ctx->buffer_count; ++i) { 
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        if (xioctl(ctx->fd, VIDIOC_QBUF, &buf) == -1) { 
            perror("Queue Buffer");
            return -1;
        }
    }

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(ctx->fd, VIDIOC_STREAMON, &type) == -1) { 
        perror("Stream On");
        return -1;
    }

    ctx->is_streaming = 1;
    return 0;
}

int video_capture_stop(VideoCaptureContext* ctx) { 
    if (!ctx || !ctx->is_streaming) return 0;

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(ctx->fd, VIDIOC_STREAMOFF, &type) == -1) { 
        perror("Stream Off");
        return -1;
    }

    ctx->is_streaming = 0;
    return 0;
}

int video_capture_process(VideoCaptureContext* ctx) { 
    if (!ctx || !ctx->is_streaming) return -1;

    fd_set fds;
    struct timeval tv;
    int r;

    FD_ZERO(&fds);
    FD_SET(ctx->fd, &fds);

    tv.tv_sec = 2; // 2秒超时
    tv.tv_usec = 0;

    r = select(ctx->fd + 1, &fds, NULL, NULL, &tv);

    if (r == -1) { 
        if (errno == EINTR) return 0; // 被信号中断，重试
        perror("select");
        return -1;
    }
    if (r == 0) { 
        fprintf(stderr, "Select timeout\n");
        return -1;
    }

    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    if (xioctl(ctx->fd, VIDIOC_DQBUF, &buf) == -1) { 
        if (errno == EAGAIN) return 0;
        perror("Dequeue Buffer");
        return -1;
    }

    // 回调给上层
    if (ctx->callback) { 
        VideoFrame frame;
        frame.start = ctx->buffers[buf.index].start;
        frame.length = buf.bytesused; // 实际使用的数据量
        frame.index = buf.index;
        frame.timestamp = buf.timestamp.tv_sec * 1000000 + buf.timestamp.tv_usec;
        frame.format = ctx->format;
        frame.width = ctx->width;
        frame.height = ctx->height;

        ctx->callback(ctx, &frame, ctx->user_data);
    }

    // 重新入队
    if (xioctl(ctx->fd, VIDIOC_QBUF, &buf) == -1) { 
        perror("Queue Buffer");
        return -1;
    }

    return 0;
}