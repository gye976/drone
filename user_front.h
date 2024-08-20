#ifndef USER_FRONT_H
#define USER_FRONT_H

class Drone;

void user_do_once(void *drone);
void user_loop(void *drone);
// void init_user_front_thread();

pthread_t make_user_front_thread(Drone *drone);

#endif 

