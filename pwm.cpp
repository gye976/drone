#include "globals.h"
#include "pwm.h"

#define PATH_PWM "/sys/class/pwm/pwmchip0"

int exit_Pwm(Pwm *pwm)
{
	if (-1 == write(pwm->_duty_cycle_fd, "1000000", strlen("1000000")))
	{
		perror("exit_pwm fail");
		return -1;
	}

	return 0;
}
DEFINE_EXIT(Pwm);

Pwm::Pwm(int num)
	: _duty_cycle(1000000), _n(num)
{
	INIT_EXIT_IN_CTOR(Pwm);

	int export_fd;
	OPEN_FD(export_fd, PATH_PWM "/export", O_WRONLY);
	char buf[3];
	if (sprintf(buf, "%d", num) < 0)
	{
		perror("init sprintf buf fail\n");
		exit_program();
	}

	if (write(export_fd, buf, strlen(buf)) == -1)
	{
		printf("Pwm export fail?\n");
	}
	close(export_fd);

	if (sprintf(_path, PATH_PWM "/pwm%d", num) < 0)
	{
		perror("init sprintf _path fail\n");
		exit_program();
	}

	open_paths_fd(&_period_fd, _path, "/period", O_RDWR);
	open_paths_fd(&_duty_cycle_fd, _path, "/duty_cycle", O_RDWR);
	open_paths_fd(&_enable_fd, _path, "/enable", O_RDWR);
	open_paths_fd(&_polarity_fd, _path, "/polarity", O_RDWR);

	/* 20000000(ns) = 2*10^7, -> 20ms*/
	if (-1 == write(_period_fd, "20000000", strlen("20000000")))
	{
		perror("init _period_fd fail");
		exit_program();
	}

	if (-1 == write(_duty_cycle_fd, "1000000", strlen("1000000")))
	{
		perror("init _duty_cycle_fd fail");
		exit_program();
	}

	if (-1 == write(_enable_fd, "1", 1))
	{
		perror("init _enable_fd fail");
		exit_program();
	}
}

Pwm::~Pwm()
{
	exit_Pwm(this);
}

void Pwm::set_duty_cycle(int duty)
{
	if (unlikely(!(PWM_MIN <= duty && duty <= PWM_MAX))) {
		fprintf(stderr, "duty range err\n");
		exit_program();
	}

	_duty_cycle = duty;

	char duty_s[15];
	sprintf(duty_s, "%d", _duty_cycle);

	int ret = write(_duty_cycle_fd, duty_s, strlen(duty_s));
	if (unlikely(ret == -1)) {
		perror("set_duty_cycle write fail\n");
		exit_program();	
	}
}
