#include <arpa/inet.h>

#include "log.h"

#include "timer.h"

void *send_socket_loop(void *arg)
{
	LogSocketManager *log_socket_manager = (LogSocketManager*)arg;

	while (log_socket_manager->check_cancle_flag() == 0) {
		log_socket_manager->send();
	}

	return NULL; 
}

pthread_t make_socket_thread(LogSocketManager *log_socket_manager)
{    
	pthread_t thread;
	if (pthread_create(&thread, NULL, send_socket_loop, log_socket_manager) != 0) {
		perror("make_socket_thread ");
		exit_program();
	}
	pthread_setname_np(thread, "gye-socket");

	struct sched_param param = {
		.sched_priority = 75,
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

Pool::Pool(int n)
	: _size_nbytes(1 << n)
{
	_memory = (char*)malloc(sizeof(1) * _size_nbytes);
}
Pool::~Pool()
{
	free(_memory);
}
void *Pool::get_memory(int nbytes) 
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
}

void exit_LogSocketManager(LogSocketManager *log_socket_manager)
{
	log_socket_manager->cancle_loggging();
	log_socket_manager->cancel_all_send();
}
INIT_EXIT(LogSocketManager);

LogSocketManager::LogSocketManager()
{
	ADD_EXIT(LogSocketManager);

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

	// if (pthread_spin_init(&_spinlock) != 0) {
	// 	perror("LogSocketManager, pthread_spin_init");
	// 	exit_program();	
	// }
}
void LogSocketManager::add_buffer(LogBuffer *log_buffer)
{
	_log_buffer[_buffer_produce_idx].add_buffer(log_buffer);
}
void LogSocketManager::increase_log_socket(LogSocket *log_socket)
{
	_log_socket_list[_list_num] = log_socket;
	_list_num++;
}

//LogTime aa(400);

void LogSocketManager::flush_buffer()
{
	int next_produce_idx = (_buffer_produce_idx + 1) & (MANAGER_BUFFER_SIZE - 1);
	if (next_produce_idx == _buffer_consume_idx) {
		printf("consumer is slow, bug\n");
		exit_program();

		return;
	}

	//aa.update_prev_time();
	for (int i = 0; i < _list_num; i++) {
		_log_socket_list[i]->add_buffer("\n", 1);
		_log_socket_list[i]->write_buffer();

		_log_socket_list[i]->clear_buffer();
	}
	//aa.update_cur_time();
	//aa.ff();

	//printf("produce idx: %d\n", _buffer_produce_idx);
	_buffer_produce_idx = next_produce_idx;
}
void LogSocketManager::send()
{
	int ret;
	char *buf;
	int buf_idx;
	struct io_uring_sqe *sqe;
	//struct io_uring_cqe *cqe;

	if (_buffer_produce_idx == _buffer_consume_idx) {
		//printf("producer slow\n");
		usleep(200);
		return;
	}

	buf = _log_buffer[_buffer_consume_idx].get_buffer();
	buf_idx = _log_buffer[_buffer_consume_idx].get_idx();

	// int space_left = io_uring_sq_space_left(&_ring);
	// if (space_left > 0) {
	// } else {
	// 	printf("송신 큐가 꽉 찼습니다. 더 이상 SQE를 추가할 수 없습니다.\n");
	// 	goto CLEARs;
	// }

	sqe = io_uring_get_sqe(&_ring);
	sqe->user_data = (__u64)NULL;
		
	io_uring_prep_send(sqe, _sockfd, buf, buf_idx, 0); 

	ret = io_uring_submit(&_ring);
	if (unlikely(ret < 0)) {
		perror("io_uring_submit err");
		exit_program();
	} else if (unlikely(ret == 0)) {
		perror("io_uring submit ret == 0\n");		
		exit_program();
	}

	//_log_buffer.print_buffer();
	_log_buffer[_buffer_consume_idx].clear_buffer();

	//printf("consume idx: %d\n", _buffer_consume_idx);
	_buffer_consume_idx = (_buffer_consume_idx + 1) & (MANAGER_BUFFER_SIZE - 1);
}
void LogSocketManager::cancel_all_send()
{
#define IORING_ASYNC_CANCEL_ALL	(1U << 0)

	struct io_uring_sqe *sqe;
	sqe = io_uring_get_sqe(&_ring);
	io_uring_prep_cancel(sqe, NULL, IORING_ASYNC_CANCEL_ALL);
	
	int ret = io_uring_submit(&_ring);
	if (unlikely(ret < 0)) {
		perror("cancel_all_send, io_uring_submit err");
	} else if (unlikely(ret == 0)) {
		perror("cancel_all_send, io_uring submit ret == 0\n");		
	}

	io_uring_queue_exit(&_ring);
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
