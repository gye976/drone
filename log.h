#ifndef LOG_H
#define LOG_H

#include <arpa/inet.h>
#include <liburing.h>
#include <stdarg.h>
#include <aio.h>


#define MULTICAST_ADDR "224.0.0.1"
#define PORT 5007

#define	SERVER_IP 	"10.42.0.1"  // 서버 IP 주소
#define	LOCAL_IP 	"10.42.0.187"

#define TTL 1

#define PARSE_STR_FORMAT(buffer, len, format, ...) \
do { \
	len = snprintf(buffer, sizeof(buffer), format, ##__VA_ARGS__); \
} while(0)

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

#define LOG_BUFFER_SIZE	1024 * 2
class LogBuffer
{
public:
	inline char *get_buffer()
	{
		return _buffer;
	}
	inline int get_idx()
	{
		return _buffer_idx;
	}
	void add_buffer(const char* buf, int len);
	void add_buffer(const LogBuffer *log_buffer);
	void clear_buffer();
	void print_buffer();
private:
	char _buffer[LOG_BUFFER_SIZE] = { 0, };
	int _buffer_idx = 0;
};

class LogSocketManager;

class LogSocket
{
public:
	LogSocket(const char *name);

	void add_buffer(const char* buf, int len);
	void write_buffer();
	void clear_buffer();
	void increase_log_socket();

private:
	LogSocketManager *_log_socket_manager;
	const char* _name;
	int _name_len;
	LogBuffer _log_buffer;
};

class LogSocketManager
{
public:
	LogSocketManager();

	void increase_log_socket(LogSocket *log_socket);
	void add_buffer(LogBuffer *log_buffer);
	void flush_buffer();

private:
	LogSocket *_log_socket_list[20];
	int _list_num = 0;

	LogBuffer _log_buffer;

	int _sockfd;
	struct sockaddr_in _group_addr;
	struct in_addr _local_ip;
	int _ttl = TTL;
	struct io_uring _ring;
};

// #define LOGFILE_BUF_SIZE	(1 << 5)
// class LogFile
// {
// public:	
// 	Log(const char *name);
// 	~Log();

// 	static void flush_log();
	
// 	void add_log_queue(const char *format, ...);
// 	void write_log();
// 	void write_log_file();
// 	void write_log_socket();

// private:
// 	static struct aiocb _s_aiocb[LOGFILE_BUF_SIZE];
// 	static struct aiocb *_s_aiocb_list[LOGFILE_BUF_SIZE];
// 	static int _s_idx;

// 	const char* _name;

// 	int _idx_global;
// 	int _fd;
// 	int _offset = 0;

// 	char _buffer[2048];
// 	int _buffer_idx = 0;
// };




#define ADD_LOG_ARRAY_SOCKET(name, num, format, data) \
do { \
	for (int i__ = 0; i__ < num - 1; i__++) { \
		ADD_LOG_SOCKET(name, format, data[i__]); \
		ADD_LOG_SOCKET(name, ","); \
	} \
	ADD_LOG_SOCKET(name, format, data[num - 1]); \
	ADD_LOG_SOCKET(name, " "); \
} while(0)


#define ADD_LOG_ALL_SOCKET(format, ...) \
do { \
	extern LogSocketManager g_log_socket_manager; \
	char buf[1024]; \
	int len; \
\
	PARSE_STR_FORMAT(buf, len, format, ##__VA_ARGS__); \
	g_log_socket_manager.add_buffer(buf, len); \
} while(0)


#define ADD_LOG_SOCKET(name, format, ...) \
do { \
	extern LogSocket g_##name##_log_socket; \
	char buf[1024]; \
	int len; \
\
	PARSE_STR_FORMAT(buf, len, format, ##__VA_ARGS__); \
	g_##name##_log_socket.add_buffer(buf, len); \
} while(0)


#define FLUSH_LOG_SOCKET() \
do { \
	extern LogSocketManager g_log_socket_manager; \
\
	g_log_socket_manager.flush_buffer(); \
} while(0)


#endif
