/**
 * @file perf_monitor.c
 * @brief 性能监控模块实现
 * 
 * 通过读取 Linux 系统文件获取各项性能指标：
 * - /proc/stat       : CPU 使用率
 * - /proc/meminfo    : 内存使用情况
 * - /sys/class/thermal/thermal_zone0/temp : 芯片温度
 * - /proc/uptime     : 系统运行时间
 */

#include "perf_monitor.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/sysinfo.h> // for sysinfo()

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "perf_monitor"

/* 默认监控间隔 (秒) */
#define DEFAULT_INTERVAL_SEC    5

/* 温度传感器路径 */
#define THERMAL_ZONE_PATH       "/sys/class/thermal/thermal_zone0/temp"
#define THERMAL_ZONE1_PATH      "/sys/class/thermal/thermal_zone1/temp"

/* =========================================================================
 *                              内部状态
 * ========================================================================= */

/* CPU 统计用于计算差值 */
typedef struct {
    uint64_t user;
    uint64_t nice;
    uint64_t system;
    uint64_t idle;
    uint64_t iowait;
    uint64_t irq;
    uint64_t softirq;
    uint64_t steal;
} CpuRawStats;

static CpuRawStats g_last_cpu_stats = {0};
static int g_cpu_stats_initialized = 0;

/* 视频统计 (由外部更新) */
static VideoStats g_video_stats = {0};
static pthread_mutex_t g_video_stats_mutex = PTHREAD_MUTEX_INITIALIZER;

/* 监控线程 */
static pthread_t g_monitor_thread;
static int g_monitor_running = 0;
static int g_monitor_interval_sec = DEFAULT_INTERVAL_SEC;

/* =========================================================================
 *                              内部函数
 * ========================================================================= */

/**
 * @brief 读取 /proc/stat 获取原始 CPU 统计
 */
static int read_cpu_raw_stats(CpuRawStats *raw) {
    FILE *fp = fopen("/proc/stat", "r");
    if (!fp) {
        return -1;
    }
    
    char line[256];
    if (fgets(line, sizeof(line), fp)) {
        /* cpu  user nice system idle iowait irq softirq steal ... */
        /* RV1126 是 32 位系统，uint64_t 需要用 %llu 读取 */
        unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
        if (sscanf(line, "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
               &user, &nice, &system, &idle,
               &iowait, &irq, &softirq, &steal) == 8) {
            
            raw->user = user;
            raw->nice = nice;
            raw->system = system;
            raw->idle = idle;
            raw->iowait = iowait;
            raw->irq = irq;
            raw->softirq = softirq;
            raw->steal = steal;
        }
    }
    fclose(fp);
    return 0;
}

/**
 * @brief 计算 CPU 使用率 (基于前后两次采样的差值)
 */
static float calc_cpu_usage(const CpuRawStats *prev, const CpuRawStats *curr) {
    uint64_t prev_idle = prev->idle + prev->iowait;
    uint64_t curr_idle = curr->idle + curr->iowait;
    
    uint64_t prev_total = prev->user + prev->nice + prev->system + prev->idle +
                          prev->iowait + prev->irq + prev->softirq + prev->steal;
    uint64_t curr_total = curr->user + curr->nice + curr->system + curr->idle +
                          curr->iowait + curr->irq + curr->softirq + curr->steal;
    
    uint64_t total_diff = curr_total - prev_total;
    uint64_t idle_diff = curr_idle - prev_idle;
    
    if (total_diff == 0) {
        return 0.0f;
    }
    
    /* 防止时钟抖动导致负值 */
    if (idle_diff > total_diff) {
        return 0.0f;
    }
    
    return 100.0f * (1.0f - (float)idle_diff / (float)total_diff);
}

/**
 * @brief 读取单个整数值的文件
 */
static int read_int_file(const char *path, int *value) {
    FILE *fp = fopen(path, "r");
    if (!fp) {
        return -1;
    }
    if (fscanf(fp, "%d", value) != 1) {
        fclose(fp);
        return -1;
    }
    fclose(fp);
    return 0;
}

/**
 * @brief 获取 CPU 核心数
 */
static int get_cpu_core_count(void) {
    int count = 0;
    FILE *fp = fopen("/proc/cpuinfo", "r");
    if (!fp) {
        return 1;  /* 默认返回 1 */
    }
    
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "processor", 9) == 0) {
            count++;
        }
    }
    fclose(fp);
    return count > 0 ? count : 1;
}

/**
 * @brief 监控线程函数
 */
static void *monitor_thread_func(void *arg) {
    (void)arg;
    LOG_INFO("Performance monitor thread started\n");
    
    while (g_monitor_running) {
        sleep(g_monitor_interval_sec);
        if (g_monitor_running) {
            perf_print_report();
        }
    }
    
    LOG_INFO("Performance monitor thread stopped\n");
    return NULL;
}

/* =========================================================================
 *                              外部接口实现
 * ========================================================================= */

int perf_monitor_init(void) {
    LOG_INFO("Performance monitor initialized\n");
    
    /* 初始化 CPU 基准统计 */
    read_cpu_raw_stats(&g_last_cpu_stats);
    g_cpu_stats_initialized = 1;
    
    return 0;
}

void perf_monitor_deinit(void) {
    perf_monitor_stop();
    LOG_INFO("Performance monitor deinitialized\n");
}

int perf_monitor_start(int interval_sec) {
    if (g_monitor_running) {
        LOG_WARN("Monitor already running\n");
        return 0;
    }
    
    g_monitor_interval_sec = (interval_sec > 0) ? interval_sec : DEFAULT_INTERVAL_SEC;
    g_monitor_running = 1;
    
    int ret = pthread_create(&g_monitor_thread, NULL, monitor_thread_func, NULL);
    if (ret != 0) {
        LOG_ERROR("Failed to create monitor thread: %d\n", ret);
        g_monitor_running = 0;
        return -1;
    }
    
    LOG_INFO("Performance monitor started, interval=%ds\n", g_monitor_interval_sec);
    return 0;
}

void perf_monitor_stop(void) {
    if (!g_monitor_running) {
        return;
    }
    
    g_monitor_running = 0;
    pthread_join(g_monitor_thread, NULL);
}

int perf_get_cpu_stats(CpuStats *stats) {
    if (!stats) {
        return -1;
    }
    
    CpuRawStats curr;
    if (read_cpu_raw_stats(&curr) != 0) {
        return -1;
    }
    
    if (g_cpu_stats_initialized) {
        stats->usage_percent = calc_cpu_usage(&g_last_cpu_stats, &curr);
    } else {
        stats->usage_percent = 0.0f;
    }
    
    /* 更新基准 */
    g_last_cpu_stats = curr;
    g_cpu_stats_initialized = 1;
    
    stats->core_count = get_cpu_core_count();
    
    return 0;
}

int perf_get_mem_stats(MemStats *stats) {
    if (!stats) {
        return -1;
    }
    
    FILE *fp = fopen("/proc/meminfo", "r");
    if (!fp) {
        return -1;
    }
    
    memset(stats, 0, sizeof(*stats));
    
    char line[256];
    char key[64];
    unsigned long long val; // 使用 unsigned long long 匹配 %llu
    
    while (fgets(line, sizeof(line), fp)) {
        /* 使用更通用的解析方式: Key: Value kB */
        /* 使用 %llu 确保 32位/64位兼容性 */
        if (sscanf(line, "%63s %llu", key, &val) == 2) {
            if (strcmp(key, "MemTotal:") == 0) {
                stats->total_kb = (uint64_t)val;
            } else if (strcmp(key, "MemFree:") == 0) {
                stats->free_kb = (uint64_t)val;
            } else if (strcmp(key, "MemAvailable:") == 0) {
                stats->available_kb = (uint64_t)val;
            }
        }
    }
    fclose(fp);
    
    stats->used_kb = stats->total_kb - stats->available_kb;
    if (stats->total_kb > 0) {
        stats->usage_percent = 100.0f * (float)stats->used_kb / (float)stats->total_kb;
    }
    
    return 0;
}

int perf_get_temp_stats(TempStats *stats) {
    if (!stats) {
        return -1;
    }
    
    int temp_raw;
    
    /* CPU 温度 (thermal_zone0) */
    if (read_int_file(THERMAL_ZONE_PATH, &temp_raw) == 0) {
        stats->cpu_temp = (float)temp_raw / 1000.0f;  /* 毫摄氏度转摄氏度 */
    } else {
        stats->cpu_temp = -1.0f;
    }
    
    /* GPU 温度 (thermal_zone1, 可能不存在) */
    if (read_int_file(THERMAL_ZONE1_PATH, &temp_raw) == 0) {
        stats->gpu_temp = (float)temp_raw / 1000.0f;
    } else {
        stats->gpu_temp = -1.0f;
    }
    
    return 0;
}

int perf_get_report(PerfReport *report) {
    if (!report) {
        return -1;
    }
    
    memset(report, 0, sizeof(*report));
    
    perf_get_cpu_stats(&report->cpu);
    perf_get_mem_stats(&report->mem);
    perf_get_temp_stats(&report->temp);
    
    /* 视频统计 */
    pthread_mutex_lock(&g_video_stats_mutex);
    report->video = g_video_stats;
    pthread_mutex_unlock(&g_video_stats_mutex);
    
    /* 系统运行时间 - 使用 sysinfo 更可靠 */
    struct sysinfo info;
    if (sysinfo(&info) == 0) {
        report->uptime_sec = (uint64_t)info.uptime;
    }
    
    return 0;
}

void perf_print_report(void) {
    PerfReport report;
    if (perf_get_report(&report) != 0) {
        LOG_ERROR("Failed to get performance report\n");
        return;
    }
    
    LOG_INFO("==== Performance Report ====\n");
    LOG_INFO("CPU: %.1f%% (%d cores)\n", report.cpu.usage_percent, report.cpu.core_count);
    /* 使用 %llu 打印 uint64_t，强制转换为 unsigned long long 避免警告 */
    LOG_INFO("MEM: %.1f%% (%llu/%llu MB used)\n", 
             report.mem.usage_percent, 
             (unsigned long long)(report.mem.used_kb / 1024), 
             (unsigned long long)(report.mem.total_kb / 1024));
    
    char temp_str[128] = {0};
    if (report.temp.cpu_temp >= 0) {
        int offset = snprintf(temp_str, sizeof(temp_str), "TEMP: CPU=%.1f°C", report.temp.cpu_temp);
        if (report.temp.gpu_temp >= 0 && offset < sizeof(temp_str)) {
            snprintf(temp_str + offset, sizeof(temp_str) - offset, ", GPU=%.1f°C", report.temp.gpu_temp);
        }
        LOG_INFO("%s\n", temp_str);
    }
    
    if (report.video.venc_fps > 0) {
        LOG_INFO("VIDEO: VI=%.1ffps, VENC=%.1ffps, Bitrate=%uKbps\n",
                 report.video.vi_fps, report.video.venc_fps, report.video.venc_bitrate_kbps);
    }
    
    /* 使用 %llu 打印 uptime */
    LOG_INFO("UPTIME: %lluh %llum %llus\n", 
             (unsigned long long)(report.uptime_sec / 3600),
             (unsigned long long)((report.uptime_sec % 3600) / 60),
             (unsigned long long)(report.uptime_sec % 60));
    LOG_INFO("============================\n");
}

void perf_update_video_stats(float vi_fps, float venc_fps, uint32_t bitrate_kbps) {
    pthread_mutex_lock(&g_video_stats_mutex);
    g_video_stats.vi_fps = vi_fps;
    g_video_stats.venc_fps = venc_fps;
    g_video_stats.venc_bitrate_kbps = bitrate_kbps;
    pthread_mutex_unlock(&g_video_stats_mutex);
}
