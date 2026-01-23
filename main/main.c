#include <getopt.h>
#include <rk_mpi_sys.h>

#include "common.h"
#include "config.h"
#include "isp.h"
#include "log.h"
#include "param.h"
#include "system.h"
#include "video.h"
#if APP_Test_PERF_MONITOR
#include "perf_monitor.h"
#endif

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "rkipc.c"

enum { LOG_ERROR, LOG_WARN, LOG_INFO, LOG_DEBUG };

int enable_minilog = 0;
int rkipc_log_level = LOG_INFO;

// 主循环退出标志，由信号触发。
static int g_main_run_ = 1;
char *rkipc_ini_path_ = NULL;
char *rkipc_iq_file_path_ = NULL;

// 简单信号处理：收到信号后退出主循环。
static void sig_proc(int signo) {
	LOG_INFO("received signo %d \n", signo);
	g_main_run_ = 0;
}

static const char short_options[] = "c:a:l:";
static const struct option long_options[] = {{"config", required_argument, NULL, 'c'},
                                             {"aiq_file", no_argument, NULL, 'a'},
                                             {"log_level", no_argument, NULL, 'l'},
                                             {"help", no_argument, NULL, 'h'},
                                             {0, 0, 0, 0}};

// 打印命令行使用说明。
static void usage_tip(FILE *fp, int argc, char **argv) {
	fprintf(fp,
	        "Usage: %s [options]\n"
	        "Version %s\n"
	        "Options:\n"
	        "-c | --config      rkipc ini file, default is "
	        "/userdata/rkipc.ini, need to be writable\n"
	        "-a | --aiq_file    aiq file dir path, default is /etc/iqfiles\n"
	        "-l | --log_level   log_level [0/1/2/3], default is 2\n"
	        "-h | --help        for help \n\n"
	        "\n",
	        argv[0], "V1.0");
}

// 解析命令行参数，支持 ini/iq 路径与日志级别覆盖。
void rkipc_get_opt(int argc, char *argv[]) {
	for (;;) {
		int idx;
		int c;
		c = getopt_long(argc, argv, short_options, long_options, &idx);
		if (-1 == c)
			break;
		switch (c) {
		case 0: /* getopt_long() flag */
			break;
		case 'c':
			rkipc_ini_path_ = optarg;
			break;
		case 'a':
			rkipc_iq_file_path_ = optarg;
			break;
		case 'l':
			rkipc_log_level = atoi(optarg);
			break;
		case 'h':
			usage_tip(stdout, argc, argv);
			exit(EXIT_SUCCESS);
		default:
			usage_tip(stderr, argc, argv);
			exit(EXIT_FAILURE);
		}
	}
}

int main(int argc, char **argv) {
	LOG_INFO("main begin\n");
	rkipc_version_dump();
	int camera_id;
	signal(SIGINT, sig_proc);
	signal(SIGTERM, sig_proc);

	rkipc_get_opt(argc, argv);
	LOG_INFO("rkipc_ini_path_ is %s, rkipc_iq_file_path_ is %s, rkipc_log_level "
	         "is %d\n",
	         rkipc_ini_path_, rkipc_iq_file_path_, rkipc_log_level);
	// 运行时只输出一次系统时间。
	LOG_INFO("当前时间: %s\n", get_time_string());

	// 初始化顺序：param -> system -> isp -> mpi -> video。
	rk_param_init(rkipc_ini_path_);
	rk_system_init();
	camera_id = rk_param_get_int("video.0:camera_id", 0); // need rk_param_init
	rk_isp_init(camera_id, rkipc_iq_file_path_);
	rk_isp_set_frame_rate(0, rk_param_get_int("isp.0.adjustment:fps", 30));
	RK_MPI_SYS_Init();
	rk_video_init();
#if APP_Test_PERF_MONITOR
	perf_monitor_init();
	perf_monitor_start(10);  // 每 10 秒打印一次性能报告
#endif
	LOG_INFO("rkipc init finished.\n");

	// 循环等待退出信号。
	while (g_main_run_) {
		usleep(1000 * 1000);
	}

	// 反初始化顺序：video -> mpi -> isp -> system -> param。
#if APP_Test_PERF_MONITOR
	perf_monitor_deinit();
#endif
	rk_system_deinit();
	rk_video_deinit(); // RK_MPI_SYS_Exit
	RK_MPI_SYS_Exit();
	rk_isp_deinit(camera_id);
	rk_param_deinit();
	LOG_INFO("rkipc deinit finished.\n");

	return 0;
}
