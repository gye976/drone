#ifndef DRONE_H
#define DRONE_H

#include <stdio.h>
#include <pthread.h>

#include "debug.h"
#include "mpu6050.h"
#include "hc_sr04.h"
#include "pid.h"
#include "pwm.h"

/*
    전방

    +y
    |
    |
    |    
    |
 +z  ㅡㅡㅡㅡㅡㅡ  +x
(+z는 지면에서 나오는 방향)

*/

/*

pitch각 입력이 감소 -> 상쇄하기 위해 pitch output 증가 : 피치각을 높여라
    
throttle업: 모든 모터 출력업
pitch업:    FRONT업, REAR 다운

roll 업:    LEFT 업, RIGHT 다운

raw  업:    FRONT_LEFT/REAR_RIGHT 업,
                FRONT_RIGHT/REAR_LEFT 다운
*/

enum {
    FRONT_LEFT, 
    FRONT_RIGHT,
    REAR_RIGHT,
    REAR_LEFT,
    NUM_MOTOR,
};

class Drone 
{
public:
    friend void drone_do_once(void *arg);

    Drone(float loop_dt);
    ~Drone();

    inline Mpu6050* get_mpu6050()
    {
        return &_mpu6050;
    }
    inline HcSr04* get_hc_sr04()
    {
        return &_hc_sr04;
    }
    // inline int try_lock_drone()
    // {
    //     int ret = pthread_spin_trylock(&_spinlock);
    //     printf("lock ret:%d\n", ret);
        
    //     return ret;
    // }
    // inline int trylock_drone()
    // {
    //     int ret = pthread_mutex_trylock(&_mutex);

    //     if (unlikely(!(ret == 0 || ret == EBUSY))) {
    //         perror("trylock_drone err");
    //         exit_program();
    //     }

    //     return ret;
    // }
    // inline void lock_drone()
    // {
    //     int ret = pthread_mutex_lock(&_mutex);
    //     if (unlikely(ret) != 0) {
    //         perror("lock_drone err");
    //         exit_program();
    //     }
    // }
    // inline void unlock_drone()
    // {   
    //     int ret = pthread_mutex_unlock(&_mutex);
    //     if (unlikely(ret) != 0) {
    //         perror("unlock_drone err");
    //         exit_program();
    //     }
    // }
    inline void set_throttle(int t)
    {
        if (t < 0 || t > 800000) {
            printf("t range err\n");
            return;
        }

        _throttle_basic = t;
    }   
    inline void up_altitude(int altitude)
    {
        _altitude_target += altitude;
    }   
    inline void down_altitude(int altitude)
    {
        _altitude_target -= altitude;
    }  
    inline void update_altitude_target()
    {
        float altitude = _hc_sr04.get_altitude();
        _altitude_target = altitude;
    }  
    inline void set_iterm_default()
    {
       // _pid_gyro_PRY[0].set_iterm(-13.0);
        //_pid_gyro_PRY[1].set_iterm(10.0);
    }  

    void loop();
    void check_loop_dt();
    void print_parameter();
    void log_data();

    void read_and_update_pid_gain();
    void cascade_drone_pid(Pid* pid_angle, Pid* pid_gyro, float angle, float gyro_rate, float* out);
    void update_PR_pid_out(float angle_input[], float gyro_rate_input[]);
    void update_Y_pid_out(float gyro_rate_input);
    void update_altitude_pid_out(float altitude_input);
    void aa();

private:    
    void print_pid_gain();
	void set_motor_speed();
    void update_pid_out(float *angle_cur, float *gyro_rate_cur); 
    void set_hovering();
    void up(int duty);

    MakeThread *_makethread;

    Mpu6050 _mpu6050;
    HcSr04 _hc_sr04;
    Pwm _pwm[NUM_MOTOR];

    Pid _pid_angle_PRY[3];
    Pid _pid_gyro_PRY[3];

    Pid _pid_altitude;
   // Pid _pid_altitude_velocity;


    // pthread_mutex_t _mutex;

	struct timespec _mono_loop_ts_cur;
	struct timespec _mono_loop_ts_prev;

    float _loop_dt;
    size_t _loop_cycle = 0;

    float _angle_target[NUM_AXIS] =  { 0, } ;
    float _rate_target[NUM_AXIS] = { 0, };

    float _altitude_target; //= 12.8f; //default
    float _altitude_input_prev; // = 12.8f; 
    
    float _altitude_velocity_prev = 0.0f;

    int _pitch = 0, _roll = 0, _yaw = 0;
    int _throttle = 0;
    int _throttle_basic = 0;
};

void drone_do_once(void *arg);
void drone_loop(void *arg);

#endif 

