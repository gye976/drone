#ifndef PWM_H
#define PWM_H

#define PWM_PERIOD	20 * 1000 * 1000  // 20ms

#define PWM_MIN 	1 * 1000 * 1000 // 1ms ~ 2ms
#define PWM_MAX 	2 * 1000 * 1000

class Pwm 
{
    friend int exit_Pwm(Pwm *pwm);
public:
    Pwm(int num);
    ~Pwm();

    void set_duty_cycle(int duty);

private:
    int _period_fd;
    int _duty_cycle_fd;
    int _polarity_fd;
    int _enable_fd;

    int _duty_cycle;
    int _n;
    char _path[30];
};

#endif 

