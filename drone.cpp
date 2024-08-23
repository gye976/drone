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
	, _pid_angle_PRY{Pid(loop_dt), Pid(loop_dt), Pid(loop_dt)}
	, _pid_gyro_PRY{Pid(loop_dt), Pid(loop_dt), Pid(loop_dt)}
	, _pid_altitude(loop_dt)
	, _pid_altitude_rate(loop_dt)
	, _loop_dt(loop_dt)
{
	read_and_update_pid_gain();

	print_pid_gain();

	_altitude_target = _hc_sr04.get_altitude();
	_altitude_input_prev = _altitude_target;
}
Drone::~Drone()
{
	print_pid_gain();
}

DtTrace dt_mpu6050("mpu6050");
DtTrace dt_socket_flush("socket_flush");

void Drone::print_pid_gain()
{
	printf("P gain:\n");
	_pid_angle_PRY[0].print_gain();
	_pid_gyro_PRY[0].print_gain();

	printf("\nR gain:\n");
	_pid_angle_PRY[1].print_gain();
	_pid_gyro_PRY[1].print_gain();

	printf("\nY gain:\n");
	_pid_angle_PRY[2].print_gain();
	_pid_gyro_PRY[2].print_gain();

	printf("\altitude gain:\n");
	_pid_altitude.print_gain();
	_pid_altitude.print_gain();
}

void Drone::check_loop_dt()
{
	update_new_mono_time(&_mono_loop_ts_cur);

	float dt_prev = timespec_to_double(&_mono_loop_ts_cur) - timespec_to_double(&_mono_loop_ts_prev);
	float dt = dt_prev;
	if (dt < _loop_dt) {
		//printf("delta dt:%f\n", dt - _loop_dt);
		struct timespec ts;
		ts.tv_sec = 0;
		ts.tv_nsec =  (_loop_dt - dt) * 1000 * 1000 * 1000;

		nanosleep(&ts, NULL);
		update_new_mono_time(&_mono_loop_ts_cur);
		dt = timespec_to_double(&_mono_loop_ts_cur) - timespec_to_double(&_mono_loop_ts_prev);
	}
	if (unlikely(dt > _loop_dt + 0.0001)) {
		printf("!!!!!!!!!!loop_dt over, dt:%f ---> %f, cycle:%zu\n", dt_prev, dt, _loop_cycle);
		exit_program();
	}

	_loop_cycle++;

	update_new_mono_time(&_mono_loop_ts_prev);
}

void Drone::loop()
{
	float *angle = _mpu6050.get_angle();
	float *gyro_rate = _mpu6050.get_gyro_rate();

	TRACE_FUNC_DT(dt_mpu6050, _mpu6050.do_mpu6050);

////////////////////////////////////////////////////////////////////////////////////////////////////////////

	set_hovering();
	// update_target();

	update_PRY_pid_out(angle, gyro_rate);

	// float altitude = _hc_sr04.get_altitude();
	// update_altitude_pid_out(altitude);

////////////////////////////////////////////////////////////////////////////////////////////////////////////

	check_loop_dt();

////////////////////////////////////////////////////////////////////////////////////////////////////////////

	set_motor_speed(); 

	log_data();

	// ADD_LOG_SOCKET(dt, "%f", dt);

	// dt_socket_flush.update_prev_time();
	// FLUSH_LOG_SOCKET();
	// dt_socket_flush.update_cur_time();
	// dt_socket_flush.update_data();
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

	// dbg_print("pwm_in:\n");
	for (int i = 0; i < 4; i++) {
		if (pwm_in[i] < 0) {
			// dbg_print("clipping to min, adjust throttle level.");
			pwm_in[i] = 0;
		} else if (pwm_in[i] > PWM_MAX - PWM_MIN) {
			// dbg_print("clipping to max, adjust throttle level.");
			pwm_in[i] = PWM_MAX - PWM_MIN;

			//printf("max, maybe bug, exit\n");
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

void Drone::print_parameter()
{
	// _pid.print_parameter();

	printf("drone:print_parameter\n");
	printf("_throttle, %d\n\n", _throttle);
}

void Drone::read_and_update_pid_gain()
{
    const char *filename = "pid_meta.txt";
    FILE *file = fopen(filename, "r");

    if (file == NULL) {
        perror("pid_meta.txt err");
        exit_program();
    }

    float f[16];
    size_t cnt = 0;

    char line[50];
    while (fgets(line, sizeof(line), file) != NULL) {
        if (line[0] == '\n') {
            continue;
         }

        char *token = strtok(line, "\t");
           while (token != NULL) {
            f[cnt++] = strtof(token, NULL);
             if (cnt == 16) {
                 goto END;
              }
             token = strtok(NULL, "\t");
        }
     }

END:
    fclose(file);

     if (cnt != 16) {
         fprintf(stderr, "pid_meta file is not enough\n");
        exit_program();
    }


    float PR_angle_gain[3] = { f[0], f[1], f[2] };
	float PR_angle_iterm_max = f[3];

    float PR_gyro_gain[3] = { f[4], f[5], f[6] };
	float PR_gyro_iterm_max = f[7];

    float throttle_altitude_gain[3] = { f[8], f[9], f[10] };
	float throttle_altitude_iterm_max = f[11];

    float throttle_altitude_rate_gain[3] = { f[12], f[13], f[14] };
	float throttle_altitude_rate_iterm_max = f[15];


    for (int i = 0; i < 2; i++) {
        _pid_angle_PRY[i].update_gain(PR_angle_gain, PR_angle_iterm_max);
        _pid_gyro_PRY[i].update_gain(PR_gyro_gain, PR_gyro_iterm_max);
    }

	_pid_altitude.update_gain(throttle_altitude_gain, throttle_altitude_iterm_max);
	_pid_altitude_rate.update_gain(throttle_altitude_rate_gain, throttle_altitude_rate_iterm_max);
}

void Drone::cascade_drone_pid(Pid* pid_angle, Pid* pid_gyro, float angle, float gyro_rate, float* out)
{
    float out_angle = 0.0f;
    float out_gyro_rate = 0.0f;

	float angle_d_err_per_dt = (-1) * gyro_rate;

    pid_angle->calc_pid_derr_per_dt(_angle_target[0], angle, angle_d_err_per_dt, &out_angle);

    float rate_target = out_angle;

    pid_gyro->calc_pid_no_overshoot(rate_target, gyro_rate, &out_gyro_rate);

    *out = out_gyro_rate;
}
void Drone::update_PRY_pid_out(float angle_input[], float gyro_rate_input[])
{
    float out_temp[3] = { 0, };
    int out[3] = {0};

    for (int i = 0; i < 3; i++) {
        cascade_drone_pid(&_pid_angle_PRY[i], &_pid_gyro_PRY[i], angle_input[i], gyro_rate_input[i], &out_temp[i]);
        out[i] = (int)round(out_temp[i] * 1000);
    }

	// if (_pid_angle_PRY[0].check_dterm() == 1) {
	// 	printf("angle P dterm over\n");
	// }
	// if (_pid_angle_PRY[1].check_dterm() == 1) {
	// 	printf("angle R dterm over\n");
	// }
	// if (_pid_gyro_PRY[0].check_dterm() == 1) {
	// 	printf("gyro P dterm over\n");
	// }
	// if (_pid_gyro_PRY[1].check_dterm() == 1) {
	// 	printf("gyro R dterm over\n");
	// }
	// printf("angle,\n");
	//_pid_angle_PRY[0].print_data();
	// printf("gyro,\n");
	//_pid_gyro_PRY[0].print_data();

    _pitch = out[PITCH];
    _roll = out[ROLL];
    _yaw = out[YAW];
}
void Drone::update_altitude_pid_out(float altitude_input)
{
    float out_altitude = 0.0f;

    _pid_altitude.calc_pid_no_overshoot(_altitude_target, altitude_input, &out_altitude);

	float altitude_velocity = (altitude_input - _altitude_input_prev) / (_loop_dt * 100);		// cm/s --> m/s
	float altitude_velocity_target = out_altitude / 100;

    float out_altitude_velocity = 0.0f;

    _pid_altitude_rate.calc_pid_no_overshoot(altitude_velocity_target, altitude_velocity, &out_altitude_velocity);

	// _pid_altitude.print_data();
	// _pid_altitude_rate.print_data();

    _throttle = (int)round(out_altitude_velocity * 1000);

	_altitude_input_prev = altitude_input;
}

void drone_do_once(void *arg)
{
	Drone *drone = (Drone*)arg;

	pthread_setschedparam_wrapper(pthread_self(), SCHED_RR, 99);
	//set_rt_deadline(0, 250 * 1000, 1000 * 1000, 1000 * 1000);

	for (int i = 0; i < 300; i++) {
		drone->get_mpu6050()->do_mpu6050();
	}

	update_new_mono_time(&drone->_mono_loop_ts_prev);
}
void drone_loop(void *arg)
{
	((Drone*)arg)->loop();
}

// void Drone::update_pid_out(float *angle, float *gyro_rate)
// {
// 	float out_temp[3] = { 0, };
// 	int out[3] = {0};

// 	for (int i = 0; i < NUM_AXIS; i++) {
// 		_pid.do_cascade_pid(i, angle[i], gyro_rate[i], &_angle_target[i], &_rate_target[i],
// 			&_angle_error[i], &_rate_error[i], &out_temp[i]);

// 		out[i] = (int)round(out_temp[i] * 1000);
// 	}

// 	_pitch = out[PITCH];
// 	_roll = out[ROLL];
// 	_yaw = out[YAW];
// }

