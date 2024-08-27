#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <poll.h>

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

//static int n = 0;

// void pause_user(Drone *drone)
// {
// 	if (n == 0) {
// 		drone->lock_drone();
// 	}

// 	n++;
// }
// void resume_user(Drone *drone)
// {
// 	if (n == 1) {
// 		drone->unlock_drone();
// 	}

// 	n--;
// }

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
	(void)drone;

	static bool f = 0;

	if (f == 0) {
		printf("pause\n");
		f = 1;
	} else { 
		f = 0;
	}	
}

void cmd_debug_toggle(Drone *drone)
{
	(void)drone;

	// g_debug_flag = !g_debug_flag;
	// sync?
}
void cmd_show_pid(Drone *drone)
{
	drone->print_parameter();
}

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>

int kbhit(void) {
    struct termios oldt, newt;
    int ch;
    int oldf;

    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);

    if(ch != EOF) {
        ungetc(ch, stdin);
        return 1;
    }

    return 0;
}

#define THROTTLE_UNIT	1000
void cmd_throttle(Drone *drone)
{
	printf("enter throttle mode\n");
	int t = 0;
	char buf[20];

    while (1) {
        if (kbhit()) {
            char ch = getchar();

			switch (ch) {
			case 's':
				scanf("%s", buf);
				t = atoi(buf);
				break;
			case '8': //up
				t += THROTTLE_UNIT;
				break;
			case '2': //donw
				t -= THROTTLE_UNIT;
				break;
			case '0':
				printf("exit throttle mode\n");
				return;
			default:
				break;
			}
        }

		printf("throttle: %d\n", t);

		drone->set_throttle(t);
    }
}
void cmd_set_throttle(Drone *drone)
{
	char buf[15];
	if (scan_str(buf) == -1) {
		printf("arg err\n");
		return;
	}

	int t = atoi(buf);
	if (t == 0) {
		printf("arg err\n");
		return;
	}
	printf("set throttle: %d\n", t);

	drone->set_throttle(t);
}

void cmd_up_altitude(Drone *drone)
{
	char buf[15];
	if (scan_str(buf) == -1) {
		printf("arg err\n");
		return;
	}

	int altitude = atoi(buf);
	if (altitude == 0) {
		printf("arg err\n");
		return;
	}
	printf("up altitude: %d\n", altitude);

	drone->up_altitude(altitude);
}
void cmd_down_altitude(Drone *drone)
{
	char buf[15];
	if (scan_str(buf) == -1) {
		printf("arg err\n");
		return;
	}

	int altitude = atoi(buf);
	if (altitude == 0) {
		printf("arg err\n");
		return;
	}
	printf("down altitude: %d\n", altitude);

	drone->down_altitude(altitude);
}
void cmd_update_altitude_target(Drone *drone)
{
	printf("update altitude target\n");

	drone->update_altitude_target();
}
// void cmd_pid(Drone *drone)
// {
// 	// pause_user(drone);

// 	// drone->get_pid()->read_meta_file();
// 	// drone->get_pid()->print_parameter();

// 	// resume_user(drone);

// 	return;

// // ERR:

// // 	resume_user();
// // 	printf("arg err\n");
// }

void cmd_debug_flag(Drone *drone)
{
	(void)drone;
	// pause_user(drone);

	char buf[10];
	if (scan_str(buf) == -1) {
		printf("arg err\n");
		return;
	}

	// if (strcmp(buf, "mpu6050") == 0) {
	// 	drone->toggle_mpu6050_f();
	// }
	// if (strcmp(buf, "pid") == 0) {
	// 	drone->toggle_pid_f();
	// }
	// if (strcmp(buf, "pwm") == 0) {
	// 	drone->toggle_pwm_f();
	// }

	// resume_user(drone);
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

void user_do_once(void *drone)
{
	(void)drone;

	pthread_setschedparam_wrapper(pthread_self(), SCHED_RR, 50);

	INIT_CMD("show", show_pid);
	INIT_CMD("up", up_altitude);
	INIT_CMD("down", down_altitude);
	INIT_CMD("h", update_altitude_target);
	INIT_CMD("t", throttle);
	INIT_CMD("s", set_throttle);
	//INIT_CMD("pid", pid);
}

#define TIMEOUT 	2 //ms
void user_loop(void *drone)
{	
	struct pollfd fds[1];
    int ret;
	char buf[30];

    // 파일 디스크립터 설정
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN; // 읽기 가능 이벤트

	ret = poll(fds, 1, TIMEOUT);

  	if (unlikely(ret == -1)) {
        perror("select");
        exit_program();
    } else if (ret == 0) {
		return;
    } else {
        if (fds[0].revents & POLLIN) {
			if (scanf("%s", buf) <= 0) {
				printf("error cmd, again\n");
				return;
			}
		}
    }

	//buf[29] = '\0';

	ret = parse_cmd(buf);
	if (ret == -1) {
		return;
	} else {
		s_func((Drone*)drone);
	}
}
