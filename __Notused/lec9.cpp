#include "parts.h"

int setnonblocking(int fd) {
	int old_option = fcntl(fd, F_GETFL);
	int new_option = old_option | O_NONBLOCK;
	fcntl(fd, F_SETFL, new_option);
	return old_option;
}
// struct epoll_event
// {
//		__uint32_t events;   epoll�¼����ͱ�ǡ�����ȡ�õ�ʱ����**��λ**�����ֵģ�������ʱ����λ����
//			�޸ģ� |=			�ж��Ƿ����     this.events & target
//		epoll_data_t data;  �û����ݣ��ڲ���Ҫ�ǰ����ļ������� fd
// }
void addfd(int epollfd, int fd, bool enable_et) {
	epoll_event event;
	// �¼����ļ�������
	event.data.fd = fd;
	// �¼������ EPOLLIN ���ݿɶ�
	event.events = EPOLLIN;
	if (enable_et) {
		event.events |= EPOLLET;
	}
	epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
	setnonblocking(fd);
}

// epoll_event[]�� ͨ�� epoll_wait()���ò�ѯ�õ� �Ѿ����ھ���״̬���ļ�����������
void epoll_LT(epoll_event* events, int number, int epollfd, int listenfd) {
	char buffer[BUFFER_SIZE];
	// ����
	for (int i = 0; i < number; i++) {
		int sockfd = events[i].data.fd;
		// �¼��Ǳ������Ķ˿ڵ�---->���µ����ӽ���
		// ---->�������ӣ��������ӵĿͻ��˴���һ���¼���ע�ᵽepoll����ѯ���ݵ���
		if (sockfd == listenfd) {
			struct sockaddr_in client_address;
			socklen_t client_addrlength = sizeof(client_address);
			int connfd = accept(listenfd, (struct sockaddr*)&client_address, &client_addrlength);
			addfd(epollfd, connfd, false);
		}
		else if (events[i].events & EPOLLIN) {
			// 1.������������Ҫ����
			// 2.����¼����ڿɶ�������״̬---->�����ݶ�����
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
			// true --> ETģʽ
			addfd(epollfd, connfd, true);
			// ETģʽͨ�� |= EPOLLET ��λ����Ǵ��� events���ˣ�����Ҫ����ļ��
		}
		else if (events[i].events & EPOLLIN) {
			// EPOLLET & EPOLLIN ��λ�����ˣ���ƽģʽ�Ż�ִ�У���ִ��һ��
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

