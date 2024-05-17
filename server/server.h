#pragma once
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/epoll.h>

#include "../threadpool/threadpool.h"
#include "../http_conn/http_conn.h"
#include "../timer/lst_timer.h"
#include "../logger/logger.h"

extern int addfd(int epollfd, int fd, bool one_shot);
extern int removefd(int epollfd, int fd);

class myServer {
// 单例模式
private:
	myServer() {
		initServer();
	};
	~myServer() {
		close(m_epollfd);
		close(m_listenfd);
		delete[] users;
		delete[] pipefd;
		delete timer;
		delete m_pool;
		delete[] m_events;
	}
	myServer(const myServer&);
	myServer& operator=(const myServer&)=delete;
public:
	static myServer& getInstance() {
		static myServer inst;
		return inst;
	}
/*===============================================================*/
private:
	static int MAX_FD;
	static int MAX_EVENT_NUMBER;
	mutable char* ip = "192.168.3.175";
	mutable int port = atoi("12345");

public:
	// 读取配置文件，修改Mutable基础参数
	const void initProperty(const char*);
	void runServer(int useLog);
private:
	int createConnection();
	int closeConnection(int sockfd);
	int readMessage(int sockfd);
	int sendMessage(int sockfd);
	int handleSignals();

private:
	int m_listenfd;
	int m_ret;
	int m_epollfd;
	sockaddr_in m_server_address;
	log* logger;
	int m_close_log;

	threadpool<http_conn> *m_pool;	// new
	int m_user_count;
	epoll_event* m_events; // new[]

	static http_conn* users;
	static int* pipefd;
	static time_heap* timer;
	int m_timerSLOT;
	friend void timer_cb_func(util_timer* t);
public:
	void initServer();
private:
	bool initSocket();
	
	bool initListen();

	bool initThreadpool();

	bool initEpoll();

	void addtimer(int connfd);

	friend void regist_signals();

	friend void time_handler(int t);

	friend void sig_handler(int sig);

	friend void addsig(int sig);
};