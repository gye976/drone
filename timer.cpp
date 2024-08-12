#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>

#include "timer.h"
#include "mpu6050.h"
#include "globals.h"

DtTrace::DtTrace(int n) 
    : _n(n), _i(0), _dt_min(100.0f), _dt_max(0.0f)
{
    _mono_dt = (float*)malloc(_n * sizeof(float));
    _cpu_dt = (float*)malloc(_n * sizeof(float));

    _mono_s = (long*)malloc(_n * sizeof(long));
    _mono_ns = (long*)malloc(_n * sizeof(long));
}
DtTrace::~DtTrace()
{
    free(_mono_dt);
    free(_mono_s);
    free(_mono_ns);
}
void DtTrace::ff()
{
	_mono_dt[_i] =  timespec_to_float(&_ts_mono_cur) - timespec_to_float(&_ts_mono_prev);
	_cpu_dt[_i] =  timespec_to_float(&_ts_cpu_cur) - timespec_to_float(&_ts_cpu_prev);

	_mono_s[_i] = _ts_mono_cur.tv_sec;
	_mono_ns[_i] = _ts_mono_cur.tv_nsec;

    _i++;

    int min_i = 0, max_i = 0;
	int n_over = 0;

    if (_i == _n) {
	    for (int i = 1; i < _n; i++) {
			printf("%02d, dt:%f/%f   (%ld.%09ld)\n", i, _mono_dt[i], _cpu_dt[i], _mono_s[i], _mono_ns[i]);
            if (_mono_dt[i] > _dt_max) { 
                _dt_max = _mono_dt[i];
                max_i = i;
            } else if (_mono_dt[i] < _dt_min) {
                _dt_min = _mono_dt[i];
                min_i = i;
            }

            if (_mono_dt[i] > 0.006) {
                n_over++;
            }
		}

		printf("\nmax:%f(%d)\n(%ld.%09ld) -> (%ld.%09ld)\nmin:%f(%d)\n(%ld.%09ld) -> (%ld.%09ld)\n",
             _dt_max, max_i, _mono_s[max_i - 1], _mono_ns[max_i - 1], _mono_s[max_i], _mono_ns[max_i], \
             _dt_min, min_i, _mono_s[min_i - 1], _mono_ns[min_i - 1], _mono_s[min_i], _mono_ns[min_i]);
        
        printf("over n: %d\n", n_over);
        exit_program();
	} 
}