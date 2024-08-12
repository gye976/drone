#define DEBUG

#include <stdio.h>
#include <sys/mman.h>
#include <pthread.h>

#include "globals.h"

#include "drone.h"
#include "timer.h"
#include "user_front.h"

Drone::Drone()
	: _pwm{Pwm(1), Pwm(2), Pwm(3), Pwm(4)}
{
	pthread_spin_init(&_spinlock, PTHREAD_PROCESS_PRIVATE);
}
Drone::~Drone()
{
	pthread_spin_destroy(&_spinlock);
}

void Drone::loop()
{
	float *angle = _mpu6050.get_angle();
	float *gyro_rate = _mpu6050.get_gyro_rate();

	//DtTrace dt(100000);

	// _mpu6050.set_irq_thread_high_priority();
	// _mpu6050.calibrate(30);

	//init_rt_sched();

	// char reg[1] = {ACCEL_XOUT_H};
	// int ret = write(_mpu6050._i2c_fd, reg, 1);
	// if (unlikely(ret != 1)) {
    //     perror("mpu read_raw err");
    //     exit_program();
    // }

	struct timespec ts_mono_cur;
	struct timespec ts_mono_prev;
	
	size_t cycle = 0;
	while (1)
	{
		update_new_mono_time(&ts_mono_prev);

		ADD_LOG(mpu6050, "%zu ", cycle);

		_mpu6050.do_mpu6050();

		lock_drone();
		// 목표각 설정.
		//	타겟을 롤, 피치는 0로 지정 (자세제어)
		//	쓰로틀,요우는 따로 지정 (일단 yaw는 항상 오차가 0이도록 설정)
	
		set_hovering();
		// update_target();

		update_pid_out(angle, gyro_rate);
		set_motor_speed();

		log_data();
		
		unlock_drone();

		cycle++;

		ADD_LOG(mpu6050, "\n");
		update_new_mono_time(&ts_mono_cur);
		float dt = timespec_to_float(&ts_mono_cur) - timespec_to_float(&ts_mono_prev);
		ADD_LOG(dt, "%f\n", dt);
	}
}
void Drone::set_hovering()
{
	_angle_target[PITCH] = 0.0f;
	_angle_target[ROLL] = 0.0f;
	_angle_target[YAW] = _mpu6050.get_angle()[YAW];

	//_throttle = 호버링 duty
}

void Drone::set_motor_speed()
{
	int pwm_in[4];

	pwm_in[FRONT_LEFT] = _throttle + _yaw + _roll + _pitch;
	pwm_in[FRONT_RIGHT] = _throttle - _yaw - _roll + _pitch;
	pwm_in[REAR_RIGHT] = _throttle + _yaw - _roll - _pitch;
	pwm_in[REAR_LEFT] = _throttle - _yaw + _roll - _pitch;

	dbg_print("pwm_in:\n");
	for (int i = 0; i < 4; i++) {
		if (pwm_in[i] < 0) {
			dbg_print("clipping to min, adjust throttle level.");
			pwm_in[i] = 0; 
		} else if (pwm_in[i] > PWM_MAX - PWM_MIN) {
			dbg_print("clipping to max, adjust throttle level.");
			pwm_in[i] = PWM_MAX - PWM_MIN;

			printf("max, maybe bug, exit\n");
			exit_program();;
		}

		dbg_print("  [%d]:%d \n", i, pwm_in[i]);
		_pwm[i].set_duty_cycle(pwm_in[i] + PWM_MIN);
	}
	dbg_print("\n");
}

void Drone::log_data()
{
	float *angle_cur = _mpu6050.get_angle();
	float *rate_cur = _mpu6050.get_gyro_rate();
	
	ADD_LOG_ARRAY(mpu6050, 3, "%f", _angle_target);
	ADD_LOG_ARRAY(mpu6050, 3, "%f", angle_cur);
	ADD_LOG_ARRAY(mpu6050, 3, "%f", _rate_target);
	ADD_LOG_ARRAY(mpu6050, 3, "%f", rate_cur);
	// if (_pr_mpu6050_f == true {
	// 	_mpu6050.print_data();
	// }

	// if (_pr_pid_f == true) {
	// 	_pid.print_data(PITCH);
	// }

	// if (_pr_pwm_f == true) {
	// 	printf("pwm, pitch:%d, roll:%d, yaw:%d\n", _pitch, _roll, _yaw);
	// 	printf("\n");	
	// }
}

void Drone::update_pid_out(float *angle, float *gyro_rate)
{
	float out_temp[3] = { 0, };
	int out[3] = {0};

	for (int i = 0; i < NUM_AXIS; i++) {
		_pid.do_cascade_pid(i, angle[i], gyro_rate[i], &_angle_target[i], &_rate_target[i], 
			&_angle_error[i], &_rate_error[i], &out_temp[i]);

		out[i] = (int)round(out_temp[i] * 1000);
	}
	
	_pitch = out[PITCH];
	_roll = out[ROLL];
	_yaw = out[YAW];
}

void Drone::print_parameter()
{
	_pid.print_parameter();

	printf("drone:print_parameter\n");
	printf("_throttle, %d\n\n", _throttle);
}

void* drone_loop(void *drone)
{
//	init_user_front(drone);

	((Drone*)drone)->loop();

	return NULL; // error
}

pthread_t make_drone_thread(Drone *drone)
{    
	pthread_t thread;
    if (pthread_create(&thread, NULL, drone_loop, drone) != 0) {
        perror("make_drone_thread ");
        exit_program();
    }
	pthread_setname_np(thread, "gye-drone");

    // cap_t caps = cap_get_proc();
    // if (caps == NULL) {
    //     perror("cap_get_proc");
    //     exit_program();
    // }

    // cap_value_t cap_values[] = { CAP_SYS_NICE, CAP_SYS_ADMIN };

    // if (cap_set_flag(caps, CAP_EFFECTIVE, 1, cap_values, CAP_SET) == -1) {
    //     perror("cap_set_flag(CAP_EFFECTIVE) failed");
    //     cap_free(caps);
    //     exit_program();
    // }

    // char *str = cap_to_text(caps, NULL);
    // printf("str:%s\n", str);

    // set_rtprio_limit();

    // int max = sched_get_priority_max(SCHED_DEADLINE);
    // if (max == -1) {
    //     perror("sched_get_priority_max\n");
    //     exit_program();
    // }
    //set_rt_deadline(thread, 150 * 1000, 2000 * 1000, 2000 * 1000);

	struct sched_param param = {
		.sched_priority = 99,
	};
	if (pthread_setschedparam(thread, SCHED_FIFO, &param) != 0)
	{
		perror("make_drone_thread");
		exit_program();
	}

	return thread;
}
