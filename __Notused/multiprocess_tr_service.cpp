//#include <sys/socket.h>
//#include <netinet/in.h>
//#include <arpa/inet.h>
//#include <assert.h>
//#include <stdio.h>
//#include <unistd.h>
//#include <errno.h>
//#include <string>
//#include <string.h>
//#include <fcntl.h>
//#include <stdlib.h>
//#include <sys/epoll.h>
//#include <signal.h>
//#include <sys/wait.h>
//#include <sys/mman.h>
//#include <sys/stat.h>
//#include <fcntl.h>
//
//
//#define USER_LIMIT 5
//#define BUFFER_SIZE 1024
//#define FD_LIMIT 65535
//#define MAX_EVENT_NUMBER 1024
//#define PROCESS_LIMIT 65536
//
//struct client_data
//{
//	sockaddr_in address;
//	int connfd;
//	pid_t pid;
//	int pipefd[2];
//	// 匿名管道，0读取，1写入，单向。
//};
//
//static const char* shm_name = "/my_shm";
//int sig_pipefd[2];
//int epollfd;
//int listenfd;
//int shmfd;
//char* share_mem = 0;
//client_data* users = 0;
//int* sub_process = 0;
//int user_count = 0;
//bool stop_child = false;
//
//int setnonblocking(int fd) {
//	int old_op = fcntl(fd, F_GETFL);
//	int new_op = old_op | O_NONBLOCK;
//	fcntl(fd, F_SETFL, new_op);
//	return old_op;
//}
//// IO复用 epoll
//void addfd(int epollfd, int fd) {
//	epoll_event event;
//	event.data.fd = fd;
//	event.events = EPOLLIN | EPOLLET;
//	epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
//	setnonblocking(fd);
//}
//// 信号处理
//// 1.多线程可复用，保存errno状态
//void sig_handler(int sig) {
//	int save_erron = errno;
//	int msg = sig;
//	send(sig_pipefd[1], (char*)&msg, 1, 0);
//	errno = save_erron;
//}
//void addsig(int sig, void(*handler)(int), bool restart = true) {
//	struct sigaction sa;
//	memset(&sa, '\0', sizeof(sa));
//	sa.sa_handler = handler;
//	if (restart) {
//		sa.sa_flags |= SA_RESTART;
//	}
//	sigfillset(&sa.sa_mask);
//	assert(sigaction(sig, &sa, NULL) != -1);
//}
//
//void del_resource() {
//	close(sig_pipefd[0]);
//	close(sig_pipefd[1]);
//	close(listenfd);
//	close(epollfd);
//	shm_unlink(shm_name);
//	delete[] users;
//	delete[] sub_process;
//}
//
//void child_term_handler(int sig) {
//	stop_child = true;
//}
//
//int run_child(int idx, client_data* users, char* share_shm) {
//	epoll_event events[MAX_EVENT_NUMBER];
//	// 子进程监听两个：与父进程的pipefd[]，与客户端的users[idx]->fd
//	int child_epollfd = epoll_create(5);
//	assert(child_epollfd != -1);
//	int connfd = users[idx].connfd;
//	addfd(child_epollfd, connfd);
//	int fatherfd = users[idx].pipefd[1];// 1写入
//	addfd(child_epollfd, fatherfd);
//
//	int ret;
//	// 注册子进程结束的时候使用哪个回调函数清理
//	addsig(SIGTERM, child_term_handler, false);
//
//	while (!stop_child) {
//		int number = epoll_wait(child_epollfd, events, MAX_EVENT_NUMBER, -1);
//		if ((number < 0) && (errno != EINTR)) {
//			printf("child epoll faliure\n");
//			break;
//		}
//		for (int i = 0; i < number; i++) {
//			int sockfd = events[i].data.fd;
//			if ((sockfd == connfd) && (events[i].events & EPOLLIN)) {
//				const char* info = "mesaage send.\n-----------------------------------\n";
//				printf("%s", info);
//				send(connfd, info, strlen(info), 0);
//				memset(share_mem + idx * BUFFER_SIZE, '\0', BUFFER_SIZE);
//				ret = recv(connfd, share_mem + idx * BUFFER_SIZE, BUFFER_SIZE - 1, 0);
//				if (ret < 0) {
//					if (errno != EAGAIN) {
//						stop_child = true;
//					}
//				}
//				else if (ret == 0) {
//					stop_child = true;
//				}
//				else {
//					send(fatherfd, (char*)&idx, sizeof(idx), 0);
//				}
//			}
//			else if ((sockfd == fatherfd) && events[i].events & EPOLLIN) {
//				int client = 0;
//				ret = recv(sockfd, (char*)&client, sizeof(client), 0);
//				if (ret < 0) {
//					if (errno != EAGAIN) {
//						stop_child = true;
//					}
//				}
//				else if (ret == 0) {
//					stop_child = true;
//				}
//				else {
//					const char* info = "-----------------------------------\n get mesaage:\n";
//					printf("%s", info);
//					send(connfd, info, strlen(info), 0);
//					send(connfd, share_mem + client * BUFFER_SIZE, BUFFER_SIZE, 0);
//				}
//			}
//			else {
//				continue;
//			}
//		}
//	}
//	close(connfd);
//	close(fatherfd);
//	close(child_epollfd);
//	return 0;
//}
//
//int main(int argc, char* argv[]) {
//	const char* ip = "192.168.3.175";
//	int port = atoi("12345");
//	
//	// 创建 IPv4 socket 地址
//	struct sockaddr_in server_address;
//	memset(&server_address, 0, sizeof(server_address));
//	server_address.sin_family = AF_INET;
//	inet_pton(AF_INET, ip, &server_address.sin_addr);
//	server_address.sin_port = htons(port);
//	
//	int listenfd = socket(PF_INET, SOCK_STREAM, 0);
//	//int reuse = 1;
//	//setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof reuse);
//	assert(listenfd >= 0);
//	
//	// bind
//	int ret = bind(listenfd, (struct sockaddr*)&server_address, sizeof(server_address));
//	assert(listenfd != -1);
//	
//	ret = listen(listenfd, 5);
//	assert(ret != -1);
//
//	user_count = 0;
//	users = new client_data[USER_LIMIT + 1];
//	sub_process = new int[PROCESS_LIMIT];
//	for (int i = 0; i < PROCESS_LIMIT; ++i) {
//		sub_process[i] = -1;
//	}
//
//	epoll_event events[MAX_EVENT_NUMBER];
//	epollfd = epoll_create(5);
//	assert(epollfd != -1);
//	addfd(epollfd, listenfd);
//
//	ret = socketpair(PF_UNIX, SOCK_STREAM, 0, sig_pipefd);
//	assert(ret != -1);
//	setnonblocking(sig_pipefd[1]);
//	addfd(epollfd, sig_pipefd[0]);
//
//	addsig(SIGCHLD, sig_handler);
//	addsig(SIGTERM, sig_handler);
//	addsig(SIGINT, sig_handler);
//	addsig(SIGPIPE, SIG_IGN);
//	bool stop_server = false;
//	bool terminate = false;
//
//	// 创建共享内存
//	shmfd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
//	assert(shmfd != -1);
//	ret = ftruncate(shmfd, USER_LIMIT * BUFFER_SIZE);
//	assert(ret != -1);
//
//	share_mem = (char*)mmap(NULL, USER_LIMIT * BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);
//	assert(share_mem != MAP_FAILED);
//	close(shmfd);
//
//	while (!stop_server) {
//		int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
//
//		if ((number < 0) && (errno != EINTR)) { //EINTR是表示这个文件标识符是阻塞，虽然没读取到东西，但是只是因为被阻塞了，暂时不是出错
//			printf("father epoll faliure\n");
//			break;
//		}
//
//		for (int i = 0; i < number; i++) {
//			int sockfd = events[i].data.fd;
//			if (sockfd == listenfd) { // 客户端发来连接请求
//				sockaddr_in client_addr;
//				socklen_t client_addr_len = sizeof(client_addr);
//				int connfd = accept(listenfd, (sockaddr*)&client_addr, &client_addr_len);
//				if (connfd < 0) {
//					printf("errno is : %d\n", errno);
//					continue;
//				}
//				if (user_count >= USER_LIMIT) {
//					const char* info = "Too many users.Try again later.\n";
//					printf("%s", info);
//					send(connfd, info, strlen(info), 0);
//					close(connfd);
//					continue;
//				}
//
//				users[user_count].address = client_addr;
//				users[user_count].connfd = connfd;
//				ret = socketpair(PF_UNIX, SOCK_STREAM, 0, users[user_count].pipefd);
//				assert(ret != -1);
//
//				pid_t child_pid = fork();
//				if (child_pid < 0) {
//					close(connfd);
//					continue;
//				}
//				else if (child_pid == 0) { // 返回0说明是子线程
//					close(listenfd);
//					close(epollfd);
//					close(users[user_count].pipefd[0]);
//					close(sig_pipefd[0]);
//					close(sig_pipefd[1]);
//					run_child(user_count, users, share_mem);
//					munmap((void*)share_mem, USER_LIMIT * BUFFER_SIZE);
//					exit(0);
//				}
//				else {  // 在父线程中执行
//					// 任务是记录用户
//					close(connfd);
//					close(users[user_count].pipefd[1]);
//					addfd(epollfd, users[user_count].pipefd[0]);
//					users[user_count].pid = child_pid;
//					sub_process[child_pid] = user_count;
//					user_count++;
//				}
//			}
//			else if ((sockfd == sig_pipefd[0]) && (events[i].events & EPOLLIN)) {   // 子线程的信号请求
//				int sig;
//				char signals[1024];
//				ret = recv(sig_pipefd[0], signals, sizeof signals, 0);
//				if (ret == -1) {
//					continue;
//				}
//				else if (ret == 0) {
//					continue;
//				}
//				else {
//					for (int i = 0; i < ret; i++) {
//						switch (signals[i])
//						{
//							case SIGCHLD: // 子进程退出-->客户端离开了
//							{
//								printf("clinet quit.\n");
//								pid_t pid;
//								int stat;
//								while ((pid = waitpid(-1, &stat, WNOHANG)) > 0) {
//									int del_user = sub_process[pid];
//									sub_process[pid] -= 1;
//									if ((del_user < 0) || (del_user > USER_LIMIT)) {
//										continue;
//									}
//									epoll_ctl(epollfd, EPOLL_CTL_DEL, users[del_user].pipefd[0], 0);
//									close(users[del_user].pipefd[0]);
//									users[del_user] = users[--user_count];
//									sub_process[users[del_user].pid] = del_user;
//								}
//								if (terminate && user_count == 0) {
//									stop_server = true;
//								}
//								break;
//							}
//							case SIGTERM:
//							case SIGINT: {
//								printf("kill all child now\n");
//								if (user_count == 0) {
//									stop_server = true;
//									break;
//								}
//								for (int i = 0; i < user_count; ++i) {
//									int pid = users[i].pid;
//									kill(pid, SIGTERM);
//								}
//								terminate = true;
//								break;
//							}
//							default:
//								break;
//						}
//					}
//				}
//			}
//			else if (events[i].events & EPOLLIN) {
//				int child = 0;
//				ret = recv(sockfd, (char*)&child, sizeof child, 0);
//				printf("read data from child across pipe\n");
//				if (ret == -1) {
//					continue;
//				}
//				else if (ret == 0) {
//					continue;
//				}
//				else {
//					for (int j = 0; j < user_count; ++j) {
//						if (users[j].pipefd[0] != sockfd) {
//							printf("send data to child accross pipe\n");
//							send(users[j].pipefd[0], (char*)&child, sizeof(child), 0);
//						}
//					}
//				}
//			}
//			
//		}
//	}
//
//	del_resource();
//	return 0;
//}




//int epollfd = epoll_create(5);
//
//// 定时器回调函数
//void timeout_cb(client_data* user_data) {
//	if (user_data) {
//		epoll_ctl(epollfd, EPOLL_CTL_DEL, user_data->sockfd, NULL);
//		close(user_data->sockfd);  // 关闭套接字
//		std::cout << "Closed connection: " << inet_ntoa(user_data->address.sin_addr) << std::endl;
//		delete user_data->timer;
//		delete user_data;
//	}
//}
//
//
//int main() {
//	int listenfd = socket(AF_INET, SOCK_STREAM, 0);
//	if (listenfd < 0) {
//		std::cerr << "Failed to create socket." << std::endl;
//		return 1;
//	}
//
//	const char* ip = "192.168.3.175";
//	int port = atoi("12345");
//	
//	// 创建 IPv4 socket 地址
//	struct sockaddr_in server_addr;
//	memset(&server_addr, 0, sizeof(server_addr));
//	server_addr.sin_family = AF_INET;
//	inet_pton(AF_INET, ip, &server_addr.sin_addr);
//	server_addr.sin_port = htons(port);
//
//	if (bind(listenfd, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
//		std::cerr << "Bind error." << std::endl;
//		return 1;
//	}
//
//	if (listen(listenfd, 5) < 0) {
//		std::cerr << "Listen error." << std::endl;
//		return 1;
//	}
//
//	time_heap timer_heap;
//	epoll_event events[MAX_EVENT_NUMBER];
//
//	addfd(epollfd, listenfd, true);
//
//	while (true) {
//		timer_heap.tick();
//		int ret = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
//		if (ret < 0) {
//			printf("epoll failure\n");
//			break;
//		}
//		else {
//			for (int i = 0; i < ret; i++) {
//				int sockfd = events[i].data.fd;
//				if (sockfd == listenfd) {
//					sockaddr_in client_addr;
//					socklen_t client_addr_len = sizeof(client_addr);
//					int connfd = accept(listenfd, (sockaddr*)&client_addr, &client_addr_len);
//					setnonblocking(connfd);
//					if (connfd < 0) {
//						std::cerr << "Accept error." << std::endl;
//							continue;
//					}	
//
//					// 创建客户端数据
//					client_data* user_data = new client_data;
//					user_data->address = client_addr;
//					user_data->sockfd = connfd;
//
//					// 创建定时器
//					util_timer* timer = new util_timer(TIMEOUT);
//					timer->user_data = user_data;
//					timer->cb_func = timeout_cb;
//					user_data->timer = timer;
//
//					// 添加到时间堆
//					timer_heap.add_timer(timer);
//					addfd(epollfd, connfd, false);
//					std::cout << "Accepted connection from " << inet_ntoa(client_addr.sin_addr) << std::endl;
//				}
//				else if (events[i].events & EPOLLIN) {
//					printf("event triggerd.\n");
//				}
//			}
//		}
		//sockaddr_in client_addr;
		//socklen_t client_addr_len = sizeof(client_addr);
		//int connfd = accept(listenfd, (sockaddr*)&client_addr, &client_addr_len);
		//if (connfd < 0) {
		//	std::cerr << "Accept error." << std::endl;
		//	continue;
		//}
		//// 创建客户端数据
		//client_data* user_data = new client_data;
		//user_data->address = client_addr;
		//user_data->sockfd = connfd;
		//// 创建定时器
		//util_timer* timer = new util_timer(TIMEOUT);
		//timer->user_data = user_data;
		//timer->cb_func = timeout_cb;
		//user_data->timer = timer;
		//// 添加到时间堆
		//timer_heap.add_timer(timer);
		//std::cout << "Accepted connection from " << inet_ntoa(client_addr.sin_addr) << std::endl;
		//// 处理到期的定时器
		//timer_heap.tick();
//	}
//
//	close(listenfd);
//	return 0;
//}
//int main(int argc, char* argv) {
//
//	const char* ip = "192.168.3.175";
//	int port = atoi("12344");
//
//	// 创建 IPv4 socket 地址
//	struct sockaddr_in server_address;
//	memset(&server_address, 0, sizeof(server_address));
//	server_address.sin_family = AF_INET;
//	inet_pton(AF_INET, ip, &server_address.sin_addr);
//	server_address.sin_port = htons(port);
//
//	int listenfd = socket(PF_INET, SOCK_STREAM, 0);
//	//int reuse = 1;
//	//setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof reuse);
//	assert(listenfd >= 0);
//
//	// bind
//	int ret = bind(listenfd, (struct sockaddr*)&server_address, sizeof(server_address));
//	assert(listenfd != -1);
//
//	ret = listen(listenfd, 5);
//	assert(ret != -1);
//
//	sleep(10);
//
//	// Lec8
//	//struct sockaddr_in client;
//	//socklen_t cilent_addrlength = sizeof(client);
//	//int connfd = accept(sock, (struct sockaddr*)&client, &cilent_addrlength);
//
//	//parsehttp(connfd);
//
//
//	// Lec9
//	epoll_event events[MAX_EVENT_NUMBER];
//	int epollfd = epoll_create(5);
//	assert(epollfd != -1);
//
//	addfd(epollfd, listenfd, true);
//
//	while (1) {
//		int ret = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
//		if (ret < 0) {
//			printf("epoll failure\n");
//			break;
//		}
//		else {
//			//epoll_LT(events, ret, epollfd, listenfd);
//			epoll_ET(events, ret, epollfd, listenfd);
//		}
//	}
//
//	close(listenfd);
//	return 0;
//}