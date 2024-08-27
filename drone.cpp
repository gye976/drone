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
	//, _pid_altitude_velocity(loop_dt)
	, _loop_dt(loop_dt)
{
	read_and_update_pid_gain();

	// _hc_sr04.init_altitude(300);
	// _altitude_target = _hc_sr04.get_altitude();
	// _altitude_input_prev = _altitude_target;

	//set_iterm_default();
	print_pid_gain();
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

	printf("\naltitude gain:\n");
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

	update_PR_pid_out(angle, gyro_rate);
	update_Y_pid_out(gyro_rate[YAW]);

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
	//_angle_target[YAW] = _mpu6050.get_angle()[YAW];

	//_throttle = 호버링 duty
}

void Drone::set_motor_speed()
{
	int pwm_in[4];

	int throttle_sum = _throttle_basic + _throttle;
//	printf("throttle: %d, %d\n", _throttle_basic, _throttle);

	if (throttle_sum < 0) {
		throttle_sum = 0;
	} 

	pwm_in[FRONT_LEFT] = throttle_sum + _yaw + _roll + _pitch;
	pwm_in[FRONT_RIGHT] = throttle_sum - _yaw - _roll + _pitch;
	pwm_in[REAR_RIGHT] = throttle_sum + _yaw - _roll - _pitch;
	pwm_in[REAR_LEFT] = throttle_sum - _yaw + _roll - _pitch;

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
	// ADD_LOG_ARRAY_SOCKET(mpu6050, 3, "%f", _rate_target);
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

    float f[30];
    size_t cnt = 0;

    char line[50];
    while (fgets(line, sizeof(line), file) != NULL) {
        if (line[0] == '\n') {
            continue;
         }

        char *token = strtok(line, "\t");
           while (token != NULL) {
            f[cnt++] = strtof(token, NULL);
             if (cnt == 30) {
                 goto END;
            	}
             token = strtok(NULL, "\t");
        }
     }

END:
    fclose(file);

	printf("cnt:%ld\n", cnt);
	if (cnt < 30) {
         fprintf(stderr, "pid_meta file is not enough\n");
        exit_program();
    }


	for (int i = 0; i < 2; i++) {
		int idx = i * 10;

		float angle_gain[3] = { f[idx], f[idx + 1], f[idx + 2] };
		float angle_iterm_default = f[idx + 3];
		float angle_iterm_max = f[idx + 4];
	
    	float gyro_gain[3] = { f[idx + 5], f[idx + 6], f[idx + 7] };
		float gyro_iterm_default = f[idx + 8];
		float gyro_iterm_max = f[idx + 9];
	
        _pid_angle_PRY[i].update_gain(angle_gain, angle_iterm_default, angle_iterm_max);
        _pid_gyro_PRY[i].update_gain(gyro_gain, gyro_iterm_default, gyro_iterm_max);
	}



    float Y_gyro_gain[3] = { f[20], f[21], f[22] };
	float Y_gyro_iterm_default = f[23];
	float Y_gyro_iterm_max = f[24];

	_pid_gyro_PRY[YAW].update_gain(Y_gyro_gain, Y_gyro_iterm_default, Y_gyro_iterm_max);



    float throttle_altitude_gain[3] = { f[25], f[26], f[27] };
	float throttle_altitude_iterm_default = f[28];
	float throttle_altitude_iterm_max = f[29];

    // float throttle_altitude_velocity_gain[3] = { f[28], f[29], f[30] };
	// float throttle_altitude_velocity_iterm_max = f[31];

	_pid_altitude.update_gain(throttle_altitude_gain, throttle_altitude_iterm_default, throttle_altitude_iterm_max);
	//_pid_altitude_velocity.update_gain(throttle_altitude_velocity_gain, throttle_altitude_velocity_iterm_max);
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
void Drone::update_PR_pid_out(float angle_input[], float gyro_rate_input[])
{
    float out_temp[2] = { 0, };
    int out[2] = {0};

    for (int i = 0; i < 2; i++) {
        cascade_drone_pid(&_pid_angle_PRY[i], &_pid_gyro_PRY[i], angle_input[i], gyro_rate_input[i], &out_temp[i]);
        out[i] = (int)round(out_temp[i] * 1000);
    }

    _pitch = out[PITCH];
    _roll = out[ROLL];
}
void Drone::update_Y_pid_out(float gyro_rate_input)
{
    float out_gyro_rate = 0.0f;

    _pid_gyro_PRY[YAW].calc_pid_no_overshoot(0.0f, gyro_rate_input, &out_gyro_rate);

	int out = (int)round(out_gyro_rate * 1000);

    _yaw = out;
}
// void Drone::aa(float altitude_input)
// {
// 	int cycle = 0;

// 	float altitude_velocity;
// 	if (altitude_input == _altitude_input_prev) {
// 		altitude_velocity = _altitude_velocity_prev;
// 	} else {
// 		altitude_velocity = (altitude_input - _altitude_input_prev) / (_loop_dt);

// 		if (altitude_velocity > )
// 	}

// }

void Drone::update_altitude_pid_out(float altitude_input)
{
#if 1
    float out_altitude = 0.0f;

    _pid_altitude.calc_pid_no_overshoot(_altitude_target, altitude_input, &out_altitude);
	_pid_altitude.print_data();

	int temp = (int)round(out_altitude * 1000);

    _throttle = temp;

	_altitude_input_prev = altitude_input;

	// float err = _altitude_target - altitude_input;
	// float a = ceil ((sigmoid(err) - 1.0 / 2.0) * 100) / 100; 
	// _throttle_basic += 100 * a; 

	printf("alti %f,%f, throttle:%d, %d \n", _altitude_target , altitude_input, _throttle, _throttle_basic);

#else
    float out_altitude = 0.0f;

    _pid_altitude.calc_pid_no_overshoot(_altitude_target, altitude_input, &out_altitude);


	float altitude_velocity_target = out_altitude;

	float altitude_velocity;
	if (altitude_input == _altitude_input_prev) {
		altitude_velocity = _altitude_velocity_prev;
	} else {
		altitude_velocity = (altitude_input - _altitude_input_prev) / (_loop_dt);
	}

    float out_altitude_velocity = 0.0f;

    _pid_altitude_velocity.calc_pid_no_overshoot(altitude_velocity_target, altitude_velocity, &out_altitude_velocity);

	// printf("velocity target/input:%f, %f\n %f\n", altitude_velocity_target, altitude_velocity, out_altitude_velocity);

	// printf("alti, alti_rate:\n");
	//_pid_altitude.print_data();
	_pid_altitude_velocity.print_data();

	int temp = (int)round(out_altitude_velocity / 100);
	temp *= 100;

    _throttle = temp;

	printf("cur altitude: %f, target:%f, t: %d\n", altitude_input, _altitude_target, _throttle);
	_altitude_input_prev = altitude_input;
	_altitude_velocity_prev = altitude_velocity;
#endif
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

