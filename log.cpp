#include <stdarg.h>

#include "timer.h"
#include "log.h"

static Pool s_pool(14);

Log *g_log_list[20];
int g_log_list_num = 0;

Log g_mpu6050_log("mpu6050");
Log g_pid_log("pid");
Log g_dt_log("dt");

Log::Log(const char *filename)
{
	char path[20];
	sprintf(path, "log/%s.txt", filename);

	OPEN_FD(_fd, path, O_WRONLY | O_CREAT | O_TRUNC | O_APPEND, 0666);

	_idx_global = g_log_list_num;
	g_log_list[g_log_list_num] = this;	
	g_log_list_num++;
}
Log::~Log()
{
	close(_fd);
}

void Log::add_log_queue(const char *format, ...)
{	
	char buf[100];

    va_list args;
    va_start(args, format);

    vsnprintf(buf, sizeof(buf), format, args);

    va_end(args);

    int len = strlen(buf);

	if (unlikely(_buffer_idx + len >= (int)sizeof(_buffer))) {
		fprintf(stderr, "_buffer[] size overflow err\n");
		printf("%d, %d, %d\n", _buffer_idx, len, (int)sizeof(_buffer));
		exit_program();
	}

	strcat(_buffer + _buffer_idx, buf);
	_buffer_idx += len;

	//printf("str:%s, \n", buf);
	// printf("str:%s, \n", _buffer);
	// printf("add_log_queue1, _buffer_idx:%d, len:%d, \n", _buffer_idx, len);
}

void Log::write_log_queue()
{
	void *ptr = s_pool.get_memory(_buffer_idx);
	memcpy(ptr, _buffer, _buffer_idx);
	memset(&_s_aiocb[_s_idx], 0, sizeof(struct aiocb));

	_s_aiocb[_s_idx].aio_fildes = _fd;
	_s_aiocb[_s_idx].aio_offset = _offset;
	_s_aiocb[_s_idx].aio_buf = ptr;
	_s_aiocb[_s_idx].aio_nbytes = _buffer_idx;
	_s_aiocb[_s_idx].aio_lio_opcode = LIO_WRITE;

	_s_aiocb_list[_idx_global] = &_s_aiocb[_s_idx];
	
	// int ret = aio_write(&_s_aiocb[_s_idx]);
    // if (unlikely(ret != 0)) {
    //     perror("aio_write err");
    //     exit_program();
    // }

	_offset += _buffer_idx;
	_s_idx = (_s_idx + 1) & (N - 1);

	_buffer_idx = 0;
	_buffer[0] = '\0';
}

void Log::flush_log_queue()
{
	int ret = lio_listio(LIO_NOWAIT, _s_aiocb_list, g_log_list_num, NULL); 
	if (unlikely(ret != 0)) { 
		perror("lio_listio"); 
		exit_program(); 
	} 
}

struct aiocb Log::_s_aiocb[N];
struct aiocb *Log::_s_aiocb_list[N];
int Log::_s_idx = 0;
