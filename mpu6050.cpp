#ifndef _GNU_SOURCE
    #define _GNU_SOURCE
#endif

#include <sys/ioctl.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <linux/i2c-dev.h>
#include <endian.h>

#include "globals.h"
#include "debug.h"
#include "mpu6050.h"
#include "timer.h"


// AVG(t) = B * Avg(t-1) + (1 - B)*V(t)

void exit_Mpu6050(Mpu6050 *mpu6050)
{
	(void)mpu6050;
}
INIT_EXIT(Mpu6050);

Mpu6050::Mpu6050()
{
	ADD_EXIT(Mpu6050);

	OPEN_FD(_i2c_fd, "/dev/i2c-2", O_RDWR);

    // Set the I2C address for the MPU6050
    if (ioctl(_i2c_fd, I2C_SLAVE, MPU6050_ADDR) < 0) {
        perror("Failed to connect to MPU6050 sensor");
        exit_program();
    }

    // Initialize MPU6050: Set Power Management 1 register to 0 to wake up the sensor
    char config[2] = {PWR_MGMT_1, 0};
    if (write(_i2c_fd, config, 2) != 2) {
        perror("Failed to initialize MPU6050");
        exit_program();
    }

    // Set DLPF: 21Hz bandwidth (Configuration 2)
    char dlpf_config[2] = {CONFIG, 0x04}; // 0x03 sets DLPF to Configuration 2
    if (write(_i2c_fd, dlpf_config, 2) != 2) {
        perror("Failed to set DLPF");
        exit_program();
    }

    // Set Gyroscope sensitivity: ±250°/s (Configuration 0)
    char gyro_config[2] = {GYRO_CONFIG, 0x00}; // 0x00 sets gyroscope sensitivity to ±250°/s
    if (write(_i2c_fd, gyro_config, 2) != 2) {
        perror("Failed to set gyroscope configuration");
        exit_program();
    }

    // Set Accelerometer sensitivity: ±2g (Configuration 0)
    char accel_config[2] = {ACCEL_CONFIG, 0x00}; // 0x00 sets accelerometer sensitivity to ±2g
    if (write(_i2c_fd, accel_config, 2) != 2) {
        perror("Failed to set accelerometer configuration");
        exit_program();
    }

    // Set sample rate divider for 1 kHz sample rate
    char smplrt_div_config[2] = {SMPLRT_DIV, 0}; // 0 sets sample rate to 1 kHz
    if (write(_i2c_fd, smplrt_div_config, 2) != 2) {
        perror("Failed to set sample rate divider");
        exit_program();
    }

	set_irq_thread_high_priority();
}

Mpu6050::~Mpu6050()
{
	exit_Mpu6050(this);
}

void Mpu6050::set_irq_thread_high_priority()
{

}

void Mpu6050::calibrate(int n)
{
	if (unlikely(n > 65500)) {
		exit_program();
	}

	float gyro[NUM_AXIS] = { 0, };
	float gyro_sum[NUM_AXIS] = { 0, };

	float acc[NUM_AXIS] = { 0, };
	float acc_angle[NUM_AXIS] = { 0, };
	float acc_angle_sum[NUM_AXIS] = { 0, };

	for (int i = 0; i < n; i++) {
		read_raw(acc, gyro);
		//printf("acc: %f, %f, %f\n", acc[0], acc[1], acc[2]);
		acc_to_angle(acc, acc_angle);
		//printf("acc angle: %f, %f, %f\n", acc_angle[0], acc_angle[1], acc_angle[2]);

		for (int j = 0; j < NUM_AXIS; j++) {
			acc_angle_sum[j] += acc_angle[j];
			gyro_sum[j] += gyro[j];
		}
	}

	for (int i = 0; i < NUM_AXIS; i++) {
		_acc_angle_bias[i] = acc_angle_sum[i] / n;
		_gyro_rate_bias[i] = gyro_sum[i] / n;
	}

	printf("acc bias: %f, %f, %f\n", _acc_angle_bias[0], _acc_angle_bias[1], _acc_angle_bias[2]);
	printf("gyro bias: %f, %f, %f\n", _gyro_rate_bias[0], _gyro_rate_bias[1], _gyro_rate_bias[2]);
	printf("\n");
}

//Trace dt(400);

void Mpu6050::read_raw(float acc[], float gyro[])
{
    char data[14];
    
    char reg[1] = {ACCEL_XOUT_H};
	int ret;
	
	ret = write(_i2c_fd, reg, 1);
	if (unlikely(ret != 1)) {
        perror("mpu read_raw err");
        exit_program();
    }
	
	ret = read(_i2c_fd, data, 14);
	if (unlikely(ret != 14)) {
        perror("mpu read_raw err");
        exit_program();
    }

    int16_t accel_x = (data[0] << 8) | data[1];
    int16_t accel_y = (data[2] << 8) | data[3];
    int16_t accel_z = (data[4] << 8) | data[5];

    int16_t gyro_x = (data[8] << 8) | data[9];
    int16_t gyro_y = (data[10] << 8) | data[11];
    int16_t gyro_z = (data[12] << 8) | data[13];

    acc[X] = (float)accel_x / ACC_FS_SENSITIVITY;
    acc[Y] = (float)accel_y / ACC_FS_SENSITIVITY;
    acc[Z] = (float)accel_z / ACC_FS_SENSITIVITY;

    gyro[X] = (float)gyro_x / GYRO_FS_SENSITIVITY;
    gyro[Y] = (float)gyro_y / GYRO_FS_SENSITIVITY;
    gyro[Z] = (float)gyro_z / GYRO_FS_SENSITIVITY;
}


void Mpu6050::do_mpu6050()
{
	//static float acc_angle_prev[NUM_AXIS] = { 0, };
	static float acc_angle_cur[NUM_AXIS] = { 0, };

	//for (int i = 0; i < NUM_AXIS; i++) {
	//	_angle_prev[i] = _angle_cur[i];
	//	acc_angle_prev[i] = acc_angle_cur[i];
	//	_gyro_vel_prev[i] = _gyro_vel_cur[i];
	//}

	float new_acc[NUM_AXIS] = { 0, };
	float new_acc_angle[NUM_AXIS] = { 0, };

	float new_gyro[NUM_AXIS] = { 0, };
	float new_gyro_angle[NUM_AXIS] = { 0, };

	read_raw(new_acc, new_gyro);

	gryo_rate_bias(new_gyro);

	for (int i = 0; i < NUM_AXIS; i++) {
		do_EMA(A_GYRO, &_gyro_rate[i], new_gyro[i]);
	}

	gyro_rate_to_angle(_gyro_rate, new_gyro_angle);

	acc_to_angle(new_acc, new_acc_angle);

	acc_angle_bias(new_acc_angle);

	for (int i = 0; i < 2; i++) {
		do_EMA(A_ACC, &acc_angle_cur[i], new_acc_angle[i]);
	}
	do_complementary_filter(acc_angle_cur, new_gyro_angle);

	_angle[YAW] = new_gyro_angle[YAW];

	//printf("_gyro_vel_cur: %f, %f, %f\n", _gyro_vel_cur[0], _gyro_vel_cur[1], _gyro_vel_cur[2]);
	//printf("acc angle: %f, %f, %f\n", acc_angle_cur[0], acc_angle_cur[1], acc_angle_cur[2]);
	//printf("gyro_angle: %f, %f, %f\n", new_gyro_angle[0],  new_gyro_angle[1],  new_gyro_angle[2]);
	//printf("_angle_cur: %f, %f, %f\n", _angle_cur[0],  _angle_cur[1],  _angle_cur[2]);
	//printf("\n");
}

void Mpu6050::print_data()
{
	printf("Pitch,	angle:%f, gyro:%f\n", _angle[PITCH], _gyro_rate[PITCH]);
	printf("Roll,	angle:%f, gyro:%f\n", _angle[ROLL], _gyro_rate[ROLL]);
	printf("Yaw,	angle:%f, gyro:%f\n\n", _angle[YAW], _gyro_rate[YAW]);
}