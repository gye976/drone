#include <stdio.h>
#include <sys/mman.h>

#include "globals.h"
#include "mpu6050.h"
#include "pid.h"
#include "timer.h"
#include "user_front.h"
#include "drone.h"

#define THREADS_NUM	4	 
Drone g_drone(0.0015);
pthread_t g_threads[THREADS_NUM];

int main() 
{
	if (geteuid() != 0) {
        	fprintf(stderr, "This program requires root privileges.\n");
        	exit_program();
	}

	int rt_fd;
	OPEN_FD(rt_fd, "/proc/sys/kernel/sched_rt_runtime_us", O_RDWR);
	if (write(rt_fd, "-1", strlen("-1")) == -1) {
		perror("write -1 to rt_sched error\n");
		exit_program();
	}

	init_signal();	
		
	////////////////////////////////

	Mpu6050 *mpu6050 = g_drone.get_mpu6050();

	g_threads[0] = make_user_front_thread(&g_drone);
	g_threads[1] = make_socket_thread(&g_log_socket_manager); 
	g_threads[2] = make_mpu6050_read_hwfifo_thread(mpu6050);
	g_threads[3] = make_drone_thread(&g_drone);


	if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1) {
		perror("mlockall\n");
		exit_program();
	}

	for (int i = 0; i < THREADS_NUM; i++) {
        	if (pthread_join(g_threads[i], NULL) != 0) {
				fprintf(stderr, "threads[%d], ", i);
				perror("Failed to join thread");
	        }
	}

	///////////////////////////////

	exit_program();
	return 0;
}
