#ifndef _GNU_SOURCE
    #define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

// #include <sched.h>
// #include <sys/syscall.h>
#include <endian.h>

#include "globals.h"
#include "debug.h"
#include "mpu6050.h"

				///sys/devices/platform/soc@3000000/5002400.twi/i2c-1/1-0068/iio:device1
#define IIO_DEVICE_PATH "/sys/bus/iio/devices/iio:device0"
#define CDEV_PATH "/dev/iio:device0"

#define ENABLE_PATH					IIO_DEVICE_PATH "/buffer/enable"
#define LENGTH_PATH					IIO_DEVICE_PATH "/buffer/length"
#define WATERMARK_PATH				IIO_DEVICE_PATH "/buffer/watermark"

#define SAMPLING_FREQUENCY_PATH		IIO_DEVICE_PATH	"/sampling_frequency"

#define ACCEL_X_EN_PATH 			IIO_DEVICE_PATH "/scan_elements/in_accel_x_en"
#define ACCEL_Y_EN_PATH 			IIO_DEVICE_PATH "/scan_elements/in_accel_y_en"
#define ACCEL_Z_EN_PATH 			IIO_DEVICE_PATH "/scan_elements/in_accel_z_en"
#define ANGLVEL_X_EN_PATH 			IIO_DEVICE_PATH "/scan_elements/in_anglvel_x_en"
#define ANGLVEL_Y_EN_PATH 			IIO_DEVICE_PATH "/scan_elements/in_anglvel_y_en"
#define ANGLVEL_Z_EN_PATH			IIO_DEVICE_PATH "/scan_elements/in_anglvel_z_en"

#define ACCEL_X_CALIBBIAS_PATH 			IIO_DEVICE_PATH "/in_accel_x_calibbias"
#define ACCEL_Y_CALIBBIAS_PATH 			IIO_DEVICE_PATH "/in_accel_y_calibbias"
#define ACCEL_Z_CALIBBIAS_PATH 			IIO_DEVICE_PATH "/in_accel_z_calibbias"
#define ANGLVEL_X_CALIBBIAS_PATH 			IIO_DEVICE_PATH "/in_anglvel_x_calibbias"
#define ANGLVEL_Y_CALIBBIAS_PATH 			IIO_DEVICE_PATH "/in_anglvel_y_calibbias"
#define ANGLVEL_Z_CALIBBIAS_PATH			IIO_DEVICE_PATH "/in_anglvel_z_calibbias"

#define ACCEL_SCALE_PATH 			IIO_DEVICE_PATH "/in_accel_scale"
#define ANGLVEL_SCALE_PATH 			IIO_DEVICE_PATH "/in_anglvel_scale"

// AVG(t) = B * Avg(t-1) + (1 - B)*V(t)

void exit_Mpu6050(Mpu6050 *mpu6050)
{
	printf("exit_Mpu6050\n");

	if (write(mpu6050->_rt_para_fd, "0", 1) == -1) {
		perror("_rt_para_fd write err");
	}

	if (write(mpu6050->_fd._enable, "0", 1) == -1) {
		perror("exit_mpu6050 fail");
	}
}
INIT_EXIT(Mpu6050);

Mpu6050_fd::Mpu6050_fd()
{
	setup_buffer();
	setup_sampling_frequency();
	
	setup_acc_buffer();
	setup_gyro_buffer();
}

void Mpu6050_fd::setup_buffer()
{
	OPEN_FD(_enable, ENABLE_PATH, O_RDWR);
	if (write(_enable, "0", 1) == -1) {
		perror("setup_buffer");
		exit_program();
	}

	OPEN_FD(_watermark, WATERMARK_PATH, O_RDWR);
	OPEN_FD(_length, LENGTH_PATH, O_RDWR);
	OPEN_FD(_cdev, CDEV_PATH, O_RDONLY);

	char buf[10];

	if (LENGTH < WATERMARK) {
		fprintf(stderr, "lengh < watermark err\n");
		exit_program();
	}

	sprintf(buf, "%d", WATERMARK);
	if (write(_watermark, buf, strlen(buf)) == -1) {
		perror("setup_buffer");
		exit_program();
	}

	sprintf(buf, "%d", LENGTH);
	if (write(_length, buf, strlen(buf)) == -1) {
		perror("setup_buffer");
		exit_program();
	}
}
void Mpu6050_fd::setup_sampling_frequency()
{
	OPEN_FD(_sampling_frequency, SAMPLING_FREQUENCY_PATH, O_RDWR);

	char buf[10];

	sprintf(buf, "%d", SAMPLING_FREQ);
	if (write(_sampling_frequency, buf, strlen(buf)) == -1) {
		perror("setup_sampling_frequency");
		exit_program();
	}
}
void Mpu6050_fd::setup_gyro_buffer()
{
	OPEN_FD(_en[GYRO][X], ANGLVEL_X_EN_PATH, O_RDWR);
	OPEN_FD(_en[GYRO][Y], ANGLVEL_Y_EN_PATH, O_RDWR);
	OPEN_FD(_en[GYRO][Z], ANGLVEL_Z_EN_PATH, O_RDWR);

	OPEN_FD(_bias[GYRO][X], ANGLVEL_X_CALIBBIAS_PATH, O_RDONLY);
	OPEN_FD(_bias[GYRO][Y], ANGLVEL_Y_CALIBBIAS_PATH, O_RDONLY);
	OPEN_FD(_bias[GYRO][Z], ANGLVEL_Z_CALIBBIAS_PATH, O_RDONLY);

	OPEN_FD(_scale[GYRO], ANGLVEL_SCALE_PATH, O_RDWR);

	for (int i = 0; i < 3; i++) {
		if (write(_en[GYRO][i], "1", 1) == -1) {
			perror("setup_gyro_buffer");
			exit_program();
		}
	
		//(fd.bias[GYRO][i], &s_bias[GYRO][i]);
	}
}
void Mpu6050_fd::setup_acc_buffer()
{
	OPEN_FD(_en[ACC][X], ACCEL_X_EN_PATH, O_RDWR);
	OPEN_FD(_en[ACC][Y], ACCEL_Y_EN_PATH, O_RDWR);
	OPEN_FD(_en[ACC][Z], ACCEL_Z_EN_PATH, O_RDWR);

	OPEN_FD(_bias[ACC][X], ACCEL_X_CALIBBIAS_PATH, O_RDONLY);
	OPEN_FD(_bias[ACC][Y], ACCEL_Y_CALIBBIAS_PATH, O_RDONLY);
	OPEN_FD(_bias[ACC][Z], ACCEL_Z_CALIBBIAS_PATH, O_RDONLY);

	OPEN_FD(_scale[ACC], ACCEL_SCALE_PATH, O_RDWR);

	for (int i = 0; i < 3; i++) {
		if (write(_en[ACC][i], "1", 1) == -1) {
			perror("setup_acc_buffer");
			exit_program();
		}
	
		//read_bias(_fd.bias[ACC][i], &s_bias[ACC][i]);
	}
}

// void read_bias(int fd, int *val)
// {	
// 	char buf[100];
// 	int ret = 0, i = 0;

// 	while ((ret = read(fd, buf + i, 100)) != 0) {
// 		if (ret == -1) {
// 			perror("setup_anglvel_buffer");
// 			exit_program());
// 		} else {
// 			i += ret;
// 		}
// 	}

// 	*val = atoi(buf);
// }

Mpu6050::Mpu6050()
{
	ADD_EXIT(Mpu6050);

	set_sensitivity();

	if (write(_fd._enable, "1", 1) == -1) {
		perror("init_mpu6050");
		exit_program();
	}
	set_irq_thread_high_priority();
	
	OPEN_FD(_rt_para_fd, "/sys/module/inv_mpu6050/parameters/gye_para", O_RDWR);
	if (write(_rt_para_fd, "1", 1) == -1) {
		perror("_rt_para_fd write err");
		exit_program();
	}

	calibrate(30);
}
Mpu6050::~Mpu6050()
{
	exit_Mpu6050(this);
}
void Mpu6050::set_sensitivity()
{
	if (write(_fd._scale[GYRO], "0.000133090", strlen("0.000133090")) == -1) {
		perror("set_sensitivity fail\n");
		exit_program();
	}
}
void Mpu6050::set_irq_thread_high_priority()
{
    char pids[5][30];
	if (get_pid_str("ps -Leo pid,comm | grep 'irq.*inv_mpu'", pids) != 1) {  // trigger
		perror("get_pid_str, pid[0]");
		exit_program();
	}
	if (get_pid_str("ps -Leo pid,comm | grep 'irq.*mpu6050'", pids + 1) != 1) { // 
		perror("get_pid_str, pid[1]");
		exit_program();
	}
	if (get_pid_str("ps -Leo pid,comm | grep 'mv64xxx_i2c' | grep 296", pids + 2) != 1) {
		perror("get_pid_str, pid[2]");
		exit_program();
	}
	// if (get_pid_str("ps -Leo pid,comm | grep 'mv64xxx_i2c' | grep 20", pids + 2) != 1) {
	// 	perror("get_pid_str, pid[3]");
	// 	exit_program();
	// }

	int rt_fd;
	OPEN_FD(rt_fd, "/sys/fs/cgroup/cpuset/rt/tasks", O_RDWR);

	if ( write(rt_fd, pids[0], strlen(pids[0])) ==1 -1) {
		perror("write rt_fd err");
		exit_program();
	}
	if ( write(rt_fd, pids[1], strlen(pids[1])) == -1) {
		perror("write rt_fd err");
		exit_program();
	}	
	if ( write(rt_fd, pids[2], strlen(pids[2])) == -1) {
		perror("write rt_fd err");
		exit_program();
	}	

	//set_rt_fifo(atoi(pids[0]), 99);
	//set_rt_fifo(atoi(pids[1]), 99);
	//set_rt_fifo(atoi(pids[2]), 99);

	set_rt_deadline(atoi(pids[0]), 100 * 1000, 500 * 1000, 2000 * 1000);
	set_rt_deadline(atoi(pids[1]), 200 * 1000, 1500 * 1000, 2000 * 1000);
	//set_rt_deadline(atoi(pids[2]), 200 * 1000, 2000 * 1000, 2000 * 1000);	
	// set_rt_deadline(atoi(pids[3]), 200 * 1000, 2000 * 1000, 2000 * 1000);	
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
		int bytes_read = read_mpu6050_buf();
		
        if (bytes_read == -1) {
            perror("read");
			continue;
        }

		read_raw(acc, gyro);
		acc_to_angle(acc, acc_angle);

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

void Mpu6050::calc_sensor()
{
	//static float acc_angle_prev[NUM_AXIS] = { 0, };
	static float acc_angle_cur[NUM_AXIS] = { 0, };

	for (int i = 0; i < NUM_AXIS; i++) {
	//	_angle_prev[i] = _angle_cur[i];
	//	acc_angle_prev[i] = acc_angle_cur[i];
	//	_gyro_vel_prev[i] = _gyro_vel_cur[i];	
	}

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