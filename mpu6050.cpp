#ifndef _GNU_SOURCE
    #define _GNU_SOURCE
#endif

#include "global_predefined.h"

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

int exit_Mpu6050(Mpu6050 *mpu6050)
{
	(void)mpu6050;

	return 0;
}
DEFINE_EXIT(Mpu6050);

Mpu6050::Mpu6050(float dt)
	: _dt(dt)
{
	INIT_EXIT_IN_CTOR(Mpu6050);

	OPEN_FD(_i2c_fd, "/dev/i2c-2", O_RDWR);

    // Set the I2C address for the MPU6050
    if (ioctl(_i2c_fd, I2C_SLAVE, MPU6050_ADDR) < 0) {
        perror("Failed to connect to MPU6050 sensor");
        exit_program();
    }

    // reset device
    char config[2] = {PWR_MGMT_1, 0x40};
    if (write(_i2c_fd, config, 2) != 2) {
        perror("Failed to reset MPU6050");
        exit_program();
    }
    // Initialize MPU6050: Set Power Management 1 register to 0 to wake up the sensor
    config[1] = 0;
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

    // Set Gyroscope sensitivity: ±500°/s (Configuration 0)
    char gyro_config[2] = {GYRO_CONFIG, 0x01}; 
    if (write(_i2c_fd, gyro_config, 2) != 2) {
        perror("Failed to set gyroscope configuration");
        exit_program();
    }

    // Set Accelerometer sensitivity: ±4g (Configuration 0)
    char accel_config[2] = {ACCEL_CONFIG, 0x01}; 
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

#ifdef MPU6050_FIFO

    // 0x70: gyro XYZ, 0x08: acc XYZ
    char fife_en_list[2] = {FIFO_EN, 0x78};
    if (write(_i2c_fd, fife_en_list, 2) != 2) {
        perror("Failed to set fife_en_list");
        exit_program();
    }

    // 0x40: fifo enable, 0x04: reset after reading fifo
    char user_ctrl[2] = {USER_CTRL, 0x44};
    if (write(_i2c_fd, user_ctrl, 2) != 2) {
        perror("Failed to set user_ctrl(fifo)");
        exit_program();
    }

	/* To do: fifo reset interrupt*/

#endif

	set_irq_thread_high_priority();
}

Mpu6050::~Mpu6050()
{
	exit_Mpu6050(this);
}

void Mpu6050::set_irq_thread_high_priority()
{

}

void Mpu6050::calibrate(int n, float acc_angle_mean[], float gyro_rate_mean[])
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
		acc_angle_mean[i] = acc_angle_sum[i] / n;
		gyro_rate_mean[i] = gyro_sum[i] / n;
	}

	printf("cur acc: %f, %f, %f\n", acc_angle_mean[0], acc_angle_mean[1], acc_angle_mean[2]);
	printf("cur gyro: %f, %f, %f\n", gyro_rate_mean[0], gyro_rate_mean[1], gyro_rate_mean[2]);
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
		perror("read_raw, write err");
		exit_program();
	}
		
	ret = read(_i2c_fd, data, 14);
	if (unlikely(ret != 14)) {
			perror("read_raw, read err");
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


	// printf("read_raw, acc: %d, %d, %d\n", accel_x,  accel_y,  accel_z);
	//  printf("		gyro: %d, %d, %d\n\n", gyro_x, gyro_y, gyro_z);
}

void Mpu6050::read_fifo(float acc[], float gyro[])
{
	static int delta_idx_prev;
	
	int delta_idx = _produce_idx - _consume_idx;
	if (delta_idx < 0) {
		delta_idx += FIFO_SIZE;
	}

	//printf("delta idx: %d, consume %d, produce %d\n", delta_idx, _consume_idx, _produce_idx);

	if (unlikely(delta_idx - delta_idx_prev > FIFO_SIZE / 2)) {
		printf("producer is slow, consume idx overflow, bug\n");
		exit_program();
	}

	if (delta_idx >= 16) {
		_consume_idx = (_consume_idx + (delta_idx / 2)) & (FIFO_SIZE - 1);
		// printf(" ---> %d\n", _consume_idx);
	} else {
		//  printf("\n");
	}

	//if (delta_idx >= FIFO_SIZE / 4) {
	// 	_consume_idx = (_consume_idx + FIFO_SIZE / 8) & (FIFO_SIZE - 1);
	// }

// A:
// 	if (unlikely(_consume_idx == _produce_idx)) {
// 		printf("!!!!!????read_fifo, idx same\n");
// 		usleep(100);
// 		goto A;
// 	}

	// if (unlikely((delta_idx + fifo_count / 12) > FIFO_SIZE)) {
	// 	printf("%d, %d\n", delta_idx, fifo_count / 12);
	// 	printf("mpu consumer is slow, bug\n");
	// 	exit_program();
	// }

	int i = _consume_idx * 12;
	char *buf = _fifo;

	int16_t accel_x = (buf[i] << 8) | buf[i + 1];
	int16_t accel_y = (buf[i + 2] << 8) | buf[i + 3];
	int16_t accel_z = (buf[i + 4] << 8) | buf[i + 5];

	int16_t gyro_x = (buf[i + 6] << 8) | buf[i + 7];
	int16_t gyro_y = (buf[i + 8] << 8) | buf[i + 9];
	int16_t gyro_z = (buf[i + 10] << 8) | buf[i + 11];

	acc[X] = (float)accel_x / ACC_FS_SENSITIVITY;
	acc[Y] = (float)accel_y / ACC_FS_SENSITIVITY;
	acc[Z] = (float)accel_z / ACC_FS_SENSITIVITY;

	gyro[X] = (float)gyro_x / GYRO_FS_SENSITIVITY;
	gyro[Y] = (float)gyro_y / GYRO_FS_SENSITIVITY;
	gyro[Z] = (float)gyro_z / GYRO_FS_SENSITIVITY;

	_consume_idx = (_consume_idx + 1) & (FIFO_SIZE - 1); 

	// printf("read_fifo, acc: %d, %d, %d\n", accel_x,  accel_y,  accel_z);
	// printf("		gyro: %d, %d, %d\n\n", gyro_x, gyro_y, gyro_z);
}

void Mpu6050::read_hwfifo()
{
    // Read FIFO count
    char fifo_buf[1024];
    char buf[2];
	int ret;
	int fifo_count;
	int delta_idx;
	int next_produce_idx;
	
	if (check_hwfifo_overflow() == 1) {
		reset_hwfifo();
	}

	buf[0] = FIFO_COUNT_H;
	ret = write(_i2c_fd, buf, 1);
	if (unlikely(ret != 1)) {
		perror("read_fifo, write err");
		exit_program();
	}

	ret = read(_i2c_fd, buf, 2);
	if (unlikely(ret != 2)) {
		perror("read_fifo, read err");
		exit_program();
	}

	fifo_count = (buf[0] << 8) | buf[1];
	// printf("fifo_cnt:%d\n", fifo_count);

	if (fifo_count < 12) {
		// printf("fifo_cnt < 12, again\n");
		return;
	}
	if (fifo_count % 12 != 0) {
		// printf("fifo_cnt % 12 != 0, again\n");
		return;
	}

	buf[0] = FIFO_R_W;
	ret = write(_i2c_fd, buf, 1);
    if (unlikely(ret != 1)) {
		perror("read_fifo, write err");
		exit_program();
    }
	ret = read(_i2c_fd, fifo_buf, fifo_count); 
    if (unlikely(ret != fifo_count)) {
		perror("read_fifo, read err");
		exit_program();
    }

	if (check_hwfifo_overflow() == 1) {
		reset_hwfifo();
		return;
	}

	delta_idx = _produce_idx - _consume_idx;
	if (delta_idx < 0) {
		delta_idx += FIFO_SIZE;
	}

	if (unlikely((delta_idx + fifo_count / 12) > FIFO_SIZE)) {
		printf("%d, %d\n", delta_idx, fifo_count / 12);
		printf("consumer is slow, produce idx overflow, bug\n");
		exit_program();
	}

	int packet_cnt = fifo_count / 12;

	next_produce_idx = (_produce_idx + packet_cnt) & (FIFO_SIZE - 1);

	if (_produce_idx + packet_cnt > FIFO_SIZE - 1) {
		int fifo_cnt_1 = (FIFO_SIZE - _produce_idx) * 12;
		int fifo_cnt_2 = fifo_count - fifo_cnt_1;

		//printf("!!!!!!!cnt1,2: %d, %d, fifo_cnt:%d, pro_idx:%d,\n", fifo_cnt_1, fifo_cnt_2, fifo_count, _produce_idx);
		memcpy(&_fifo[_produce_idx * 12], fifo_buf, fifo_cnt_1);
		memcpy(&_fifo[0], fifo_buf + fifo_cnt_1, fifo_cnt_2);

		// int16_t accel_x = (fifo_buf[_produce_idx * 12] << 8) |fifo_buf[_produce_idx * 12 + 1];
		// 		printf("0");

		// int16_t accel_y = (fifo_buf[_produce_idx * 12 + 2] << 8) | fifo_buf[_produce_idx * 12 + 3];
		// 		printf("1");

		// int16_t accel_z = (fifo_buf[_produce_idx * 12 + 4] << 8) | fifo_buf[_produce_idx * 12 + 5];
		// 		printf("2");

		// int16_t gyro_x = (fifo_buf[_produce_idx * 12 + 6] << 8) | fifo_buf[_produce_idx * 12 + 7];
		// 				printf("3");
		// int16_t gyro_y = (fifo_buf[_produce_idx * 12 + 8] << 8) | fifo_buf[_produce_idx * 12 + 9];
		// 		printf("4");

		// int16_t gyro_z = (fifo_buf[_produce_idx * 12 + 10] << 8) | fifo_buf[_produce_idx * 12 + 11];
		// printf("read_raw, acc: %d, %d, %d\n", accel_x,  accel_y,  accel_z);
		// printf("		gyro: %d, %d, %d\n\n", gyro_x, gyro_y, gyro_z);
	} else {
		// printf("else, %d\n", _produce_idx);
		memcpy(&_fifo[_produce_idx * 12], fifo_buf, fifo_count);
	}

	// printf("fifo_cnt:%d, _consume_idx, _produce_idx:%d, %d, ----------idx delta:%d \n", fifo_count, _consume_idx, _produce_idx, delta_idx);	
	// printf("produce idx:%d --> %d \n", _produce_idx, next_produce_idx);	
	_produce_idx = next_produce_idx;
}

int Mpu6050::check_hwfifo_overflow()
{
	int ret;
	char buf[1] = { INT_STATUS };
	
	ret = write(_i2c_fd, buf, 1);
    if (unlikely(ret != 1)) {
		perror("check_hwfifo_overflow, write err");
		exit_program();
    }
	ret = read(_i2c_fd, buf, 1); 
    if (unlikely(ret != 1)) {
		perror("check_hwfifo_overflow, read err");
		exit_program();
    }

	if (buf[0] & FIFO_OVERFLOW_BIT) {
		// printf("!!!!!!!!!!!!!hwfifo overflow!\n");
		return 1;
	} else {
		return 0;
	}
}

void Mpu6050::reset_hwfifo()
{
	char user_ctrl[2] = {USER_CTRL, 0x44};
	if (write(_i2c_fd, user_ctrl, 2) != 2) {
		perror("Failed to set user_ctrl(fifo)");
		exit_program();
	}
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


#ifdef MPU6050_FIFO
	read_fifo(new_acc, new_gyro);
#else 
	read_raw(new_acc, new_gyro);
#endif

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

	// printf("_gyro_vel_cur: %f, %f, %f\n", _gyro_rate[0], _gyro_rate[1], _gyro_rate[2]);
	// printf("acc angle: %f, %f, %f\n", acc_angle_cur[0], acc_angle_cur[1], acc_angle_cur[2]);
	// printf("gyro_angle: %f, %f, %f\n", new_gyro_angle[0],  new_gyro_angle[1],  new_gyro_angle[2]);
	// printf("_angle_cur: %f, %f, %f\n", _angle[0],  _angle[1],  _angle[2]);
	// printf("\n");
}

void Mpu6050::print_data()
{
	printf("Pitch,	angle:%f, gyro:%f\n", _angle[PITCH], _gyro_rate[PITCH]);
	printf("Roll,	angle:%f, gyro:%f\n", _angle[ROLL], _gyro_rate[ROLL]);
	printf("Yaw,	angle:%f, gyro:%f\n\n", _angle[YAW], _gyro_rate[YAW]);
}

void *mpu6050_read_hwfifo_loop(void *arg)
{
#ifndef MPU6050_FIFO
	while (1);
#endif

	Mpu6050 *mpu6050 = (Mpu6050*)arg;

	while (1) {
		mpu6050->read_hwfifo();
	}

	return NULL; 
}

pthread_t make_mpu6050_read_hwfifo_thread(Mpu6050 *mpu6050)
{    
	pthread_t thread;
	if (pthread_create(&thread, NULL, mpu6050_read_hwfifo_loop, mpu6050) != 0) {
		perror("make_mpu6050_read_hwfifo_thread ");
		exit_program();
	}
	pthread_setname_np(thread, "gye-mpu6050");

	struct sched_param param = {
		.sched_priority = 99,
	};
	if (pthread_setschedparam(thread, SCHED_RR, &param) != 0)
	{
		perror("make_mpu6050_read_hwfifo_thread setschedparam");
		exit_program();
	}

	return thread;
}