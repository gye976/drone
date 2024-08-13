
#define SERVER_PORT 8888         // 서버 포트 번호
#define BUF_SIZE 1024            // 버퍼 크기
#define SERVER_IP "10.42.0.1"  // 서버 IP 주소

#include <arpa/inet.h>
#include <stdarg.h>

#include "timer.h"
#include "log.h"

static Pool s_pool(14);

#define TO_SOCKET
//#define TO_FILE

Log *g_log_list[20];
int g_log_list_num = 0;

Log g_mpu6050_log("mpu6050");
Log g_pid_log("pid");
//Log g_dt_log("dt");

Log::Log(const char *name)
	: _name(name)
{
#ifdef TO_FILE
	char path[20];
	sprintf(path, "log/%s.txt", name);

	OPEN_FD(_fd, path, O_WRONLY | O_CREAT | O_TRUNC | O_APPEND, 0666);
#endif

#ifdef TO_SOCKET
    if ((_sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        exit_program();
    }

    memset(&_server_addr, 0, sizeof(_server_addr));
    _server_addr.sin_family = AF_INET;
    _server_addr.sin_port = htons(SERVER_PORT);
    _server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);

	add_log_queue("%s ", _name);
#endif

	_idx_global = g_log_list_num;
	g_log_list[g_log_list_num] = this;	
	g_log_list_num++;
}
Log::~Log()
{
#ifdef TO_FILE
	close(_fd);
#endif
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
}

//DtTrace dt(200);

void Log::write_log_file()
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

	_offset += _buffer_idx;
	_s_idx = (_s_idx + 1) & (N - 1);

	_buffer_idx = 0;
	_buffer[0] = '\0';
}

void Log::write_log_socket()
{
	int ret = sendto(_sockfd, _buffer, _buffer_idx, 0, (const struct sockaddr *) &_server_addr, sizeof(_server_addr));
    if (unlikely(ret < 0)) {
        perror("Send failed");
		exit_program();
    }
	
	//printf("%s\n", _buffer); //

	_offset += _buffer_idx;
	_s_idx = (_s_idx + 1) & (N - 1);

	_buffer_idx = 0;
	_buffer[0] = '\0';

	add_log_queue("%s ", _name);
}

void Log::write_log()
{
#ifdef TO_FILE
	write_log_file();
#endif

#ifdef TO_SOCKET
	write_log_socket();
#endif
}

void Log::flush_log()
{
#ifdef TO_FILE
	int ret = lio_listio(LIO_NOWAIT, _s_aiocb_list, g_log_list_num, NULL); 
	if (unlikely(ret != 0)) { 
		perror("lio_listio"); 
		exit_program(); 
	} 
#endif
}

struct aiocb Log::_s_aiocb[N];
struct aiocb *Log::_s_aiocb_list[N];
int Log::_s_idx = 0;
