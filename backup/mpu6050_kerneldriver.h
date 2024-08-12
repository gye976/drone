#ifndef MPU6050_H
#define MPU6050_H

#include <math.h>

#define WATERMARK				1
#define LENGTH					1
#define SAMPLING_FREQ   		500
#define DT						(1.0f / (float)SAMPLING_FREQ)

#define A_ACC	0.2
#define A_GYRO	0.4

#define CHAN_NUM 				6
#define TOTAL_BYTES_PER_CYCLE 	(2 * CHAN_NUM * WATERMARK)
#define RADIANS_TO_DEGREES 		180/3.14159

#define ALPHA                   0.96

#define AFS 	16384.0
#define FS  	131.0

enum enum_axis {
	X, Y, Z, NUM_AXIS
};

enum enum_angle {
	PITCH, ROLL, YAW, NUM_ANGLE
};
#define THROTTLE 3

enum enum_data {
    ACC, GYRO, NUM_DATA
};

struct Mpu6050_fd 
{
	Mpu6050_fd();
	
	void setup_buffer();
	void setup_sampling_frequency();
	void setup_gyro_buffer();
	void setup_acc_buffer();

	int _enable;
	int _data_available;
	int _watermark;
    int _length;
	int _sampling_frequency;
	int _cdev;
    int _scale[NUM_DATA];
    int _en[NUM_DATA][NUM_AXIS];
    int _bias[NUM_DATA][NUM_AXIS];
};

//'be16toh' only considers the bit, does not guarantee the sign.
static inline int16_t be16toh_s(int16_t be_val)
{
    return (int16_t)be16toh(be_val);
}

class Mpu6050 
{
	friend void exit_Mpu6050(Mpu6050 *mpu6050);
	
public:
	inline int read_mpu6050_buf()
	{
		return read(_fd._cdev, _buffer, TOTAL_BYTES_PER_CYCLE);
	}
	inline void read_raw(float acc[], float gyro[])
	{
		int16_t (*buf)[NUM_AXIS] = (int16_t (*)[NUM_AXIS])_buffer;

		for (int i = 0; i < NUM_AXIS; i++) {
			acc[i] = (float)(be16toh_s(buf[ACC][i])) / AFS;
			gyro[i] = (float)(be16toh_s(buf[GYRO][i])) / FS;
		}
	}
	inline void gryo_rate_bias(float gyro_rate[])
	{
		for (int i = 0; i < NUM_AXIS; i++) {
			gyro_rate[i] -= _gyro_rate_bias[i];
		}
	}
	inline void gyro_rate_to_angle(float gyro[],  float gyro_angle[])
	{
		for (int i = 0; i < NUM_AXIS; i++) {
			gyro_angle[i] = _angle[i] + gyro[i] * DT;
		}
	}
	inline void acc_angle_bias(float acc_angle[])
	{
		for (int i = 0; i < 2; i++) {
			acc_angle[i] -= _acc_angle_bias[i];
		}
	}
	inline void acc_to_angle(float acc[], float acc_angle[])
	{
		acc_angle[PITCH] = atan(acc[Y] / sqrt(pow(acc[X],2) + pow(acc[Z],2))) \
			* RADIANS_TO_DEGREES;

		acc_angle[ROLL] = atan(-1 * acc[X] / sqrt(pow(acc[Y], 2) + pow(acc[Z],2))) \
			* RADIANS_TO_DEGREES;
	}
	inline void do_EMA(float alpha, float *data, float new_data)
	{
		*data = alpha * new_data + (1.0f - alpha) * (*data);
	}
	inline void do_complementary_filter(float acc_angle[], float gyro_angle[])
	{
		for (int i = 0; i < 2; i++) {
			_angle[i] = (1.0 - ALPHA) * acc_angle[i] + ALPHA * gyro_angle[i];
		}
	}
	inline float *get_angle()
	{
		return _angle;
	}
	inline float *get_gyro_rate()
	{
		return _gyro_rate;
	}
	void print_data();

	Mpu6050();
	~Mpu6050();

	void set_sensitivity();
	void calibrate(int n);
	void calc_sensor();
	void set_irq_thread_high_priority();

private:
	Mpu6050_fd _fd;
	int16_t _buffer[CHAN_NUM * WATERMARK];

	float _gyro_rate_bias[NUM_AXIS];
	float _acc_angle_bias[NUM_AXIS];

	float _angle[NUM_AXIS] = { 0, };
	//float _angle_prev[NUM_AXIS] = { 0, };

	float _gyro_rate[NUM_AXIS] = { 0, };
	//float _gyro_vel_prev[NUM_AXIS] = { 0, };

	// //0.000133090 0.000266181 0.000532362 0.001064724
	// static float _s_fs[4] = {131.0, 65.5, 32.8, 16.4};
	// static int _s_fs_i = 3;

	// //0.000598, 0.001196, 0.002392, 0.004785
	// static float _s_afs[4] = {16384.0, 8192.0, 4096.0, 2048.0};
	// static int _s_afs_i = 0;

	int _rt_para_fd;
};

#endif 

