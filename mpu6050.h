#ifndef MPU6050_H
#define MPU6050_H

#include <math.h>

#define MPU6050_ADDR 0x68 // MPU6050 I2C address
#define PWR_MGMT_1 0x6B // Power Management 1 register
#define CONFIG 0x1A // DLPF Configuration register
#define SMPLRT_DIV 0x19

#define USER_CTRL      0x6A
#define FIFO_EN        0x23
#define FIFO_R_W       0x74

#define FIFO_COUNT_H 0x72
#define FIFO_COUNT_L 0x73

#define GYRO_CONFIG 0x1B // Gyroscope Configuration register
#define ACCEL_CONFIG 0x1C // Accelerometer Configuration register
#define ACCEL_XOUT_H 0x3B // Accelerometer X-axis high byte
#define ACCEL_XOUT_L 0x3C // Accelerometer X-axis low byte
#define GYRO_XOUT_H 0x43 // Gyroscope X-axis high byte
#define GYRO_XOUT_L 0x44 // Gyroscope X-axis low byte

#define RADIANS_TO_DEGREES 		((float)(180/3.14159))

#define ACC_FS_SENSITIVITY					16384.0f
#define GYRO_FS_SENSITIVITY					 131.0f

#define DT 0.002f  //2ms

#define ALPHA                   0.96f
#define A_ACC	0.2f
#define A_GYRO	0.4f

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

//'be16toh' only considers the bit, does not guarantee the sign.
static inline int16_t be16toh_s(int16_t be_val)
{
    return (int16_t)be16toh(be_val);
}

class Mpu6050 
{
	friend int exit_Mpu6050(Mpu6050 *mpu6050);
	
public:
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

	Mpu6050();
	~Mpu6050();

	void do_mpu6050();
	void print_data();
	void calibrate(int n, float acc_angle_mean[], float gyro_rate_mean[]);
private:
	void read_raw(float acc[], float gyro[]);
	void set_irq_thread_high_priority();

	float _gyro_rate_bias[NUM_AXIS] = { -1.12, 0.345, 0.85 };
	float _acc_angle_bias[NUM_AXIS] = { 0.75, -0.13, 0.0 }; //mounting bias

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
	int _i2c_fd;
};

#endif 

