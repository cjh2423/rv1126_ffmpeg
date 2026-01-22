/**
 * @file frame_queue.h
 * @brief 线程安全的帧队列接口
 * 
 * 用于在采集、编码、推流线程之间传递数据帧。
 * 基于环形缓冲区实现，支持阻塞式读写和超时机制。
 */

#ifndef FRAME_QUEUE_H
#define FRAME_QUEUE_H

#include <stdint.h>
#include <stddef.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 队列默认容量 */
#define FRAME_QUEUE_DEFAULT_CAPACITY 8

/**
 * @brief 视频帧类型枚举
 */
typedef enum {
    FRAME_TYPE_RAW_YUV = 0,  /**< 原始 YUV 帧 (采集输出) */
    FRAME_TYPE_ENCODED,      /**< 编码后码流帧 */
} FrameType;

/**
 * @brief 帧数据结构
 * 
 * 统一封装原始帧和编码帧的数据描述。
 */
typedef struct {
    FrameType type;          /**< 帧类型 */
    void *data;              /**< 数据指针 (对于 RAW 可能是 MB_BLK 句柄, 对于 ENCODED 是拷贝的内存) */
    size_t size;             /**< 数据大小 (字节) */
    uint64_t pts;            /**< 时间戳 (微秒) */
    int is_keyframe;         /**< 是否为关键帧 */
    int width;               /**< 图像宽度 (RAW 帧使用) */
    int height;              /**< 图像高度 (RAW 帧使用) */
    void *extra;             /**< 扩展字段, 用于传递 MB_BLK 等句柄 */
} FrameData;

/**
 * @brief 帧队列结构 (内部实现, 对外不透明)
 */
typedef struct FrameQueue {
    FrameData *buffer;       /**< 环形缓冲区 */
    int capacity;            /**< 队列容量 */
    int head;                /**< 队头索引 (下一个读取位置) */
    int tail;                /**< 队尾索引 (下一个写入位置) */
    int count;               /**< 当前元素数量 */
    int closed;              /**< 队列是否已关闭 */
    pthread_mutex_t mutex;   /**< 互斥锁 */
    pthread_cond_t not_empty;/**< 非空条件变量 */
    pthread_cond_t not_full; /**< 非满条件变量 */
} FrameQueue;

/**
 * @brief 创建帧队列
 * 
 * @param capacity 队列容量，若为 0 则使用默认值
 * @return FrameQueue* 成功返回队列指针，失败返回 NULL
 */
FrameQueue *frame_queue_create(int capacity);

/**
 * @brief 销毁帧队列
 * 
 * @param queue 队列指针
 */
void frame_queue_destroy(FrameQueue *queue);

/**
 * @brief 向队列推送帧 (阻塞式)
 * 
 * 若队列已满，将阻塞等待直到有空间或队列被关闭。
 * 
 * @param queue 队列指针
 * @param frame 帧数据 (会被拷贝到队列内部)
 * @param timeout_ms 超时时间 (毫秒)，-1 表示无限等待
 * @return 0 成功，-1 失败或超时，-2 队列已关闭
 */
int frame_queue_push(FrameQueue *queue, const FrameData *frame, int timeout_ms);

/**
 * @brief 从队列弹出帧 (阻塞式)
 * 
 * 若队列为空，将阻塞等待直到有数据或队列被关闭。
 * 
 * @param queue 队列指针
 * @param frame 输出帧数据
 * @param timeout_ms 超时时间 (毫秒)，-1 表示无限等待
 * @return 0 成功，-1 失败或超时，-2 队列已关闭且为空
 */
int frame_queue_pop(FrameQueue *queue, FrameData *frame, int timeout_ms);

/**
 * @brief 尝试非阻塞推送
 * 
 * @param queue 队列指针
 * @param frame 帧数据
 * @return 0 成功，-1 队列已满或已关闭
 */
int frame_queue_try_push(FrameQueue *queue, const FrameData *frame);

/**
 * @brief 尝试非阻塞弹出
 * 
 * @param queue 队列指针
 * @param frame 输出帧数据
 * @return 0 成功，-1 队列为空或已关闭
 */
int frame_queue_try_pop(FrameQueue *queue, FrameData *frame);

/**
 * @brief 关闭队列
 * 
 * 关闭后，所有阻塞的推送/弹出操作将返回。
 * 
 * @param queue 队列指针
 */
void frame_queue_close(FrameQueue *queue);

/**
 * @brief 获取队列当前元素数量
 * 
 * @param queue 队列指针
 * @return 当前元素数量
 */
int frame_queue_size(FrameQueue *queue);

/**
 * @brief 检查队列是否为空
 * 
 * @param queue 队列指针
 * @return 1 空，0 非空
 */
int frame_queue_is_empty(FrameQueue *queue);

/**
 * @brief 检查队列是否已关闭
 * 
 * @param queue 队列指针
 * @return 1 已关闭，0 未关闭
 */
int frame_queue_is_closed(FrameQueue *queue);

#ifdef __cplusplus
}
#endif

#endif // FRAME_QUEUE_H
