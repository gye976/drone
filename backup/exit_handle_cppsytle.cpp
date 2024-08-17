#include <stdio.h>
#include <stdlib.h>

#include "exit_handle.h"

ExitHandle g_exit_handle;

Exit::Exit(ExitHandle *exit_handle,  void (*func)())
    : _exit_handle(exit_handle), _exit_func(func)
{
    _exit_handle->add_exit(this);
	    printf(" Exit const, inst ptr:%p\n", this); 
}

void Exit::add_exit_instance(void *instance)
{
    _instance_list[_instance_num] = instance;
    _instance_num++;
}
void Exit::invoke_exit_global()
{
    _exit_func();
}

void ExitHandle::invoke_exit_program()
{
	static bool s_exit_flag = 0;
	if (s_exit_flag == 1) {
		// while (1) {
		// 	usleep(1000);
		// }

        return;
	}
	s_exit_flag = 1;

	printf("#####exit_program entry\n");
	printf("exit_class num:%d\n", _exit_list_num);

	for (int i = 0; i < _exit_list_num; i++) {
		_exit_list[i]->invoke_exit_global();
	}

	printf("#####exit_program exit\n");

	exit(0);
}
void ExitHandle::add_exit(Exit *exit)
{
    _exit_list[_exit_list_num] = exit;
    _exit_list_num++;
}
