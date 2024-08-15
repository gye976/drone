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
static inline float time_to_float(long s, long ns)
{
    return (float)(s * SEC_TO_NSEC + ns) / 1000000000.0f;
}
static inline float timespec_to_float(struct timespec *ts)
{
    return time_to_float(ts->tv_sec, ts->tv_nsec);
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
    float *_mono_dt;
    float *_cpu_dt;
    
    long *_mono_s;
    long *_mono_ns;

    int _n;

    int _i;
    float _dt_min;
    float _dt_max;

    struct timespec _ts_mono_cur;
    struct timespec _ts_mono_prev;

    struct timespec _ts_cpu_cur;
    struct timespec _ts_cpu_prev;
};

class DtTrace 
{
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

    float _mono_dt_max = 0.0f;
    float _mono_dt_min = 999999.0f;
    float _mono_dt_mean = 0.0f;

    float _cpu_dt_max = 0.0f;
    float _cpu_dt_min = 999999.0f;
    float _cpu_dt_mean = 0.0f;
    
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


#endif 

