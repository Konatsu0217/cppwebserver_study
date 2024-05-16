#include "parts.h"

int setnonblocking(int fd) {
	int old_option = fcntl(fd, F_GETFL);
	int new_option = old_option | O_NONBLOCK;
	fcntl(fd, F_SETFL, new_option);
	return old_option;
}
// struct epoll_event
// {
//		__uint32_t events;   epoll事件类型标记。具体取用的时候是**按位**来区分的，操作的时候都是位运算
//			修改： |=			判断是否相等     this.events & target
//		epoll_data_t data;  用户数据，内部主要是包含文件描述符 fd
// }
void addfd(int epollfd, int fd, bool enable_et) {
	epoll_event event;
	// 事件的文件描述符
	event.data.fd = fd;
	// 事件的类别， EPOLLIN 数据可读
	event.events = EPOLLIN;
	if (enable_et) {
		event.events |= EPOLLET;
	}
	epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
	setnonblocking(fd);
}

// epoll_event[]是 通过 epoll_wait()调用查询得到 已经处于就绪状态的文件描述符数组
void epoll_LT(epoll_event* events, int number, int epollfd, int listenfd) {
	char buffer[BUFFER_SIZE];
	// 遍历
	for (int i = 0; i < number; i++) {
		int sockfd = events[i].data.fd;
		// 事件是被监听的端口的---->有新的连接建立
		// ---->建立连接，并该连接的客户端创建一个事件，注册到epoll的轮询内容当中
		if (sockfd == listenfd) {
			struct sockaddr_in client_address;
			socklen_t client_addrlength = sizeof(client_address);
			int connfd = accept(listenfd, (struct sockaddr*)&client_address, &client_addrlength);
			addfd(epollfd, connfd, false);
		}
		else if (events[i].events & EPOLLIN) {
			// 1.不是有新连接要建立
			// 2.这个事件处于可读就绪的状态---->把数据读出来
			printf("event trigger once\n");
			memset(buffer, '\0', BUFFER_SIZE);
			int ret = recv(sockfd, buffer, BUFFER_SIZE - 1, 0);
			if (ret <= 0) {
				close(sockfd);
			}
			printf("get %d bytes of content: %s\n", ret, buffer);
		}
		else {
			printf("wtf, LT\n");
		}
	}
}

void epoll_ET(epoll_event* events, int number, int epollfd, int listenfd) {
	char buffer[BUFFER_SIZE];
	for (int i = 0; i < number; i++) {
		int sockfd = events[i].data.fd;
		if (sockfd == listenfd) {
			struct sockaddr_in client_address;
			socklen_t client_addrlength = sizeof(client_address);
			int connfd = accept(listenfd, (struct sockaddr*)&client_address, &client_addrlength);
			// true --> ET模式
			addfd(epollfd, connfd, true);
			// ET模式通过 |= EPOLLET 按位将标记打在 events上了，不需要额外的检查
		}
		else if (events[i].events & EPOLLIN) {
			// EPOLLET & EPOLLIN 按位处理了，电平模式才会执行，仅执行一次
			printf("event triggerd. ET mode.\n");
			while (1) {
				memset(buffer, '\0', BUFFER_SIZE);
				int ret = recv(sockfd, buffer, BUFFER_SIZE - 1, 0);
				if (ret < 0) {
					if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
						printf("read later\n");
						break;
					}
					close(sockfd);
					break;
				}
				else if (ret == 0) {
					close(sockfd);
				}
				else {
					printf("get %d bytes of content: %s\n", ret, buffer);
				}
			}
		}
		else {
			printf("wtf, ET\n");
		}
	}
}

