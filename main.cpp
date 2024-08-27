#include <stdio.h>
#include <sys/mman.h>

#include "globals.h"
#include "mpu6050.h"
#include "pid.h"
#include "timer.h"
#include "user_front.h"
#include "drone.h"


Drone g_drone(0.002);

DEFINE_THREAD_WITH_INIT(drone, &g_drone);

DEFINE_THREAD_WITH_INIT(user, &g_drone);

//DEFINE_THREAD_WITH_INIT(hc_sr04, g_drone.get_hc_sr04());

#ifndef NO_SOCKET
	DEFINE_THREAD(socket, send_socket_loop, send_socket_do_once, &g_log_socket_manager);
#endif


pthread_t g_main_thread_id;

int main() 
{
	int rt_fd;
	OPEN_FD(rt_fd, "/proc/sys/kernel/sched_rt_runtime_us", O_RDWR);
	if (write(rt_fd, "-1", strlen("-1")) == -1) {
		perror("write -1 to rt_sched error\n");
		exit_program();
	}

	g_main_thread_id = pthread_self();
	printf("main thread id: %zu\n", g_main_thread_id);

	sched_setscheduler_wrapper(0, SCHED_RR, 99);

	init_signal();	
		
	////////////////////////////////


	//Mpu6050 *mpu6050 = g_drone.get_mpu6050();
	
	g_user_thread.make_thread();

	//g_hc_sr04_thread.make_thread();	

	g_drone_thread.make_thread();	

#ifndef NO_SOCKET
	g_socket_thread.make_thread();
#endif

// 	threads[3] = make_mpu6050_read_hwfifo_thread(mpu6050);

	if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1) {
		perror("mlockall\n");
		exit_program();
	}

	extern pthread_mutex_t g_exit_mutex;
	extern pthread_cond_t g_exit_cond;
	extern bool g_non_main_thread_request_flag;

	pthread_mutex_lock(&g_exit_mutex);
	while (g_non_main_thread_request_flag == 0) {
        pthread_cond_wait(&g_exit_cond, &g_exit_mutex);
    }
	pthread_mutex_unlock(&g_exit_mutex);

	exit_program(); // end

	while(1) {
		printf("if here, error\n");
	}
	
	return 0;
}
