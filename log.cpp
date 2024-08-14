#include <arpa/inet.h>


#include "timer.h"
#include "log.h"

pthread_t make_socket_thread(LogSocketManager *log_socket_manager)
{    
	pthread_t thread;
	if (pthread_create(&thread, NULL, send_socket, log_socket_manager) != 0) {
		perror("make_socket_thread ");
		exit_program();
	}
	pthread_setname_np(thread, "gye-socket");

	struct sched_param param = {
		.sched_priority = 99,
	};
	if (pthread_setschedparam(thread, SCHED_RR, &param) != 0)
	{
		perror("make_socket_thread setschedparam");
		exit_program();
	}

	return thread;
}

static Pool s_pool(14);

LogSocketManager g_log_socket_manager;

LogSocket g_mpu6050_log_socket("mpu6050");
LogSocket g_pid_log_socket("pid");

void LogBuffer::add_buffer(const char* buf, int len)
{
	if (unlikely(_buffer_idx + len >= (int)sizeof(_buffer))) {
		fprintf(stderr, "_buffer[%d] size overflow err\n", LOG_BUFFER_SIZE);
		printf("%d, %d, %d\n", _buffer_idx, len, (int)sizeof(_buffer));
		printf("cur buf:%s\nadding buf:%s", _buffer, buf);
		exit_program();
	}

	strcat(_buffer + _buffer_idx, buf);
	_buffer_idx += len;
}
void LogBuffer::add_buffer(const LogBuffer *log_buffer)
{
	const char* buf = log_buffer->_buffer;
	int len = log_buffer->_buffer_idx;

	add_buffer(buf, len);
}
void LogBuffer::clear_buffer()
{
	_buffer[0] = '\0';
	_buffer_idx = 0;
}
void LogBuffer::print_buffer()
{
	printf("buf/num: %s/%d\n", _buffer, _buffer_idx);
}
LogSocket::LogSocket(const char *name)
	: _log_socket_manager(&g_log_socket_manager)
	, _name(name), _name_len(strlen(name))
{
	_log_socket_manager->increase_log_socket(this);
	_log_buffer.add_buffer(_name, _name_len);
	_log_buffer.add_buffer(" ", 1);
}

void LogSocket::add_buffer(const char* buf, int len)
{
	_log_buffer.add_buffer(buf, len);
}
void LogSocket::clear_buffer()
{
	_log_buffer.clear_buffer();
	_log_buffer.add_buffer(_name, _name_len);
	_log_buffer.add_buffer(" ", 1);
}
void LogSocket::write_buffer()
{
	_log_socket_manager->add_buffer(&_log_buffer);

	_log_buffer.clear_buffer();
}

LogSocketManager::LogSocketManager()
{
	if ((_sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		perror("Socket creation failed");
		exit_program();
	}
		// 송신자 로컬 IP 주소 설정
	if (inet_aton(LOCAL_IP, &_local_ip) == 0) {
	perror("inet_aton");
		close(_sockfd);
		exit_program();
	}

	if (setsockopt(_sockfd, IPPROTO_IP, IP_MULTICAST_IF, &_local_ip, sizeof(_local_ip)) < 0) {
		perror("setsockopt (IP_MULTICAST_IF)");
		close(_sockfd);
		exit_program();
	}

	// 멀티캐스트 TTL 설정
	if (setsockopt(_sockfd, IPPROTO_IP, IP_MULTICAST_TTL, &_ttl, sizeof(_ttl)) < 0) {
		perror("setsockopt (IP_MULTICAST_TTL)");
		close(_sockfd);
		exit_program();
	}

		// 멀티캐스트 주소 설정
	memset(&_group_addr, 0, sizeof(_group_addr));
	_group_addr.sin_family = AF_INET;
	_group_addr.sin_addr.s_addr = inet_addr(MULTICAST_ADDR);
	_group_addr.sin_port = htons(PORT);
	if (connect(_sockfd, (struct sockaddr *)&_group_addr, sizeof(_group_addr)) < 0) {
		perror("connect");
		close(_sockfd);
		exit_program();
	}

	int ret = io_uring_queue_init(32, &_ring, 0);
	if (ret < 0) {
		perror("io_uring_queue_init");
		close(_sockfd);
		exit_program();
	}
}
void LogSocketManager::add_buffer(LogBuffer *log_buffer)
{
	_log_buffer.add_buffer(log_buffer);
}
void LogSocketManager::increase_log_socket(LogSocket *log_socket)
{
	_log_socket_list[_list_num] = log_socket;
	_list_num++;
}

LogTime log_socket_time(1000);

void LogSocketManager::flush_buffer()
{
	for (int i = 0; i < _list_num; i++) {
		_log_socket_list[i]->write_buffer();
	}

	char *buf = _log_buffer.get_buffer();
	int buf_idx = _log_buffer.get_idx();

	struct io_uring_sqe *sqe;
	//struct io_uring_cqe *cqe;
	sqe = io_uring_get_sqe(&_ring);
	
	io_uring_prep_send(sqe, _sockfd, buf, buf_idx, 0); 

	log_socket_time.update_prev_time();

	int ret = io_uring_submit(&_ring);
	if (unlikely(ret < 0)) {
		perror("io_uring_submit err");
		exit_program();
	} else if (ret == 0) {
		perror("io_uring submit ret == 0\n");
		exit_program();
	}

	log_socket_time.update_cur_time();
	log_socket_time.ff();

	//_log_buffer.print_buffer();
	_log_buffer.clear_buffer();

	for (int i = 0; i < _list_num; i++) {
		_log_socket_list[i]->clear_buffer();
	}
}


// Log g_dt_log("dt");

// LogFile::LogFile(const char *name)
// 	: _name(name)
// {
// 	char path[20];
// 	sprintf(path, "log/%s.txt", name);

// 	OPEN_FD(_fd, path, O_WRONLY | O_CREAT | O_TRUNC | O_APPEND, 0666);

// 	_idx_global = g_log_list_num;
// 	g_log_list[g_log_list_num] = this;	
	
// 	g_log_list_num++;
// }

// LogFile::~LogFile()
// {
// 	close(_fd);
// }

// void LogFile::flush_log()
// {
// 	int ret = lio_listio(LIO_NOWAIT, _s_aiocb_list, g_log_list_num, NULL); 
// 	if (unlikely(ret != 0)) { 
// 		perror("lio_listio"); 
// 		exit_program(); 
// 	} 
// }
// void LogFile::write_log_file()
// {
// 	void *ptr = s_pool.get_memory(_buffer_idx);
// 	memcpy(ptr, _buffer, _buffer_idx);
// 	memset(&_s_aiocb[_s_idx], 0, sizeof(struct aiocb));

// 	_s_aiocb[_s_idx].aio_fildes = _fd;
// 	_s_aiocb[_s_idx].aio_offset = _offset;
// 	_s_aiocb[_s_idx].aio_buf = ptr;
// 	_s_aiocb[_s_idx].aio_nbytes = _buffer_idx;
// 	_s_aiocb[_s_idx].aio_lio_opcode = LIO_WRITE;

// 	_s_aiocb_list[_idx_global] = &_s_aiocb[_s_idx];

// 	_offset += _buffer_idx;
// 	_s_idx = (_s_idx + 1) & (LOGFILE_BUF_SIZE - 1);

// 	_buffer_idx = 0;
// 	_buffer[0] = '\0';
// }

// struct aiocb Log::_s_aiocb[LOGFILE_BUF_SIZE];
// struct aiocb *Log::_s_aiocb_list[LOGFILE_BUF_SIZE];
// int Log::_s_idx = 0;
