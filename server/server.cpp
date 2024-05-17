#include "server.h"

int myServer::MAX_FD = 65536;
int myServer::MAX_EVENT_NUMBER = 10000;
http_conn* myServer::users = new http_conn[myServer::MAX_FD];
int* myServer::pipefd = new int[2];
time_heap* myServer::timer = new time_heap;

//myServer::myServer()
//{
//}
void time_handler(int t) {
	myServer::timer->tick();
	// 每次重设定时器以最近会过期的定时器的剩余时长更新
	int adaptTIMESOLT;
	if(myServer::timer->getTopTime(adaptTIMESOLT))
		alarm(adaptTIMESOLT);
	else alarm(TIMESLOT);
}

void sig_handler(int sig) {
	int save_errno = errno;
	int msg = sig;
	send(myServer::pipefd[1], (char*)&msg, 1, 0);
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

void regist_signals() {
	addsig(SIGALRM);
	signal(SIGALRM, time_handler);
	addsig(SIGTERM);
}

const void myServer::initProperty(const char* property = nullptr)
{
	if (!property) {
		return;
	}
	else {
		// todo 配置文件解析器
	}
}

void myServer::runServer(int useLog)
{
	if (!useLog)
		m_close_log = 0;

	while (1) {
		int number = epoll_wait(m_epollfd, m_events, MAX_EVENT_NUMBER, -1);
		if ((number < 0) && (errno != EINTR)) {
			printf("epoll faliure\n");
			break;
		}
		for (int i = 0; i < number; ++i) {
			int sockfd = m_events[i].data.fd;
			if (sockfd == m_listenfd) {
				createConnection();
			}
			else if (m_events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
				closeConnection(sockfd);
			}
			else if (m_events[i].events & EPOLLIN) {
				readMessage(sockfd);
			}
			else if (m_events[i].events & EPOLLOUT) {
				sendMessage(sockfd);
			}
			else if (sockfd == pipefd[0]) {
				handleSignals();
			}
		}
	}
}

inline int myServer::createConnection() {
	struct sockaddr_in client_addr;
	socklen_t client_addrlength = sizeof(client_addr);
	int connfd = accept(m_listenfd, (struct sockaddr*)&client_addr, &client_addrlength);
	if (connfd < 0) {
		LOG_ERROR("%s:errno is:%d.", "accept error", errno);
		//printf("errno is: %d \n", errno);
		return 1;
	}
	if (http_conn::m_user_count >= MAX_FD) {
		LOG_ERROR("%s", "Internal server busy.");
		//printf("server busy.\n");
		return 2;
	}
	char client_ip[INET_ADDRSTRLEN] = { 0 };
	LOG_INFO("client %d (%s:%d) connected.",connfd, inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip)),
		ntohs(client_addr.sin_port));
	users[connfd].init(connfd, client_addr);
	// 5.14 begin
	addtimer(connfd);
	alarm(TIMESLOT);
	return 0;
}

inline int myServer::closeConnection(int sockfd) {
	LOG_INFO("client %d connection closed.", sockfd);
	users[sockfd].close_conn();
	return 0;
}

inline int myServer::readMessage(int sockfd) {
	if (users[sockfd].read()) {
		LOG_INFO("read mssage from client %d.", sockfd);
		m_pool->append(users + sockfd);
		timer->reset(sockfd);
	}
	else {
		closeConnection(sockfd);
		//users[sockfd].close_conn();
	}
	return 0;
}

inline int myServer::sendMessage(int sockfd) {
	if (!users[sockfd].write()) {
		users[sockfd].close_conn();
	}
	else {
		timer->reset(sockfd);
		LOG_INFO("send %s to client %d. timer reset.", users[sockfd].m_url, sockfd);
	}
	return 0;
}

inline int myServer::handleSignals() {
	char signals[1024];
	int ret = recv(pipefd[0], signals, sizeof signals, 0);
	for (int j = 0; j < ret; ++j) {
		// 定时器
		if (signals[j] == SIGALRM) {
			time_handler(0);
		}
	}
	return 0;
}

void myServer::initServer()
{
	logger = log::getInstance();
	logger->init("./ServerLog", 1, 2000, 800000, 800);

	LOG_INFO("=========================================================");
	LOG_INFO(" ");
	LOG_INFO("                   Starting Server!                      ");
	LOG_INFO(" ");
	LOG_INFO("=========================================================");

	initSocket();
	initListen();
	initThreadpool();
	initEpoll();
	regist_signals();
}

inline bool myServer::initSocket() {
	LOG_INFO("server socket init %s : %d", ip, port);
	memset(&m_server_address, 0, sizeof m_server_address);
	m_server_address.sin_family = AF_INET;
	inet_pton(AF_INET, ip, &m_server_address.sin_addr);
	m_server_address.sin_port = htons(port);

	return true;
}

inline bool myServer::initListen() {
	m_listenfd = socket(PF_INET, SOCK_STREAM, 0);
	assert(m_listenfd >= 0);
	m_ret = bind(m_listenfd, (struct sockaddr*)&m_server_address, sizeof(m_server_address));
	assert(m_listenfd != -1);
	m_ret = listen(m_listenfd, 5);
	assert(ret != -1);
	LOG_INFO("%s", "Start Listen");
	return true;
}

inline bool myServer::initThreadpool() {
	m_pool = NULL;
	try {
		m_pool = new threadpool<http_conn>;
		LOG_INFO("%s", "Threadpool init.");
	}
	catch (std::exception()) {
		LOG_ERROR("%s", "failed to init threadpool.");
		//printf("failed to init threadpool.\n");
		return false;
	}
	return true;
}

inline bool myServer::initEpoll() {
	LOG_INFO("%s", "Epoll init.");
	m_events = new epoll_event[MAX_EVENT_NUMBER];
	m_epollfd = epoll_create(5);
	assert(m_epollfd != -1);
	addfd(m_epollfd, m_listenfd, false);
	http_conn::m_epollfd = m_epollfd;
	return true;
}

void timer_cb_func(util_timer* t) {
	//std::cout << "timer : " << t->user_data->sockfd << "  is triggered." << std::endl;
	int sockfd = t->user_data->sockfd;
	myServer::getInstance().closeConnection(sockfd);
	//myServer::users[t->user_data->sockfd].close_conn();
	delete t;
}

void myServer::addtimer(int connfd) {
	util_timer* t = new util_timer(TIMEOUT);
	client_data* cdata = new client_data;
	cdata->sockfd = connfd;
	if (t && cdata) {
		t->user_data = cdata;
		t->cb_func = timer_cb_func;
		timer->add_timer(t);
		//std::cout << "timer : " << t->user_data->sockfd << "  is set" << std::endl;
	}
	else {
		LOG_ERROR("%s", "timer add filed.");
		//std::cout << "timer add filed.\n";
	}
}


