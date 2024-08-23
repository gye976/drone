#include <gpiod.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include "globals.h"

#include "hc_sr04.h"

#define CONSUMER "HC-SR04"
#define CHIPNAME "gpiochip0"
#define TRIG_PIN 272 // PI16 = 256 + 16 = 272
#define ECHO_PIN 260 // PI4 = 256 + 4 = 260


int exit_HcSr04(HcSr04 *hc_sr04)
{
	gpiod_chip_close(hc_sr04->_chip);

	return 0;
}
DEFINE_EXIT(HcSr04);


HcSr04::HcSr04()
{
	INIT_EXIT_IN_CTOR(HcSr04);

	int ret;

	// GPIO 칩 열기
	_chip = gpiod_chip_open_by_name(CHIPNAME);
	if (!_chip) {
		perror("gpiod_chip_open_by_name");
		exit_program();
	}

	// Trig 및 Echo 라인 가져오기
	_trig_line = gpiod_chip_get_line(_chip, TRIG_PIN);
	_echo_line = gpiod_chip_get_line(_chip, ECHO_PIN);
	if (!_trig_line || !_echo_line) {
		perror("gpiod_chip_get_line");
		exit_program();
	}

	// Trig 라인을 출력으로 설정
	ret = gpiod_line_request_output(_trig_line, CONSUMER, 0);
	if (ret < 0) {
		perror("gpiod_line_request_output");
		exit_program();
	}

	// Echo 라인을 입력으로 설정
	ret = gpiod_line_request_input(_echo_line, CONSUMER);
	if (ret < 0) {
		perror("gpiod_line_request_input");
		exit_program();
	}

    float altitude = 0.0f;
	for (int i = 0; i < 100; i++) {	
		altitude += measure_distance();
	}
	_altitude = altitude / 100;
}

float HcSr04::measure_distance() 
{
	int ret;

   // Trig 핀에 10µs 동안 HIGH 신호를 보냄
    ret = gpiod_line_set_value(_trig_line, 1);
	if (ret != 0) {
		perror("gpiod_line_set_value");
		exit_program();
	}

    busy_wait_micros(10);
    
	ret = gpiod_line_set_value(_trig_line, 0);
	if (ret != 0) {
		perror("gpiod_line_set_value");
		exit_program();
	}

    struct timespec start, end;

    // Echo 핀에서 신호가 들어올 때까지 대기
    while (gpiod_line_get_value(_echo_line) == 0) {
    }
    clock_gettime(CLOCK_MONOTONIC, &start);

	struct timespec ts;
	ts.tv_sec = 0;
	ts.tv_nsec = 650 * 1000; 
	nanosleep(&ts, NULL);

    while (gpiod_line_get_value(_echo_line) == 1) {
    }
    clock_gettime(CLOCK_MONOTONIC, &end);

    // 시간 차이 계산 (마이크로초 단위)
    long travel_time = (end.tv_sec - start.tv_sec) * 1000000L + (end.tv_nsec - start.tv_nsec) / 1000L;
	// printf("dt:%zu\n", travel_time);

    // 거리 계산 (음속: 34300 cm/s, 왕복 거리이므로 2로 나눔)
    float distance = (travel_time * 0.0343) / 2.0;
	// printf("distance:%f\n", distance);

    return distance;
}
void HcSr04::update_altitude() 
{
    float distance = measure_distance();

    _altitude = ((1.0 - A) * _altitude) + (A * distance);	
	//printf("alti:%f\n", _altitude);
}

void busy_wait_micros(unsigned int micros)
{
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    do {
        clock_gettime(CLOCK_MONOTONIC, &end);
    } while ((end.tv_sec - start.tv_sec) * 1000000 + (end.tv_nsec - start.tv_nsec) / 1000 < micros);
}

void hc_sr04_do_once(void *arg)
{
	HcSr04 *hc_sr04 = (HcSr04*)arg;
	(void)hc_sr04;

	pthread_setschedparam_wrapper(pthread_self(), SCHED_RR, 50);
}
void hc_sr04_loop(void *arg)
{
	((HcSr04*)arg)->update_altitude();
}