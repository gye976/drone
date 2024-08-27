#ifndef _GNU_SOURCE
    #define _GNU_SOURCE
#endif

#include <math.h>
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

// bool g_debug_flag = false;
// bool g_trace_func_flag = false;

int sched_setattr(pid_t pid, const struct sched_attr *attr, unsigned int flags) {
    return syscall(__NR_sched_setattr, pid, attr, flags);
}

int sched_getattr(pid_t pid, struct sched_attr *attr, unsigned int size, unsigned int flags) {
    return syscall(__NR_sched_getattr, pid, attr, size, flags);
}

static void signal_handler(int signum) {
	//TO DO verity signum
	printf("signum:%d\n", signum);

	exit_program();
}

static struct sigaction s_sa;

void (*g_exit_func_global_list[20])();
int g_exit_func_global_num = 0;

pthread_mutex_t g_exit_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t g_exit_cond = PTHREAD_COND_INITIALIZER;

bool g_exit_invoked_flag = 0;
bool g_non_main_thread_request_flag = 0;

extern pthread_t g_main_thread_id;

void __exit_program()
{   	
	if (pthread_self() != g_main_thread_id) {

		pthread_mutex_lock(&g_exit_mutex);
		
		if (g_non_main_thread_request_flag == 1 || g_exit_invoked_flag == 1) {
			pthread_mutex_unlock(&g_exit_mutex);
			goto SLEEP_NON_MAIN_THREAD; 
		}
		g_non_main_thread_request_flag = 1;
		pthread_cond_signal(&g_exit_cond);

		pthread_mutex_unlock(&g_exit_mutex);

		printf("Non-main thread requested exit.\n");
		goto SLEEP_NON_MAIN_THREAD;
	} 
	
	if (g_exit_invoked_flag != 0) {
		printf("exit_program invoked again, bug\n");
		return;
	}
	g_exit_invoked_flag = 1;

 	printf("main thread requested exit.\n");

	printf("cancel_all_threads...\n");

	cancel_all_threads();
	// printf("stop_all_threads...\n");
	// stop_all_threads();

	// printf("wait_all_threads_success_exit...\n");
	// wait_all_threads_success_exit();

	printf("\n###exit_program entry\n");
	printf("###exit class num:%d\n", g_exit_func_global_num);

	for (int i = 0; i < g_exit_func_global_num; i++) {
		g_exit_func_global_list[i]();
	}

	printf("###exit_program exit\n");

	exit(0);

SLEEP_NON_MAIN_THREAD:
	MakeThread *make_thread = find_make_thread(pthread_self());
	if (unlikely(make_thread == NULL)) {
		printf("__exit_program, find_make_thread bug\n");
	}

	make_thread->stop_thread_by_that_thread(); // loop
	
	printf("thread sleep fail, bug\n");
	return;
}

void init_signal() 
{    
    s_sa.sa_handler = signal_handler;
    sigemptyset(&s_sa.sa_mask);
    s_sa.sa_flags = 0;

    if (sigaction(SIGINT, &s_sa, NULL) == -1) {
		perror("init_signal\n");
        exit_program();
    }  

	// struct sigaction sa_usr1;
	// sa_usr1.sa_handler = sigusr1_handler;
    // sigemptyset(&sa_usr1.sa_mask);
    // sa_usr1.sa_flags = 0;
    // if (sigaction(SIGUSR1, &sa_usr1, NULL)) {
	// 	perror("init_signal\n");
    //     exit_program();
    // }  
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

void set_sched_deadline(pid_t pid, int runtime, int deadline, int period)
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
void sched_setscheduler_wrapper(pid_t pid, int policy, int prio)
{
	struct sched_param param = {
		.sched_priority = prio,
	};

	if (sched_setscheduler(pid, policy, &param) != 0)
	{
		perror("sched_setscheduler_wrapper");
		exit_program();
	}
}
void pthread_setschedparam_wrapper(pthread_t thread_id, int policy, int prio)
{
	struct sched_param param = {
		.sched_priority = prio,
	};

	if (pthread_setschedparam(thread_id, policy, &param) != 0)
	{
		perror("pthread_setschedparam_wrapper");
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

MakeThread *g_threads_list[20];
int g_threads_num = 0;

MakeThread *find_make_thread(pthread_t thread)
{
	for (int i = 0; i < g_threads_num; i++) {
		if (g_threads_list[i]->get_thread_id() == thread) {
			return g_threads_list[i];
		}
	}

	return NULL;
}
void stop_all_threads()
{
	for (int i = 0; i < g_threads_num; i++) {
		g_threads_list[i]->stop_thread_by_main_thread();
	}
}
void cancel_all_threads()
{
	for (int i = 0; i < g_threads_num; i++) {
		if (pthread_cancel(g_threads_list[i]->get_thread_id()) != 0) {
			perror("cancel_all_threads");
		}
	}
}
void wait_all_threads_success_exit()
{
	for (int i = 0; i < g_threads_num; i++) {
		g_threads_list[i]->wait_stop_thread_success();
	}
}

MakeThread::MakeThread(const char *name, void *(*thread_func)(void *arg), void *arg)
	: _thread_func(thread_func)
	, _name(name)
	, _arg(arg)
{
	if (pthread_mutex_init(&_mutex, NULL) != 0) {
		exit_program();
	}
	if (pthread_cond_init(&_cond, NULL) != 0) {
		exit_program();
	}
}

void MakeThread::stop_thread_by_main_thread()
{
	pthread_mutex_lock(&_mutex);
	
	if (_stop_flag == 0) {
		_stop_flag = 1;
	} 

	pthread_mutex_unlock(&_mutex);

	pthread_setschedparam_wrapper(_thread_id, SCHED_RR, 99);
}
void MakeThread::stop_thread_by_that_thread()
{
	cond_signal_stop_thread();

	pthread_setschedparam_wrapper(_thread_id, SCHED_OTHER, 0);
	sleep(1000000); // =infi loop
}
void MakeThread::wait_stop_thread_success()
{
	pthread_mutex_lock(&_mutex);

	while (_stop_flag != 2) {
        pthread_cond_wait(&_cond, &_mutex); // TO DO: timeout
    }

	pthread_mutex_unlock(&_mutex);
}
void MakeThread::cond_signal_stop_thread() // when success exit, non-main thread invoke 
{
	pthread_mutex_lock(&_mutex);
	printf("cond_signal_stop_thread after lock\n");

	if (_stop_flag <= 1) {
		_stop_flag = 2;
	} else {
		printf("cond_signal_stop_thread bug, flag:%d\n", _stop_flag);
	}

	pthread_cond_signal(&_cond);

	pthread_mutex_unlock(&_mutex);
	printf("cond_signal_stop_thread after unlock\n");

}
int MakeThread::check_stop_flag() 
{
	int ret;

	pthread_mutex_lock(&_mutex);
	ret = _stop_flag;
	pthread_mutex_unlock(&_mutex);

	return ret;
}
void MakeThread::make_thread()
{
	if (unlikely(pthread_create(&_thread_id, NULL, _thread_func, _arg) != 0)) {
		perror("init_user_front ");
		exit_program();
	}
	pthread_setname_np(_thread_id, _name);	

	g_threads_list[g_threads_num] = this;
	g_threads_num++;
}
