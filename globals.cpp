#ifndef _GNU_SOURCE
    #define _GNU_SOURCE
#endif

#include <stdatomic.h>
#include <signal.h>
#include <sched.h>
#include <sys/syscall.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/capability.h> 
#include <sys/resource.h> 
#include <string.h>
#include <sys/mman.h>

#include "globals.h"
#include "pwm.h"

bool g_debug_flag = false;
bool g_trace_func_flag = false;

int sched_setattr(pid_t pid, const struct sched_attr *attr, unsigned int flags) {
    return syscall(__NR_sched_setattr, pid, attr, flags);
}

int sched_getattr(pid_t pid, struct sched_attr *attr, unsigned int size, unsigned int flags) {
    return syscall(__NR_sched_getattr, pid, attr, size, flags);
}

static void signal_handler(int signum) {
    //TO DO signum이 종료관련일 경우 종료처리
    printf("signum:%d\n", signum);

	exit_program();
}

static struct sigaction s_sa;

void (*g_exit_func_global_list[20])();
int g_exit_func_global_num = 0;

extern pthread_t g_threads[2];
void __exit_program()
{    
	printf("exit_program entry\n");

    for (int i = 0; i < g_exit_func_global_num; i++) {
        g_exit_func_global_list[i]();
    }

	printf("exit_program exit\n");

	exit(0);
}

void init_signal() 
{    
    s_sa.sa_handler = signal_handler;
    sigemptyset(&s_sa.sa_mask);
    s_sa.sa_flags = 0;

    if (sigaction(SIGINT, &s_sa, NULL) == -1) {
        exit_program();
    }  
} 

void set_rtprio_limit() {
    struct rlimit rl;
    rl.rlim_cur = 99;  // Soft limit
    rl.rlim_max = 99;  // Hard limit

    if (setrlimit(RLIMIT_RTPRIO, &rl) != 0) {
        perror("setrlimit");
        exit_program();
    }
}

struct sched_param s_sp;

void set_rt_deadline(pid_t pid, int runtime, int deadline, int period)
{
	struct sched_attr attr;

	memset(&attr, 0, sizeof(attr));
	attr.size = sizeof(attr);
	attr.sched_policy = SCHED_DEADLINE;
	attr.sched_runtime = runtime;
	attr.sched_deadline = deadline;
	attr.sched_period = period;

	if (sched_setattr(pid, &attr, 0) != 0)
	{
		perror("set_rt_deadline err");
		exit_program();
	}
}
void set_rt_rr(pid_t pid, int prio)
{
	struct sched_param param = {
		.sched_priority = prio,
	};

    printf("set_rt_rr, pid:%d\n", pid);
	if (sched_setscheduler(pid, SCHED_RR, &param) != 0)
	{
		perror("set_rt_rr");
		exit_program();
	}
}
int get_pid_str(const char *cmd, char pid[][30])
{
	FILE *fp;
	char line[50];

	fp = popen(cmd, "r");
	if (fp == NULL)
	{
		perror("get_pid_str, popen");
		exit_program();
	}

	int pid_num = 0;
	while (fgets(line, sizeof(line), fp) != NULL)
	{
		if (line[0] == '\n' || line[0] == '\0') {
			continue;
		}

		char buf[30];
		sscanf(line, "%s", buf); // Skip the first column and read the second as PID

		strncpy(pid[pid_num], buf, 30);
		pid_num++;
	}

	return pid_num;

    pclose(fp);
}