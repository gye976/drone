#ifndef EXIT_HANDLE_H
#define EXIT_HANDLE_H

class ExitHandle;

class Exit
{
public:
	Exit(ExitHandle *exit_handle, void (*func)());

    void add_exit_instance(void *inst);
    void invoke_exit_global();
    
    ExitHandle *_exit_handle;
    void (*_exit_func)();

    void *_instance_list[10];
    int _instance_num = 0;
};

class ExitHandle
{
public:
    void invoke_exit_program();
    void add_exit(Exit *exit);

private:
    Exit *_exit_list[20];
    int _exit_list_num = 0;
};


extern ExitHandle g_exit_handle;



#define DEFINE_EXIT(type) \
extern Exit __exit_##type; \
\
void exit_##type##_global() \
{ \
    int __instance_num = __exit_##type._instance_num; \
    void ** __instance_list = __exit_##type._instance_list; \
\
	printf("    @@@@@exit_"#type" entry\n"); \
	printf("    "#type" inst num: %d\n", __instance_num); \
\
    for (int __i = 0; __i < __instance_num; __i++) { \
	    printf("    "#type" inst ptr:%p\n", __instance_list[__i]); \
    } \
    for (int __i = 0; __i < __instance_num; __i++) { \
        type *__inst = (type*)(__instance_list[__i]); \
\
	    printf("        =====exit_"#type" instance%d entry\n", __i); \
\
        int ret = exit_##type(__inst); \
	    if (ret != 0) { \
            printf("!!!!!!!!!!!!!exit error!!!!!!!!!!!!!!!\n"); \
        } \
\
        printf("        =====exit_"#type" instance%d exit\n\n", __i); \
    } \
\
    printf("    @@@@@exit_"#type" exit\n\n"); \
} \
\
Exit __exit_##type(&g_exit_handle, exit_##type##_global) 



#define INIT_EXIT_IN_CTOR(type) \
do { \
	extern Exit __exit_##type; \
\
    __exit_##type.add_exit_instance(this); \
} while (0)



#define exit_program() \
do { \
    printf("file:%s, func:%s, line:%d\n\n", __FILE__, __FUNCTION__, __LINE__); \
	g_exit_handle.invoke_exit_program(); \
} while (0)
    

#endif