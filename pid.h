#ifndef PID_H
#define PID_H

#include "mpu6050.h" 

enum e_pid {
	P,
	I,
	D
};

class Pid
{
public:
	Pid(float dt);
	
	void print_parameter();
	void print_data(int axis);
	void read_meta_file();
	void write_meta_file();
	void log_data();
	inline void update_para(float para[])
	{
		_gain[0][0] = para[0];
		_gain[0][1] = para[1];
		_gain[0][2] = para[2];

		_gain[1][0] = para[3];
		_gain[1][1] = para[4];
		_gain[1][2] = para[5];
	}

	void calc_angle_pid(int axis, float angle_in, float angle_target, float rate_in, float *out);
	void calc_rate_pid(int axis, float rate_in, float rate_target, float *out);
	void do_cascade_pid(int axis, float angle_in, float rate_in, float *angle_target, float *rate_target, 
		float *angle_error, float *rate_error, float *out);

private:
	float _gain[2][3];
	float _iterm_max[2];

	float _term[2][NUM_AXIS][3] = { 0, };
	 // 2:angle,rate,  3:PID

	float _dt;
};

#endif 

