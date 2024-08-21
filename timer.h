#ifndef TIMER_H
#define TIMER_H

#include <time.h>

#include "globals.h"

#define SEC_TO_NSEC     1000000000ULL

inline void update_new_mono_time(struct timespec *ts)
{
    if (clock_gettime(CLOCK_MONOTONIC, ts) != 0) {
        exit_program();
    }
}
inline void update_new_cpu_time(struct timespec *ts)
{
    if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, ts) != 0) {
        exit_program();
    }
}

static inline double time_to_double(long s, long ns)
{
    // return (float)(s * SEC_TO_NSEC + ns) / 1000000000.0f;  /* overflow err */

    double dt = (double)s + ((double)ns / 1000000000.0);
    return dt;
}
static inline double timespec_to_double(struct timespec *ts)
{
    return time_to_double(ts->tv_sec, ts->tv_nsec);
}

class LogTime
{
public:
    LogTime(int n);
    ~LogTime();

    inline void update_prev_time()
    {
        update_new_mono_time(&_ts_mono_prev);
        update_new_cpu_time(&_ts_cpu_prev);
    }
    inline void update_cur_time() 
    {
        update_new_mono_time(&_ts_mono_cur);
        update_new_cpu_time(&_ts_cpu_cur);
    }
    void ff();

private:
    double *_mono_dt;
    double *_cpu_dt;
    
    long *_mono_s;
    long *_mono_ns;

    int _n;

    int _i;
    double _dt_min = 999999.0f;
    double _dt_max = 0.0f;

    struct timespec _ts_mono_cur;
    struct timespec _ts_mono_prev;

    struct timespec _ts_cpu_cur;
    struct timespec _ts_cpu_prev;
};

class DtTrace 
{
    friend int exit_DtTrace(DtTrace *dt_trace);
public:     
    DtTrace(const char *name);
    ~DtTrace();

    inline void update_prev_time()
    {
        update_new_mono_time(&_ts_mono_prev);
        update_new_cpu_time(&_ts_cpu_prev);
    }
    inline void update_cur_time() 
    {
        update_new_mono_time(&_ts_mono_cur);
        update_new_cpu_time(&_ts_cpu_cur);
    }
    void print_data();
    void update_data();

private:
    const char *_name;

    double _mono_dt_max = 0.0f;
    double _mono_dt_min = 999999.0f;
    double _mono_dt_mean = 0.0f;

    double _cpu_dt_max = 0.0f;
    double _cpu_dt_min = 999999.0f;
    double _cpu_dt_mean = 0.0f;
    
    size_t _num = 0;

    struct timespec _ts_mono_cur;
    struct timespec _ts_mono_prev;

    struct timespec _ts_cpu_cur;
    struct timespec _ts_cpu_prev;
};

#define TRACE_FUNC_DT(dt_trace, func, ...) \
do { \
    dt_trace.update_prev_time(); \
    func(__VA_ARGS__); \
    dt_trace.update_cur_time(); \
    dt_trace.update_data(); \
} while(0)


// class Dt
// {
// 	Dt(double expected_dt, double error_margin);

// 	void check_dt();

// 	double _expected_dt = 0.0;
// 	double _error_dt_margin = 0.0;

// 	struct timespec _ts_cur;
// 	struct timespec _ts_prev;
// };

// Dt::Dt(double expected_dt, double error_margin)	
// 	: _expected_dt(expected_dt)
// 	, _error_dt_margin(error_margin)
// {}

// void Dt::check_dt()
// {
// 	float dt_ = timespec_to_double(&_ts_cur) - timespec_to_double(&_ts_prev);
// 	float dt = dt_;

// 	if (dt < _expected_dt) {
// 		//printf("delta dt:%f\n", dt - _loop_dt);
// 		struct timespec ts;
// 		ts.tv_sec = 0;
// 		ts.tv_nsec =  (_expected_dt - dt) * 1000 * 1000 * 1000;

// 		nanosleep(&ts, NULL);
// 		update_new_mono_time(&_ts_cur);
// 		dt = timespec_to_double(&_ts_cur) - timespec_to_double(&_ts_prev);
// 	}
// 	if (unlikely(dt > _expected_dt + _error_dt_margin)) {
// 		printf("!!!!!!!!!!loop_dt over, dt:%f ---> %f\n", dt_, dt);
// 		exit_program();
// 	}
// };


#endif 

