#include <stdarg.h>

#include "timer.h"
#include "log.h"

class Pool
{
public:
	Pool(int n) 
		: _size_nbytes(1 << n)
	{
		_memory = (char*)malloc(sizeof(1) * _size_nbytes);
	}
	~Pool()
	{
		free(_memory);
	}

	void *get_memory(int nbytes) 
	{
		if (_idx + nbytes >= _size_nbytes) {
			_idx = nbytes;
			return _memory;
		} else {
			void *ptr = _memory + _idx;

			_idx += nbytes;
			return ptr; 
		}
	}

private:
	char *_memory;

	int _size_nbytes;
	int _idx;
};

static Pool s_pool(14);

Log g_mpu6050_log("mpu6050");
Log g_pid_log("pid");

Log::Log(const char *filename)
{
	char path[20];
	sprintf(path, "log/%s.txt", filename);

	OPEN_FD(_fd, path, O_WRONLY | O_CREAT | O_TRUNC | O_APPEND, 0666);
}
Log::~Log()
{
	close(_fd);
}

void Log::add_log(const char *format, ...)
{	
    char buf[100];

    va_list args;
    va_start(args, format);

    vsnprintf(buf, sizeof(buf), format, args);

    va_end(args);

    int len = strlen(buf);

	void *ptr = s_pool.get_memory(len);

    memcpy(ptr, buf, len);

	memset(&_aiocb[_num], 0, sizeof(struct aiocb));
	
	_aiocb[_num].aio_fildes = _fd;
	_aiocb[_num].aio_offset = _offset;
	_aiocb[_num].aio_buf = ptr;
	_aiocb[_num].aio_nbytes = len;
	
    if (aio_write(&_aiocb[_num]) != 0) {
        perror("aio_write err");
        exit_program();
    }

    //printf("len:%d, _offset:%d, _num:%d\n", len, _offset, _num, (const char*)ptr);
    //printf("%s\n", (const char*)ptr);

	_offset += len;
	_num = (_num + 1) & (N - 1);
}

