/**
 * @file perf_monitor.h
 * @brief 性能监控模块
 * 
 * 该模块提供系统资源和硬件性能的实时监控功能：
 * - CPU 使用率
 * - 内存使用率
 * - 芯片温度
 * - ISP 帧率
 * - VENC 编码帧率与码率
 * 
 * 可选择后台线程持续监控并定期打印或手动查询。
 */

#ifndef __PERF_MONITOR_H__
#define __PERF_MONITOR_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 *                              数据结构
 * ========================================================================= */

/**
 * @brief CPU 统计信息
 */
typedef struct {
    float usage_percent;        /**< 总体 CPU 使用率 (0-100%) */
    int core_count;             /**< CPU 核心数 */
} CpuStats;

/**
 * @brief 内存统计信息
 */
typedef struct {
    uint64_t total_kb;          /**< 总内存 (KB) */
    uint64_t free_kb;           /**< 空闲内存 (KB) */
    uint64_t available_kb;      /**< 可用内存 (KB) */
    uint64_t used_kb;           /**< 已用内存 (KB) */
    float usage_percent;        /**< 内存使用率 (0-100%) */
} MemStats;

/**
 * @brief 温度统计信息
 */
typedef struct {
    float cpu_temp;             /**< CPU 温度 (摄氏度) */
    float gpu_temp;             /**< GPU 温度 (摄氏度, 可能不可用为 -1) */
} TempStats;

/**
 * @brief 视频处理性能统计
 */
typedef struct {
    float vi_fps;               /**< VI 采集帧率 */
    float venc_fps;             /**< VENC 编码帧率 */
    uint32_t venc_bitrate_kbps; /**< VENC 实际码率 (Kbps) */
} VideoStats;

/**
 * @brief 综合性能报告
 */
typedef struct {
    CpuStats cpu;
    MemStats mem;
    TempStats temp;
    VideoStats video;
    uint64_t uptime_sec;        /**< 系统运行时间 (秒) */
} PerfReport;

/* =========================================================================
 *                              接口函数
 * ========================================================================= */

/**
 * @brief 初始化性能监控模块
 * 
 * @return 0 成功
 */
int perf_monitor_init(void);

/**
 * @brief 反初始化性能监控模块
 */
void perf_monitor_deinit(void);

/**
 * @brief 启动后台监控线程
 * 
 * 启动后会每隔 interval_sec 秒打印一次性能报告到日志。
 * 
 * @param interval_sec 打印间隔 (秒), 0 表示使用默认值 (5秒)
 * @return 0 成功
 */
int perf_monitor_start(int interval_sec);

/**
 * @brief 停止后台监控线程
 */
void perf_monitor_stop(void);

/**
 * @brief 获取当前 CPU 统计
 * 
 * @param stats 输出 CPU 统计信息
 * @return 0 成功
 */
int perf_get_cpu_stats(CpuStats *stats);

/**
 * @brief 获取当前内存统计
 * 
 * @param stats 输出内存统计信息
 * @return 0 成功
 */
int perf_get_mem_stats(MemStats *stats);

/**
 * @brief 获取当前温度统计
 * 
 * @param stats 输出温度统计信息
 * @return 0 成功
 */
int perf_get_temp_stats(TempStats *stats);

/**
 * @brief 获取综合性能报告
 * 
 * @param report 输出完整性能报告
 * @return 0 成功
 */
int perf_get_report(PerfReport *report);

/**
 * @brief 打印性能报告到日志
 */
void perf_print_report(void);

/**
 * @brief 更新视频性能统计 (由 video 模块调用)
 * 
 * @param vi_fps 当前 VI 帧率
 * @param venc_fps 当前 VENC 帧率
 * @param bitrate_kbps 当前码率 (Kbps)
 */
void perf_update_video_stats(float vi_fps, float venc_fps, uint32_t bitrate_kbps);

#ifdef __cplusplus
}
#endif

#endif /* __PERF_MONITOR_H__ */
