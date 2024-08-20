#ifndef DEBUG_H
#define DEBUG_H

#ifdef _DEBUG_ 

// extern bool g_debug_flag;
// extern bool g_trace_func_flag;

#define mark_func(func, ...) \
do { \
    printf("------"#func" entry\n"); \
    func(__VA_ARGS__); \
    printf("------"#func" exit\n"); \
 } while(0)


    #define dbg_print(fmt, ...) \
    do { \
        if (g_debug_flag == true) { \
            printf(fmt, ##__VA_ARGS__); \
        } \
    } while (0)

    #define dbg_func(func, ...) \
    do { \
        if (g_debug_flag == true) { \
            func(__VA_ARGS__); \
        } \
    } while (0)
#else
    #define pr_debug(fmt, ...) do {} while(0)

#endif



#endif 

