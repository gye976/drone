#ifndef HC_SR04_H
#define HC_SR04_H

class HcSr04 
{
	friend int exit_HcSr04(HcSr04 *hc_sr04);

public:
	HcSr04();

	inline float get_altitude() 
	{
		return _altitude;
	}
	float measure_distance();
	void update_altitude();
	void init_altitude(int num);
	int check_edge(int event_type, struct timespec *timeout);
private:
	struct gpiod_chip *_chip;
	struct gpiod_line *_trig_line, *_echo_line;
	
	float _altitude;
};

void busy_wait_micros(unsigned int micros);

void hc_sr04_do_once(void *arg);
void hc_sr04_loop(void *arg);

#endif
