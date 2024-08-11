#ifndef LOG_H
#define LOG_H

#include <aio.h>

#define N	(1 << 10)
class Log
{
public:
	Log(const char *filename);
	~Log();
	void add_log(const char *format, ...);

private:
	int _fd;
	int _offset = 0;

	struct aiocb _aiocb[N];

	struct aiocb *empty_idx[N];
	int _num = 0;
};

#define ADD_LOG_ARRAY(filename, num, format, data) \
do { \
	for (int _i = 0; _i < num - 1; _i++) { \
		ADD_LOG(filename, format, data[_i]); \
		ADD_LOG(filename, ","); \
	} \
	ADD_LOG(filename, format, data[num - 1]); \
	ADD_LOG(filename, " "); \
} while(0)


#define ADD_LOG(filename, format, ...) \
do { \
	extern Log g_##filename##_log; \
\
	g_##filename##_log.add_log(format, ##__VA_ARGS__); \
} while(0)

#endif
