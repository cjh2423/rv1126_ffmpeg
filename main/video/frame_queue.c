/**
 * @file frame_queue.c
 * @brief 线程安全的帧队列实现
 */

#include "frame_queue.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>

/**
 * @brief 计算绝对超时时间点
 */
static void calc_abstime(struct timespec *ts, int timeout_ms) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    
    ts->tv_sec = tv.tv_sec + timeout_ms / 1000;
    ts->tv_nsec = tv.tv_usec * 1000 + (timeout_ms % 1000) * 1000000;
    
    if (ts->tv_nsec >= 1000000000) {
        ts->tv_sec++;
        ts->tv_nsec -= 1000000000;
    }
}

FrameQueue *frame_queue_create(int capacity) {
    FrameQueue *queue = (FrameQueue *)malloc(sizeof(FrameQueue));
    if (!queue) return NULL;
    
    if (capacity <= 0) {
        capacity = FRAME_QUEUE_DEFAULT_CAPACITY;
    }
    
    queue->buffer = (FrameData *)calloc(capacity, sizeof(FrameData));
    if (!queue->buffer) {
        free(queue);
        return NULL;
    }
    
    queue->capacity = capacity;
    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;
    queue->closed = 0;
    
    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->not_empty, NULL);
    pthread_cond_init(&queue->not_full, NULL);
    
    return queue;
}

void frame_queue_destroy(FrameQueue *queue) {
    if (!queue) return;
    
    pthread_mutex_lock(&queue->mutex);
    queue->closed = 1;
    pthread_cond_broadcast(&queue->not_empty);
    pthread_cond_broadcast(&queue->not_full);
    pthread_mutex_unlock(&queue->mutex);
    
    pthread_mutex_destroy(&queue->mutex);
    pthread_cond_destroy(&queue->not_empty);
    pthread_cond_destroy(&queue->not_full);
    
    if (queue->buffer) {
        free(queue->buffer);
    }
    free(queue);
}

int frame_queue_push(FrameQueue *queue, const FrameData *frame, int timeout_ms) {
    if (!queue || !frame) return -1;
    
    pthread_mutex_lock(&queue->mutex);
    
    // 等待队列非满
    while (queue->count >= queue->capacity && !queue->closed) {
        if (timeout_ms < 0) {
            pthread_cond_wait(&queue->not_full, &queue->mutex);
        } else if (timeout_ms == 0) {
            pthread_mutex_unlock(&queue->mutex);
            return -1;  // 立即返回
        } else {
            struct timespec abstime;
            calc_abstime(&abstime, timeout_ms);
            int ret = pthread_cond_timedwait(&queue->not_full, &queue->mutex, &abstime);
            if (ret == ETIMEDOUT) {
                pthread_mutex_unlock(&queue->mutex);
                return -1;
            }
        }
    }
    
    if (queue->closed) {
        pthread_mutex_unlock(&queue->mutex);
        return -2;
    }
    
    // 拷贝帧数据到队列
    memcpy(&queue->buffer[queue->tail], frame, sizeof(FrameData));
    queue->tail = (queue->tail + 1) % queue->capacity;
    queue->count++;
    
    pthread_cond_signal(&queue->not_empty);
    pthread_mutex_unlock(&queue->mutex);
    
    return 0;
}

int frame_queue_pop(FrameQueue *queue, FrameData *frame, int timeout_ms) {
    if (!queue || !frame) return -1;
    
    pthread_mutex_lock(&queue->mutex);
    
    // 等待队列非空
    while (queue->count == 0 && !queue->closed) {
        if (timeout_ms < 0) {
            pthread_cond_wait(&queue->not_empty, &queue->mutex);
        } else if (timeout_ms == 0) {
            pthread_mutex_unlock(&queue->mutex);
            return -1;
        } else {
            struct timespec abstime;
            calc_abstime(&abstime, timeout_ms);
            int ret = pthread_cond_timedwait(&queue->not_empty, &queue->mutex, &abstime);
            if (ret == ETIMEDOUT) {
                pthread_mutex_unlock(&queue->mutex);
                return -1;
            }
        }
    }
    
    // 队列已关闭且为空
    if (queue->count == 0) {
        pthread_mutex_unlock(&queue->mutex);
        return -2;
    }
    
    // 取出帧数据
    memcpy(frame, &queue->buffer[queue->head], sizeof(FrameData));
    queue->head = (queue->head + 1) % queue->capacity;
    queue->count--;
    
    pthread_cond_signal(&queue->not_full);
    pthread_mutex_unlock(&queue->mutex);
    
    return 0;
}

int frame_queue_try_push(FrameQueue *queue, const FrameData *frame) {
    if (!queue || !frame) return -1;
    
    pthread_mutex_lock(&queue->mutex);
    
    if (queue->closed || queue->count >= queue->capacity) {
        pthread_mutex_unlock(&queue->mutex);
        return -1;
    }
    
    memcpy(&queue->buffer[queue->tail], frame, sizeof(FrameData));
    queue->tail = (queue->tail + 1) % queue->capacity;
    queue->count++;
    
    pthread_cond_signal(&queue->not_empty);
    pthread_mutex_unlock(&queue->mutex);
    
    return 0;
}

int frame_queue_try_pop(FrameQueue *queue, FrameData *frame) {
    if (!queue || !frame) return -1;
    
    pthread_mutex_lock(&queue->mutex);
    
    if (queue->count == 0) {
        pthread_mutex_unlock(&queue->mutex);
        return -1;
    }
    
    memcpy(frame, &queue->buffer[queue->head], sizeof(FrameData));
    queue->head = (queue->head + 1) % queue->capacity;
    queue->count--;
    
    pthread_cond_signal(&queue->not_full);
    pthread_mutex_unlock(&queue->mutex);
    
    return 0;
}

void frame_queue_close(FrameQueue *queue) {
    if (!queue) return;
    
    pthread_mutex_lock(&queue->mutex);
    queue->closed = 1;
    pthread_cond_broadcast(&queue->not_empty);
    pthread_cond_broadcast(&queue->not_full);
    pthread_mutex_unlock(&queue->mutex);
}

int frame_queue_size(FrameQueue *queue) {
    if (!queue) return 0;
    
    pthread_mutex_lock(&queue->mutex);
    int size = queue->count;
    pthread_mutex_unlock(&queue->mutex);
    
    return size;
}

int frame_queue_is_empty(FrameQueue *queue) {
    if (!queue) return 1;
    
    pthread_mutex_lock(&queue->mutex);
    int empty = (queue->count == 0);
    pthread_mutex_unlock(&queue->mutex);
    
    return empty;
}

int frame_queue_is_closed(FrameQueue *queue) {
    if (!queue) return 1;
    
    pthread_mutex_lock(&queue->mutex);
    int closed = queue->closed;
    pthread_mutex_unlock(&queue->mutex);
    
    return closed;
}
