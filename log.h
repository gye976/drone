#ifndef LOG_H
#define LOG_H

#include <arpa/inet.h>
#include <aio.h>


#define MULTICAST_ADDR "224.0.0.1"
#define PORT 5007

#define BUF_SIZE 1024            // 버퍼 크기
#define	SERVER_IP 	"10.42.0.1"  // 서버 IP 주소
#define	LOCAL_IP 	"10.42.0.187"

#define TTL 1


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

#define N	(1 << 10)
class Log
{
public:	
	Log(const char *name);
	~Log();

	static void flush_log();
	
	void add_log_queue(const char *format, ...);
	void write_log();
	void write_log_file();
	void write_log_socket();

private:
	static struct aiocb _s_aiocb[N];
	static struct aiocb *_s_aiocb_list[N];
	static int _s_idx;

	const char* _name;

	int _idx_global;
	int _fd;
	int _offset = 0;

	char _buffer[2048];
	int _buffer_idx = 0;

	int _sockfd;
    struct sockaddr_in _group_addr;
    struct in_addr _local_ip;
    int _ttl = TTL;
};


#define ADD_LOG_ALL(format, ...) \
do { \
	extern Log *g_log_list[20]; \
	extern int g_log_list_num;	\
\
	for (int i__ = 0; i__ < g_log_list_num; i__++) { \
		g_log_list[i__]->add_log_queue(format, ##__VA_ARGS__); \
	} \
} while(0)


#define ADD_LOG_ARRAY(filename, num, format, data) \
do { \
	for (int i__ = 0; i__ < num - 1; i__++) { \
		ADD_LOG(filename, format, data[i__]); \
		ADD_LOG(filename, ","); \
	} \
	ADD_LOG(filename, format, data[num - 1]); \
	ADD_LOG(filename, " "); \
} while(0)


#define ADD_LOG(name, format, ...) \
do { \
	extern Log g_##name##_log; \
\
	g_##name##_log.add_log_queue(format, ##__VA_ARGS__); \
} while(0)


#define FLUSH_LOG() \
do { \
	extern Log *g_log_list[20]; \
	extern int g_log_list_num;	\
\
	for (int i__ = 0; i__ < g_log_list_num; i__++) { \
		g_log_list[i__]->write_log(); \
	} \
	Log::flush_log(); \
} while(0)


#endif
