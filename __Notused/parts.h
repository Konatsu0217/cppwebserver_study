#ifndef PARTS
#define PARTS

#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <fcntl.h>
#include <stdlib.h>

#define BUFFER_SIZE 1024
#define MAX_EVENT_NUMBER 1024
#define PORT 12345
#define TIMEOUT 5  // 定时器超时时间（秒）

// lec10
//class client_data
//{
//public:
//	sockaddr_in address;
//	int sockfd;
//	char buf[BUFFER_SIZE];
//	util_timer* timer;
//};

//// lec8
//void parsehttp(int);
//
// lec9
//int setnonblocking(int);
//void addfd(int, int, bool);
//void epoll_LT(epoll_event*, int, int, int);
//void epoll_ET(epoll_event*, int, int, int);

#endif // !PARTS