#define DEBUG

#include <stdio.h>
#include <sys/mman.h>
#include <pthread.h>

#include "globals.h"

#include "drone.h"
#include "timer.h"
#include "user_front.h"

Drone::Drone(float loop_dt)
	: _mpu6050(loop_dt)
	, _pwm{Pwm(1), Pwm(2), Pwm(3), Pwm(4)}
	, _pid(loop_dt)
	, _loop_dt(loop_dt)
{
	pthread_mutex_init(&_mutex, NULL);
}
Drone::~Drone()
{
	pthread_mutex_destroy(&_mutex);
}

DtTrace dt_mpu6050("mpu6050");
DtTrace dt_socket_flush("socket_flush");

void Drone::loop_logic()
{
	float *angle = _mpu6050.get_angle();
	float *gyro_rate = _mpu6050.get_gyro_rate();


	TRACE_FUNC_DT(dt_mpu6050, _mpu6050.do_mpu6050);

		// int ret = trylock_drone();
		// if (ret == 0) {
		// 	// 목표각 설정.
		// 	//	타겟을 롤, 피치는 0로 지정 (자세제어)
		// 	//	쓰로틀,요우는 따로 지정 (일단 yaw는 항상 오차가 0이도록 설정)

		// 	set_hovering();
		// 	// update_target();

		// 	log_data();

		// 	unlock_drone();
		// }

	set_hovering();
	// update_target();

	log_data();

	update_pid_out(angle, gyro_rate);

	set_motor_speed();
}
void Drone::loop()
{
	static size_t cycle = 0;
	static size_t over_num = 0;

	// usleep(10000);

	static struct timespec mono_loop_ts_cur;
	static struct timespec mono_loop_ts_prev;

	// struct timespec loop_start_ts;
	// loop_start_ts.tv_sec = 0;
	// loop_start_ts.tv_nsec = _loop_dt * 500 * 1000 * 1000;

	// for (int i = 0; i < 500; i++) {
	// 	loop_logic();
	// }

	update_new_mono_time(&mono_loop_ts_prev);

		loop_logic();
		
		update_new_mono_time(&mono_loop_ts_cur);
		float dt = timespec_to_double(&mono_loop_ts_cur) - timespec_to_double(&mono_loop_ts_prev);
		float dt_sleep = dt;
		if (unlikely(dt > _loop_dt)) {
			printf("!!!!!!!!!!loop_dt over, dt:%f\n", dt);
			over_num++;
			exit_program();
		} else {
			struct timespec ts;
			ts.tv_sec = 0;
			ts.tv_nsec =  (_loop_dt - dt) * 1000 * 1000 * 1000;

			nanosleep(&ts, NULL);
			update_new_mono_time(&mono_loop_ts_cur);
			dt_sleep = timespec_to_double(&mono_loop_ts_cur) - timespec_to_double(&mono_loop_ts_prev);
		}
		//update_new_mono_time(&mono_loop_ts_prev);

		printf("%f, %f\n", dt, dt_sleep);
		ADD_LOG_SOCKET(dt, "%f", dt);
		cycle++;

		// dt_socket_flush.update_prev_time();
		// FLUSH_LOG_SOCKET();
		// dt_socket_flush.update_cur_time();
		// dt_socket_flush.update_data();
}
void Drone::set_hovering()
{
	_angle_target[PITCH] = 0.0f;
	_angle_target[ROLL] = 0.0f;
	_angle_target[YAW] = 0.0f;//_mpu6050.get_angle()[YAW];

	//_throttle = 호버링 duty
}

void Drone::set_motor_speed()
{
	int pwm_in[4];

	pwm_in[FRONT_LEFT] = _throttle + _yaw + _roll + _pitch;
	pwm_in[FRONT_RIGHT] = _throttle - _yaw - _roll + _pitch;
	pwm_in[REAR_RIGHT] = _throttle + _yaw - _roll - _pitch;
	pwm_in[REAR_LEFT] = _throttle - _yaw + _roll - _pitch;

	// dbg_print("pwm_in:\n");
	for (int i = 0; i < 4; i++) {
		if (pwm_in[i] < 0) {
			// dbg_print("clipping to min, adjust throttle level.");
			pwm_in[i] = 0; 
		} else if (pwm_in[i] > PWM_MAX - PWM_MIN) {
			// dbg_print("clipping to max, adjust throttle level.");
			pwm_in[i] = PWM_MAX - PWM_MIN;

			printf("max, maybe bug, exit\n");
			//exit_program();
		}

		// dbg_print("  [%d]:%d \n", i, pwm_in[i]);
		_pwm[i].set_duty_cycle(pwm_in[i] + PWM_MIN);
	}
	// dbg_print("\n");
}

void Drone::log_data()
{
	float *angle_cur = _mpu6050.get_angle();
	float *rate_cur = _mpu6050.get_gyro_rate();
	
	ADD_LOG_ARRAY_SOCKET(mpu6050, 3, "%f", _angle_target);
	ADD_LOG_ARRAY_SOCKET(mpu6050, 3, "%f", angle_cur);
	ADD_LOG_ARRAY_SOCKET(mpu6050, 3, "%f", _rate_target);
	ADD_LOG_ARRAY_SOCKET(mpu6050, 3, "%f", rate_cur);
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

// void* drone_loop(void *drone)
// {
// 		/* pid=0 (current thread) */
// 	set_rt_deadline(0, 300 * 1000, 1000 * 1000, 1000 * 1000);

// 	((Drone*)drone)->loop();

// 	return NULL; // error
// }

void drone_do_once(void *drone)
{
	set_rt_deadline(0, 300 * 1000, 1000 * 1000, 1000 * 1000);

	for (int i = 0; i < 10; i++) {
		((Drone*)drone)->get_mpu6050()->do_mpu6050();
	}
}
void drone_loop(void *drone)
{
	((Drone*)drone)->loop();
}
