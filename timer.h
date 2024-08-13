#ifndef TIMER_H
#define TIMER_H

#include <time.h>

#include "globals.h"

#define SEC_TO_NSEC     1000000000ULL

static inline void update_new_mono_time(struct timespec *ts)
{
    if (clock_gettime(CLOCK_MONOTONIC, ts) != 0) {
        exit_program();
    }
}
static inline void update_new_cpu_time(struct timespec *ts)
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

// class Time
// {
// public:
//     inline void update_pre_time()
//     {
//         update_new_mono_time(&_ts_mono_prev);
//         update_new_cpu_time(&_ts_cpu_prev);
//     }
//     inline void update_cur_time() 
//     {
//         update_new_mono_time(&_ts_mono_cur);
//         update_new_cpu_time(&_ts_cpu_cur);
//     }
//     inline float get_mono_dt() 
//     {
//         long mono_s = _ts_mono_cur.tv_sec - _ts_mono_prev.tv_sec; 
//         long mono_ns = _ts_mono_cur.tv_nsec - _ts_mono_prev.tv_nsec;
        
//         return time_to_float(mono_s, mono_ns);
//     }
//     inline float get_cpu_dt() 
//     {
//         long cpu_s = _ts_cpu_cur.tv_sec - _ts_cpu_prev.tv_sec; 
//         long cpu_ns = _ts_cpu_cur.tv_nsec - _ts_cpu_prev.tv_nsec;
        
//         return time_to_float(cpu_s, cpu_ns);
//     }

// private:
//     struct timespec _ts_mono_cur;
//     struct timespec _ts_mono_prev;

//     struct timespec _ts_cpu_cur;
//     struct timespec _ts_cpu_prev;
// };

class DtTrace 
{
public:
    DtTrace(int n);
    ~DtTrace();

    inline void update_pre_time()
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


#define trace_func_dt(dt, func, ...) \
do { \
    dt.update_pre_time(); \
    func(__VA_ARGS__); \
    dt.update_cur_time(); \
    dt.ff(); \
} while(0)


#endif 

