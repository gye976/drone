#ifndef GLOBALS_H
#define GLOBALS_H

#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

#define OPEN_FD(fd, path, ...)                  \
	do                                          \
	{                                           \
		fd = open(path, ##__VA_ARGS__);         \
                                                \
		if (unlikely(fd == -1))                 \
		{                                       \
			perror("open");                     \
			fprintf(stderr, "path:%s\n", path); \
			exit_program();                     \
		}                                       \
	} while (0)

#define DEFINE_EXIT(type) \
	type *__exit_##type##_instance_list[20]; \
	int __exit_##type##_instance_num = 0; \
\
	void __exit_##type##_global_func() \
	{ \
		printf("    @@@exit_"#type" entry\n"); \
		printf("    "#type" inst num:%d\n", __exit_##type##_instance_num); \
\
		for (int i__ = 0; i__ < __exit_##type##_instance_num; i__++) { \
			printf("        ===exit_"#type" instance%d entry\n", i__); \
\
			if (exit_##type(__exit_##type##_instance_list[i__]) != 0) { \
				printf("!!!!!!!!!!!!!!!!!!!inst%d exit ERROR\n", i__); \
			} \
\
			printf("        ===exit_"#type" instance%d exit\n\n\n", i__); \
		} \
\
		printf("    @@@exit_"#type" exit\n\n\n\n\n"); \
	} 

#define INIT_EXIT_IN_CTOR(type) \
do { \
	extern void (*g_exit_func_global_list[20])(); \
	extern int g_exit_func_global_num; \
	extern type *__exit_##type##_instance_list[20]; \
	extern int __exit_##type##_instance_num; \
	static bool __exit_flag = 0; \
\
	if (__exit_flag == 0) { \
		g_exit_func_global_list[g_exit_func_global_num++] = __exit_##type##_global_func; \
		__exit_flag = 1; \
	} \
	__exit_##type##_instance_list[__exit_##type##_instance_num++] = this; \
} while (0)
#define exit_program()                            \
	extern void __exit_program();                 \
	do                                            \
	{                                             \
		printf("file:%s, func:%s, line:%d\n\n",   \
			   __FILE__, __FUNCTION__, __LINE__); \
		__exit_program();                         \
	} while (0)


inline void open_paths_fd(int *fd, const char *root_path, const char *append_path, int flag)
{
	char path[50];
	if (unlikely(sprintf(path, "%s", root_path) < 0))
	{
		perror("open_paths_fd fail\n");
		exit_program();
	}
	strcat(path, append_path);
	OPEN_FD(*fd, path, flag);
}

void init_signal();
void init_rt_sched();

struct sched_attr
{
	uint32_t size;
	uint32_t sched_policy;
	uint64_t sched_flags;
	uint32_t sched_nice;
	uint32_t sched_priority;
	uint64_t sched_runtime;
	uint64_t sched_deadline;
	uint64_t sched_period;
};

int sched_setattr(pid_t pid, const struct sched_attr *attr, unsigned int flags);
int sched_getattr(pid_t pid, struct sched_attr *attr, unsigned int size, unsigned int flags);

void set_rt_deadline(pid_t pid, int runtime, int deadline, int period);
void set_rt_rr(pid_t pid, int prio);
int get_pid_str(const char *cmd, char pid[][30]);

#include "debug.h"
#include "log.h"

#endif
