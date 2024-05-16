#ifndef HTTP_CONN_H

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <iostream>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/uio.h>

#include "../locker/locker.h"
//#include "locker/locker.h"
//#include "lst_timer.h"

class util_timer;
class timer_heap;


class http_conn {
public:
	static const int FILENAME_LEN = 200;
	static const int READ_BUFFER_SIZE = 2048;
	static const int WRITE_BUFFER_SIZE = 1024;
	// ö������http��������
	enum METHOD {
		GET = 0, POST, HEAD, PUT, DELETE,TRACE,OPTIONS,CONNECT,PATH
	};
	enum CHECK_STATE
	{
		CHECK_STATE_REQUESTLINE = 0,
		CHECK_STATE_HEADER,
		CHECK_STATE_CONTENT
	};
	enum HTTP_CODE
	{
		NO_REQUEST,
		GET_REQUEST,
		BAD_REQUEST,
		NO_RESOURCE,
		FORBIDDEN_REQUEST,
		FILE_REQUEST,
		INTERNAL_ERROR,
		CLOSED_CONNECTION
	};
	enum LINE_STATUS
	{
		LINE_OK = 0,
		LINE_BAD,
		LINE_OPEN
	};

public:
	http_conn() {};
	~http_conn() {};

public:
	// �����ӳ�ʼ��
	void init(int sockfd, const sockaddr_in& client_addr);
	// �ر�����
	void close_conn(bool read_close = true);
	// �ͻ�������<----���̳߳ص��õ������Ĺ���
	void process();
	// I/O������������
	bool read();
	bool write();

private:
	void init();
	HTTP_CODE process_read();
	bool process_write(HTTP_CODE ret);

	// process_read uilts
	HTTP_CODE parse_request_line(char* buffer);
	HTTP_CODE parse_headers(char* buffer);
	HTTP_CODE parse_content(char* buffer);
	HTTP_CODE do_request();
	char* get_line() { return m_read_buf + m_start_line; }
	LINE_STATUS parse_line();

	// process_write uilts
	void unmap();
	bool add_response(const char* format, ...);
	bool add_content(const char* content);
	bool add_status_line(int status, const char* title);
	bool add_headers(int content_length);
	bool add_content_length(int content_length);
	bool add_linger();
	bool add_blank_line();

public:
	// ȫ�ֵ�epollfd
	static int m_epollfd;
	static int m_user_count;

private:
	int m_sockfd;
	sockaddr_in m_address;

	char m_read_buf[READ_BUFFER_SIZE];
	int m_read_idx;
	int m_check_idx;
	int m_start_line;
	
	char m_write_buf[WRITE_BUFFER_SIZE];
	int m_write_idx;
	
	// ����״̬
	CHECK_STATE m_check_state;
	// HTTP��������

public:
	METHOD m_method;
	char* m_url;    // ����·��
	void getAddr(sockaddr_in*);

private:
	char* m_version;// http�汾
	bool m_linger;  // �����ӣ�
	char* m_host;		// ������
	int m_content_length; // ��Ϣ�峤��

	// �ͻ������������·��
	char m_real_fpath[FILENAME_LEN];
	// �����ļ�mmap���ڴ����ʼ��ַ
	char* m_file_address;
	// ������ļ�״̬�����ڣ��ɶ�����С��
	struct stat m_file_stat;

	// ʹ��writev����д
	struct iovec m_iv[2];
	int m_iv_count;
};

#define HTTP_CONN_H
#endif // !HTTP_CONN_H