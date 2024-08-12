#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>

#include "globals.h"
#include "user_front.h"

#include "drone.h"
#include "pwm.h"

static void (*s_func)(Drone *drone);
 
static void (*s_cmd_func[20])(Drone *drone);
static const char *s_cmd_str[20];
static size_t s_cmd_str_len[20];
static int s_cmd_i = 0;

#define INIT_CMD(str, func_name) \
do { \
	s_cmd_str[s_cmd_i] = str; \
	s_cmd_str_len[s_cmd_i] = strlen(str); \
	s_cmd_func[s_cmd_i] = cmd_##func_name; \
	s_cmd_i++; \
} while(0)

static int n = 0;

void pause_user(Drone *drone)
{
	if (n == 0) {
		drone->lock_drone();
	}

	n++;
}
void resume_user(Drone *drone)
{
	if (n == 1) {
		drone->unlock_drone();
	}

	n--;
}

int scan_str(char *buf)
{
	int ret = 0;
	if (scanf("%s", buf) <= 0) {
		ret = -1;
	}

	return ret;
}
void cmd_pause(Drone *drone)
{
	static bool f = 0;

	if (f == 0) {
		printf("pause\n");
		pause_user(drone);
		f = 1;
	} else { 
		resume_user(drone);
		f = 0;
	}	
}

void cmd_debug_toggle(Drone *drone)
{
	(void)drone;

	g_debug_flag = !g_debug_flag;
	// sync?
}
void cmd_show_pid(Drone *drone)
{
	drone->print_parameter();
}
void cmd_throttle(Drone *drone)
{
	pause_user(drone);

	char buf[15];
	if (scan_str(buf) == -1) {
		printf("arg err\n");
		return;
	}

	int t = atoi(buf);
	if (t == 0) {
		printf("arg err\n");
		goto ERR;
	}
	printf("throttle: %d\n", t);

	drone->set_throttle(t);

ERR:
	resume_user(drone);
}

void cmd_pid(Drone *drone)
{
	pause_user(drone);

	drone->get_pid()->read_meta_file();

	// char buf[50];
	// if (scan_str(buf) == -1) {
	// 	printf("arg err\n");
	// 	return;
	// }
	// printf("arg: %s\n", buf);

	// char *saveptr;
	// char *token = strtok_r(buf, ",", &saveptr);

	// int para_num = 4; ////////////
	// float para[6] = { 0, };
	// int para_i = 0;
	// while (token != NULL) {
	// 	char *endptr;
	// 	float f = strtof(token, &endptr);

	// 	if (endptr == buf) {
	// 		goto ERR;
	// 	} else {
    //     	para[para_i++] = f;
	// 	}

    //     token = strtok_r(NULL, ",", &saveptr);
	// }

	// printf("para[]: ");
	// for (int i = 0; i < para_num; i++) {
	// 	printf("%f, ", para[i]);
	// }
	// printf("\n");

	// s_drone->get_pid()->update_para(para);

	resume_user(drone);

	return;

// ERR:

// 	resume_user();
// 	printf("arg err\n");
}

void cmd_debug_flag(Drone *drone)
{
	pause_user(drone);

	char buf[10];
	if (scan_str(buf) == -1) {
		printf("arg err\n");
		return;
	}

	if (strcmp(buf, "mpu6050") == 0) {
		drone->toggle_mpu6050_f();
	}
	if (strcmp(buf, "pid") == 0) {
		drone->toggle_pid_f();
	}
	if (strcmp(buf, "pwm") == 0) {
		drone->toggle_pwm_f();
	}

	resume_user(drone);
}

int parse_cmd(const char *str)
{
	for (int i = 0; i < s_cmd_i; i++) {
		bool flag = true;

		if (strlen(str) != s_cmd_str_len[i]) 
			continue;

		for (size_t j = 0; j < s_cmd_str_len[i]; j++) {
			if (str[j] != s_cmd_str[i][j]) {
				flag = false;
				break;
			} 
		}

		if (flag == true) {
			s_func = s_cmd_func[i];
			return 0;
		}
	}

	printf("no cmd, again\n");
	return -1;
}

void *get_user_input(void *drone)
{
	char buf[30];

	while (1) {
		if (scanf("%s", buf) <= 0) {
			printf("error cmd, again\n");
			continue;
		}
		//buf[29] = '\0';

		int ret = parse_cmd(buf);
		if (ret == -1) {
			continue;
		} else {
			s_func((Drone*)drone);
		}
	}

	exit_program();

	return (void*)0; // mean error
}

pthread_t make_user_front_thread(Drone *drone)
{
	pthread_t thread;

	INIT_CMD("p", pause);
	INIT_CMD("d", debug_flag);
	INIT_CMD("\n", debug_toggle);
	INIT_CMD("show", show_pid);
	INIT_CMD("t", throttle);
	INIT_CMD("pid", pid);

    if (pthread_create(&thread, NULL, get_user_input, drone) != 0) {
        perror("init_user_front ");
        exit_program();
    }
	pthread_setname_np(thread, "gye-user-input");

	return thread;
}
