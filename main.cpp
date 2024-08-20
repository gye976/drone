#include <stdio.h>
#include <sys/mman.h>

#include "globals.h"
#include "mpu6050.h"
#include "pid.h"
#include "timer.h"
#include "user_front.h"
#include "drone.h"


Drone g_drone(0.0015);

DEFINE_THREAD(drone, drone_loop, drone_do_once, &g_drone);

DEFINE_THREAD(user, user_loop, user_do_once, &g_drone);

// DEFINE_THREAD2(drone, drone_loop, init_drone, (void*)&g_drone);
#ifndef NO_SOCKET
	MakeThread socket_thread("gye-socket", send_socket_loop, NULL, &g_drone);
#endif

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

	pthread_t threads[10];

	//Mpu6050 *mpu6050 = g_drone.get_mpu6050();
	
	threads[0] = g_user_thread.make_thread();
	threads[1] = g_drone_thread.make_thread();
// 	threads[2] = make_drone_thread(&drone);

// 	threads[3] = make_mpu6050_read_hwfifo_thread(mpu6050);

	if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1) {
		perror("mlockall\n");
		exit_program();
	}

	extern int g_threads_num;
	for (int i = 0; i < g_threads_num; i++) {
        	if (pthread_join(threads[i], NULL) != 0) {
				fprintf(stderr, "threads[%d], ", i);
				perror("Failed to join thread");
	        }
	}

	///////////////////////////////

	exit_program();
	return 0;
}
