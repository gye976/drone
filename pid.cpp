#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "globals.h"
#include "pid.h"
#include "pwm.h"
#include "drone.h"
#include "mpu6050.h"

int exit_Pid(Pid *pid)
{
	pid->print_parameter();
	pid->write_meta_file();

	return 0;
}
DEFINE_EXIT(Pid);

Pid::Pid(float dt)
	: _dt(dt)
{
	INIT_EXIT_IN_CTOR(Pid);

	read_meta_file();
}
void Pid::print_parameter()
{
	printf("angle, kp:%f, ki:%f, kd:%f\n", _gain[0][P], _gain[0][I], _gain[0][D]);
	printf("rate, kp:%f, ki:%f, kd:%f\n\n", _gain[1][P], _gain[1][I], _gain[1][D]);
}

void Pid::calc_angle_pid(int axis, float angle_in, float angle_target, float rate_in, float *out)
{
	float error = angle_target - angle_in;
	//float d_error = *error - *error_prev; cause overshoot.
	float d_error_per_dt = rate_in; 

	_term[0][axis][P] = _gain[0][P] * error;
	_term[0][axis][I] += _gain[0][I] * error * _dt; // to do: dt
	_term[0][axis][D] = (-1) * _gain[0][D] * d_error_per_dt; // to do: /dt?

	if (_term[0][axis][I] > _iterm_max[0]) {
		_term[0][axis][I] = _iterm_max[0];
	} else if (_term[0][axis][I] < -_iterm_max[0]) {
		_term[0][axis][I] = -_iterm_max[0];
	}

	(*out) = _term[0][axis][P] + _term[0][axis][I] + _term[0][axis][D];
}

#define max_iterm 500

void Pid::calc_rate_pid(int axis, float rate_in, float rate_target, float *out)
{
	float static rate_in_prev[3] = { 0, };

	float error = rate_target - rate_in;
	//float d_error = *error - *error_prev; cause overshoot.
	float d_error_per_dt = (rate_in - rate_in_prev[axis]) / _dt; 

	_term[1][axis][P] = _gain[1][P] * error;
	_term[1][axis][I] += _gain[1][I] * error * _dt; // to do: dt
	_term[1][axis][D] = (-1) * _gain[1][D] * d_error_per_dt;

	if (_term[1][axis][I] > _iterm_max[1]) {
		_term[1][axis][I] = _iterm_max[1];
	} else if (_term[1][axis][I] < -_iterm_max[1]) {
		_term[1][axis][I] = -_iterm_max[1];
	}
	(*out) = _term[1][axis][P] + _term[1][axis][I] + _term[1][axis][D];

	rate_in_prev[axis] = rate_in;
}

void Pid::do_cascade_pid(int axis, float angle_in, float rate_in, float *angle_target, float *rate_target, 
	float *angle_error, float *rate_error, float *out)
{
	float out_angle = 0.0f;

	calc_angle_pid(axis, angle_in, *angle_target, rate_in, &out_angle);

	*rate_target = out_angle;
	calc_rate_pid(axis, rate_in, *rate_target, out);

	*angle_error = *angle_target - angle_in;
	*rate_error = *rate_target - rate_in;

	log_data();
}

void Pid::log_data()
{
	ADD_LOG_ARRAY_SOCKET(pid, 3, "%f", _term[0][0]); // 각도  피치
	ADD_LOG_ARRAY_SOCKET(pid, 3, "%f", _term[0][1]);        // 롤 
	ADD_LOG_ARRAY_SOCKET(pid, 3, "%f", _term[0][2]);    	 // 요

	ADD_LOG_ARRAY_SOCKET(pid, 3, "%f", _term[1][0]); // 자이로
	ADD_LOG_ARRAY_SOCKET(pid, 3, "%f", _term[1][1]); 
	ADD_LOG_ARRAY_SOCKET(pid, 3, "%f", _term[1][2]); 

//	percent, 딱히 봐봤자 의미가없음

	// float angle_abs_sum[3];
	// float angle_term_per[3][3];

	// float rate_abs_sum[3];
	// float rate_term_per[3][3];

	// for (int i = 0; i < 3; i++) {
	// 	angle_abs_sum[i] = fabsf(_term[0][i][P]) + fabsf(_term[0][i][I]) + fabsf(_term[0][i][D]);
	// 	rate_abs_sum[i] = fabsf(_term[1][i][P]) + fabsf(_term[1][i][I]) + fabsf(_term[1][i][D]);	
		
	// 	for (int j = 0; j < 3; j++) {
	// 		angle_term_per[i][j] = fabsf(_term[0][i][j] * 100.0f / angle_abs_sum[i]);
	// 		rate_term_per[i][j] = fabsf(_term[1][i][j] * 100.0f / rate_abs_sum[i]);
	// 	}
	// }
	 
	//ADD_LOG_ARRAY(pid, 3, "%f", angle_term_per[0]); 
	//ADD_LOG_ARRAY(pid, 3, "%f", angle_term_per[1]); 
	//ADD_LOG_ARRAY(pid, 3, "%f", angle_term_per[2]); 

	//ADD_LOG_ARRAY(pid, 3, "%f", rate_term_per[0]); 
	//ADD_LOG_ARRAY(pid, 3, "%f", rate_term_per[1]); 
	//ADD_LOG_ARRAY(pid, 3, "%f", rate_term_per[2]); 
}

void Pid::read_meta_file()
{
    const char *filename = "pid_meta.txt"; 
    FILE *file = fopen(filename, "r");

    if (file == NULL) {
        perror("pid_meta.txt err");
        exit_program();
    }

    float f[8]; 
    size_t cnt = 0; 

    char line[50];
    while (fgets(line, sizeof(line), file) != NULL) {
		if (line[0] == '\n') {
			continue;
		}
	
        char *token = strtok(line, "\t"); 
        while (token != NULL) {
            f[cnt++] = strtof(token, NULL);
			if (cnt == 8) {
				goto END;
			}
            token = strtok(NULL, "\t"); 
        }
    }

END:
    fclose(file);

	if (cnt != 8) {
		fprintf(stderr, "pid_meta file is not enough\n");
		exit_program();
	}

	_gain[0][0] = f[0];
	_gain[0][1] = f[1];
	_gain[0][2] = f[2];

	_gain[1][0] = f[3];
	_gain[1][1] = f[4];
	_gain[1][2] = f[5];    
	
	_iterm_max[0] = f[6];
	_iterm_max[1] = f[7];
}

void Pid::write_meta_file()
{
    const char *filename = "pid_meta.txt"; 
    FILE *file = fopen(filename, "w");

    if (file == NULL) {
        perror("pid_meta.txt write err");
		return;
    }

	for (int i = 0; i < 2; i++) {
		int j = 0;
		for (j = 0; j < 2; j++) {
    		fprintf(file, "%f", _gain[i][j]); 
    		fprintf(file, "\t"); 
		}

		fprintf(file, "%f", _gain[i][j]); 
		fprintf(file, "\n"); 
	}

	fprintf(file, "\n"); 
	fprintf(file, "%f", _iterm_max[0]); 
    	fprintf(file, "\t"); 
	fprintf(file, "%f", _iterm_max[1]); 
}

