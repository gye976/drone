#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>

#include "timer.h"
#include "mpu6050.h"
#include "globals.h"

int exit_DtTrace(DtTrace *dt_trace)
{
    if (dt_trace->_num != 0)
        dt_trace->print_data();

    return 0;
}
DEFINE_EXIT(DtTrace);

DtTrace::DtTrace(const char *name)
    : _name(name)
{
    INIT_EXIT_IN_CTOR(DtTrace);
}
DtTrace::~DtTrace()
{
    //print_data();
}
void DtTrace::print_data()
{
    printf("%s,    num:%zu", _name, _num);
	printf("\nmono_dt_max:%f, cpu_dt_max:%f \nmono_dt_min:%f, cpu_dt_min:%f \nmono_dt_mean:%f, cpu_dt_mean:%f \n", 
        _mono_dt_max, _cpu_dt_max, _mono_dt_min, _cpu_dt_min, _mono_dt_mean, _cpu_dt_mean);
}
void DtTrace::update_data()
{
	double mono_dt =  timespec_to_double(&_ts_mono_cur) - timespec_to_double(&_ts_mono_prev);
	double cpu_dt =  timespec_to_double(&_ts_cpu_cur) - timespec_to_double(&_ts_cpu_prev);

    if (mono_dt > _mono_dt_max) { 
        _mono_dt_max = mono_dt;
    } else if (mono_dt < _mono_dt_min) {
        _mono_dt_min = mono_dt;
    }

    if (cpu_dt > _cpu_dt_max) { 
        _cpu_dt_max = cpu_dt;
    } else if (cpu_dt < _cpu_dt_min) {
        _cpu_dt_min = cpu_dt;
    }

    _num++;

    _mono_dt_mean = (_mono_dt_mean * (_num - 1) + mono_dt) / _num; 
    _cpu_dt_mean = (_cpu_dt_mean * (_num - 1) + cpu_dt) / _num; 
}  


LogTime::LogTime(int n) 
    : _n(n), _i(0), _dt_min(100.0f), _dt_max(0.0f)
{
    _mono_dt = (double*)malloc(_n * sizeof(double));
    _cpu_dt = (double*)malloc(_n * sizeof(double));

    _mono_s = (long*)malloc(_n * sizeof(long));
    _mono_ns = (long*)malloc(_n * sizeof(long));
}
LogTime::~LogTime()
{
    free(_mono_dt);
    free(_cpu_dt);
    
    free(_mono_s);
    free(_mono_ns);
}
void LogTime::ff()
{
	_mono_dt[_i] =  timespec_to_double(&_ts_mono_cur) - timespec_to_double(&_ts_mono_prev);
	_cpu_dt[_i] =  timespec_to_double(&_ts_cpu_cur) - timespec_to_double(&_ts_cpu_prev);

	_mono_s[_i] = _ts_mono_cur.tv_sec;
	_mono_ns[_i] = _ts_mono_cur.tv_nsec;

    _i++;

    int min_i = 0, max_i = 0;
	//int n_over = 0;

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

            // if (_mono_dt[i] > 0.) {
            //     n_over++;
            // }
		}

		printf("\nmax:%f(%d)\n(%ld.%09ld) -> (%ld.%09ld)\nmin:%f(%d)\n(%ld.%09ld) -> (%ld.%09ld)\n",
             _dt_max, max_i, _mono_s[max_i - 1], _mono_ns[max_i - 1], _mono_s[max_i], _mono_ns[max_i], \
             _dt_min, min_i, _mono_s[min_i - 1], _mono_ns[min_i - 1], _mono_s[min_i], _mono_ns[min_i]);
        
//        printf("over n: %d\n");
        exit_program();
	} 
}