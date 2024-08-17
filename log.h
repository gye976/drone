#ifndef LOG_H
#define LOG_H

#include <arpa/inet.h>
#include <liburing.h>
#include <stdarg.h>
#include <aio.h>
#include <pthread.h>

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
	Pool(int n);
	~Pool();

	void *get_memory(int nbytes);

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

#define MANAGER_BUFFER_SIZE	16
class LogSocketManager
{
public:
	LogSocketManager();

	void increase_log_socket(LogSocket *log_socket);
	void add_buffer(LogBuffer *log_buffer);
	void flush_buffer();
	void send();
	void cancel_all_send();
	inline bool check_cancle_flag()
	{
		return _cancle_flag;
	}
	inline void cancle_loggging()
	{
		_cancle_flag = 1;
	}
private:
	LogBuffer _log_buffer[MANAGER_BUFFER_SIZE];
	LogSocket *_log_socket_list[20];
	struct sockaddr_in _group_addr;
	struct in_addr _local_ip;
	struct io_uring _ring;

	int _list_num = 0;
	int _buffer_consume_idx = 0;
	int _buffer_produce_idx = 0;
	int _sockfd;
	int _ttl = TTL;

	bool _cancle_flag = 0;
};

// #define LOGFILE_BUF_SIZE	(1 << 5)
// class LogFile
// {
// public:	
// 	LogFile(const char *name);
// 	~LogFile();

// 	static void flush_log();
	
// 	void add_buffer(LogBuffer *log_buffer);
// 	static void flush_buffer();

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


// #define FLUSH_LOG_FILE() \
// do { \
// 	extern LogFile *g_log_list[20]; \
// \
// 	for (int __i = 0; __i < g_log_list_num; __i++) { \
// 		g_log_list[__i].flush_buffer(); \
// 	} \
// } while(0)


extern LogSocketManager g_log_socket_manager;
pthread_t make_socket_thread(LogSocketManager *log_socket_manager);


#endif
