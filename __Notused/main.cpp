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
#include <functional>

#include "threadpool.h"
#include "http_conn.h"
#include "lst_timer.h"

#define MAX_FD 65536
#define MAX_EVENT_NUMBER 10000
extern int addfd(int epollfd, int fd, bool one_shot);
extern int removefd(int epollfd, int fd); 

int pipefd[2];
time_heap timer;

// 初始化连接状态
http_conn* users = new http_conn[MAX_FD];
int user_count = 0;

void cb_func(util_timer* t) {
	std::cout << "timer : " << t->user_data->sockfd << "  is triggered." << std::endl;
	users[t->user_data->sockfd].close_conn();
	delete t;
}

void addtimer(int connfd) {
	util_timer* t = new util_timer(30);
	client_data* cdata = new client_data;
	cdata->sockfd = connfd;
	if (t && cdata) {
		t->user_data = cdata;
		t->cb_func = cb_func;
		timer.add_timer(t);
		std::cout << "timer : " << t->user_data->sockfd << "  is set" << std::endl;
	}
	else {
		std::cout << "timer add filed.\n";
	}
}

void time_handler(int t) {
	timer.tick();
	alarm(TIMESLOT);
}
void sig_handler(int sig) {
	int save_errno = errno;
	int msg = sig;
	send(pipefd[1], (char*)&msg, 1, 0);
	errno = save_errno;
}

void addsig(int sig) {
	struct sigaction sa;
	memset(&sa, '\0', sizeof sa);
	sa.sa_handler = sig_handler;
	sa.sa_flags |= SA_RESTART;
	sigfillset(&sa.sa_mask);
	assert(sigaction(sig, &sa, NULL) != -1);
}

int main(int argc, char* argv[]) {
	const char* ip = "192.168.3.175";
	int port = atoi("12345");

	// 创建 IPv4 socket 地址
	struct sockaddr_in server_address;
	memset(&server_address, 0, sizeof(server_address));
	server_address.sin_family = AF_INET;
	inet_pton(AF_INET, ip, &server_address.sin_addr);
	server_address.sin_port = htons(port);

	// 设置和绑定监听端口
	int listenfd = socket(PF_INET, SOCK_STREAM, 0);
	assert(listenfd >= 0);
	// bind
	int ret = bind(listenfd, (struct sockaddr*)&server_address, sizeof(server_address));
	assert(listenfd != -1);
	ret = listen(listenfd, 5);
	assert(ret != -1);

	// 初始化线程池
	threadpool<http_conn>* pool = NULL;
	try {
		pool = new threadpool<http_conn>;
	}
	catch (std::exception()) {
		printf("fail to initialize threadpool.\n");
		return 1;
	}

	// 初始化连接状态
	//http_conn* users = new http_conn[MAX_FD];
	//assert(users);
	//int user_count = 0;

	// 设置epoll监听
	epoll_event events[MAX_EVENT_NUMBER];
	int epollfd = epoll_create(5);
	assert(epollfd != -1);
	addfd(epollfd, listenfd, false);
	http_conn::m_epollfd = epollfd;

	// 初始化定时器


	// 注册信号和处理函数
	addsig(SIGALRM);
	signal(SIGALRM, time_handler);
	addsig(SIGTERM);
	//alarm(TIMESLOT);

	// 正式运行
	while (true) {
		int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
		if ((number < 0) && (errno != EINTR)) {
			printf("epoll faliure\n");
			break;
		}
		for (int i = 0; i < number; ++i) {
			int sockfd = events[i].data.fd;
			if (sockfd == listenfd) {
				struct sockaddr_in client_addr;
				socklen_t client_addrlength = sizeof(client_addr);
				int connfd = accept(listenfd, (struct sockaddr*)&client_addr, &client_addrlength);
				if (connfd < 0) {
					printf("errno is: %d \n", errno);
					continue;
				}
				if (http_conn::m_user_count >= MAX_FD) {
					printf("server busy.\n");
					continue;
				}
				users[connfd].init(connfd, client_addr);
				// 5.14 begin
				addtimer(connfd);
				alarm(TIMESLOT);
				// 5.14 end
			}
			else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
				users[sockfd].close_conn();
			}
			else if (events[i].events & EPOLLIN) {
				if (users[sockfd].read()) {
					pool->append(users + sockfd);
					timer.reset(sockfd);
				}
				else {
					users[sockfd].close_conn();
				}
			}
			else if (events[i].events & EPOLLOUT) {
				timer.reset(sockfd);
				if (!users[sockfd].write()) {
					users[sockfd].close_conn();
				}
			}
			else if (sockfd == pipefd[0]) {
				char signals[1024];
				int ret = recv(pipefd[0], signals, sizeof signals, 0);
				for (int j = 0; j < ret; ++j) {
					if (signals[j] == SIGALRM) {
						// 处理定时器到期
						time_handler(0);
					}
				}
			}
		}
	}

	close(epollfd);
	close(listenfd);
	delete[] users;
	delete pool;
	return 0;
}