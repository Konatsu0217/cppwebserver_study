#include "http_conn.h"

const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file form this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the request file.\n";

// 网页根目录
const char* doc_root = "/var/www";

int setnonblocking(int fd) {
	int old_op = fcntl(fd, F_GETFL);
	int new_op = old_op | O_NONBLOCK;
	fcntl(fd, F_SETFL, new_op);
	return old_op;
}

void addfd(int epollfd, int fd, bool one_shot) {
	epoll_event event;
	event.data.fd = fd;
	event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
	if (one_shot) {
		event.events |= EPOLLONESHOT;
	}
	epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
	setnonblocking(fd);
}

void removefd(int epollfd, int fd) {
	epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
	close(fd);
}

void modfd(int epollfd, int fd, int ev) {
	epoll_event event;
	event.data.fd = fd;
	event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
	epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

int http_conn::m_user_count = 0;
int http_conn::m_epollfd = -1;

void http_conn::close_conn(bool real_close) {
	if (real_close && (m_sockfd != -1)) {
		removefd(m_epollfd, m_sockfd);
		//printf("a clinet closed his connection\n");
		m_sockfd = -1;
		m_user_count--;
	}
}

void http_conn::init(int sockfd, const sockaddr_in & addr)
{
	m_sockfd = sockfd;
	m_address = addr;

	int reuse = 1;
	setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
	addfd(m_epollfd, sockfd, true);
	m_user_count++;

	init();
}

void http_conn::init() {
	//init_timer();


	m_check_state = CHECK_STATE_REQUESTLINE;
	m_linger = false;

	m_method = GET;
	m_url = 0;
	m_version = 0;
	m_content_length = 0;
	m_host = 0;
	m_start_line = 0;
	m_check_idx = 0;
	m_read_idx = 0;
	m_write_idx = 0;
	memset(m_read_buf, '\0', READ_BUFFER_SIZE);
	memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
	memset(m_real_fpath, '\0', FILENAME_LEN);
}

http_conn::HTTP_CODE http_conn::process_read()
{
	LINE_STATUS linestates = LINE_OK;
	HTTP_CODE retcode = NO_REQUEST;
	char* text = 0;
	bool msg_get = true;

	while (((linestates = parse_line()) == LINE_OK) || ((m_check_state == CHECK_STATE_CONTENT) && (linestates == LINE_OK)))
	{
		text = get_line();
		m_start_line = m_check_idx;
		//if (msg_get) {
		//	printf("recived a mssage:\n%s\n", text);
		//	msg_get = false;
		//}
		//else std::cout << text << std::endl;

		switch (m_check_state)
		{
		case http_conn::CHECK_STATE_REQUESTLINE: {
			auto ret = parse_request_line(text);
			if (ret == BAD_REQUEST) {
				return BAD_REQUEST;
			}
			break;
		}
		case http_conn::CHECK_STATE_HEADER: {
			auto ret = parse_headers(text);
			if (ret == BAD_REQUEST) {
				return BAD_REQUEST;
			}
			else if (ret == GET_REQUEST) {
				return do_request();
			}
			break;
		}
		case http_conn::CHECK_STATE_CONTENT: {
			auto ret = parse_content(text);
			if (ret == GET_REQUEST) {
				return do_request();
			}
			linestates = LINE_OPEN;
			break;
		}
		default:
			return INTERNAL_ERROR;
		}
	}
	return NO_REQUEST;
}

bool http_conn::process_write(HTTP_CODE ret)
{
	switch (ret)
	{
	case http_conn::BAD_REQUEST: {
		add_status_line(400, error_400_title);
		add_headers(strlen(error_400_form));
		if (!add_content(error_400_form)) {
			return false;
		}
		break;
	}
	case http_conn::NO_RESOURCE:
	{
		add_status_line(404, error_404_title);
		add_headers(strlen(error_404_form));
		if (!add_content(error_404_form))
			return false;
		break;
	}
	case http_conn::FORBIDDEN_REQUEST:
	{
		add_status_line(403, error_403_title);
		add_headers(strlen(error_403_form));
		if (!add_content(error_403_form))
			return false;
		break;
	}
	case http_conn::FILE_REQUEST:
	{
		add_status_line(200, ok_200_title);
		if (m_file_stat.st_size != 0)
		{
			add_headers(m_file_stat.st_size);
			m_iv[0].iov_base = m_write_buf;
			m_iv[0].iov_len = m_write_idx;
			m_iv[1].iov_base = m_file_address;
			m_iv[1].iov_len = m_file_stat.st_size;
			m_iv_count = 2;
			return true;
		}
		else
		{
			const char *ok_string = "<html><body></body></html>";
			add_headers(strlen(ok_string));
			if (!add_content(ok_string))
				return false;
		}
	}
	case http_conn::INTERNAL_ERROR: {
		add_status_line(500, error_500_title);
		add_headers(strlen(error_500_form));
		if (!add_content(error_500_form))
			return false;
		break;
	}
	default:
		return false;
	}
	m_iv[0].iov_base = m_write_buf;
	m_iv[0].iov_len = m_write_idx;
	m_iv_count = 1;
	return true;
}

http_conn::HTTP_CODE http_conn::parse_request_line(char * read_buffer)
{
	m_url = strpbrk(read_buffer, " \t");
	if (!m_url)
	{
		return BAD_REQUEST;
	}
	*m_url++ = '\0';
	char *method = read_buffer;
	if (strcasecmp(method, "GET") == 0)
		m_method = GET;
	else if (strcasecmp(method, "POST") == 0)
	{
		m_method = POST;
		//cgi = 1;
	}
	else
		return BAD_REQUEST;
	m_url += strspn(m_url, " \t");
	m_version = strpbrk(m_url, " \t");
	if (!m_version)
		return BAD_REQUEST;
	*m_version++ = '\0';
	m_version += strspn(m_version, " \t");
	if (strcasecmp(m_version, "HTTP/1.1") != 0)
		return BAD_REQUEST;
	if (strncasecmp(m_url, "http://", 7) == 0)
	{
		m_url += 7;
		m_url = strchr(m_url, '/');
	}
	if (!m_url || m_url[0] != '/')
		return BAD_REQUEST;
	m_check_state = CHECK_STATE_HEADER;
	return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::parse_headers(char * read_buffer)
{
	if (read_buffer[0] == '\0') {
		if (m_content_length != 0) {
			m_check_state = CHECK_STATE_CONTENT;
			return NO_REQUEST;
		}
		return GET_REQUEST;
	}
	else if (strncasecmp(read_buffer, "Connection:", 11) == 0)
	{
		read_buffer += 11;
		read_buffer += strspn(read_buffer, " \t");
		if (strcasecmp(read_buffer, "Keep-Alive") == 0)
		{
			m_linger = true;
		}
	}
	else if (strncasecmp(read_buffer, "Content-length:", 15) == 0)
	{
		read_buffer += 15;
		read_buffer += strspn(read_buffer, " \t");
		m_content_length = atol(read_buffer);
	}
	else if (strncasecmp(read_buffer, "Host:", 5) == 0)
	{
		read_buffer += 5;
		read_buffer += strspn(read_buffer, " \t");
		m_host = read_buffer;
	}
	else
	{
		//LOG_INFO("oop!unknow header: %s", read_buffer);
		/*printf("oop!unknow header: %s", read_buffer);*/
	}
	return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::parse_content(char * text)
{
	if (m_read_idx >= (m_content_length + m_check_idx))
	{
		text[m_content_length] = '\0';
		return GET_REQUEST;
	}
	return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::do_request()
{
	strcpy(m_real_fpath, doc_root);
	int len = strlen(doc_root);
	strncpy(m_real_fpath + len, m_url, FILENAME_LEN - len - 1);
	if (stat(m_real_fpath, &m_file_stat) < 0) {
		return NO_RESOURCE;
	}
	if (!(m_file_stat.st_mode & S_IROTH)) {
		return FORBIDDEN_REQUEST;
	}
	if (S_ISDIR(m_file_stat.st_mode)) {
		return BAD_REQUEST;
	}

	int fd = open(m_real_fpath, O_RDONLY);
	m_file_address = (char*)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	close(fd);
	return FILE_REQUEST;
}

http_conn::LINE_STATUS http_conn::parse_line() {
	char temp;
	for (; m_check_idx < m_read_idx; ++m_check_idx) {
		temp = m_read_buf[m_check_idx];
		if (temp == '\r') {
			if ((m_check_idx + 1) == m_read_idx) {
				return LINE_OPEN;
			}
			else if (m_read_buf[m_check_idx + 1] == '\n') {
				m_read_buf[m_check_idx++] = '\0';
				m_read_buf[m_check_idx++] = '\0';
				return LINE_OK;
			}
			return LINE_BAD;
		}
		else if (temp == '\n') {
			if ((m_check_idx > 1) && m_read_buf[m_check_idx - 1] == '\r') {
				m_read_buf[m_check_idx - 1] = '\0';
				m_read_buf[m_check_idx++] = '\0';
				return LINE_OK;
			}
			return LINE_BAD;
		}
	}
	return LINE_OPEN;
}

void http_conn::unmap()
{
	if (m_file_address) {
		munmap(m_file_address, m_file_stat.st_size);
		m_file_address = 0;
	}
}

bool http_conn::add_response(const char * format, ...)
{
	if (m_write_idx >= WRITE_BUFFER_SIZE) {
		return false;
	}
	va_list arg_list;
	va_start(arg_list, format);
	int len = vsnprintf(m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - 1 - m_write_idx, format, arg_list);
	if (len >= (WRITE_BUFFER_SIZE - 1 - m_write_idx)) {
		return false;
	}
	m_write_idx += len;
	va_end(arg_list);
	return true;
}

bool http_conn::add_content(const char * content)
{
	return add_response("%s", content);
}

bool http_conn::add_status_line(int status, const char * title)
{
	return add_response("%s %d %s \r\n", "HTTP/1.1", status, title);
}

bool http_conn::add_headers(int content_len)
{
	return add_content_length(content_len) && add_linger() &&
		add_blank_line();
}

bool http_conn::add_content_length(int content_len)
{
	return add_response("Content-Length:%d\r\n", content_len);
}

bool http_conn::add_linger()
{
	return add_response("Connection:%s\r\n", (m_linger == true) ? "Keep-Alive" : "close");
}

bool http_conn::add_blank_line()
{
	return add_response("%s", "\r\n");
}


void http_conn::process()
{
	HTTP_CODE read_ret = process_read();
	if (read_ret == NO_REQUEST) {
		// 读完了，对应端口变成EPOLLIN状态
		modfd(m_epollfd, m_sockfd, EPOLLIN);
		return;
	}
	bool write_ret = process_write(read_ret);
	if (!write_ret) {
		close_conn();
	}
	modfd(m_epollfd, m_sockfd, EPOLLOUT);
}

bool http_conn::read()
{
	if (m_read_idx >= READ_BUFFER_SIZE)
		return false;
	int bytes_read = 0;
	while (1) {
		bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - 1, 0);
		if (bytes_read == -1) {
			if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
				break;
			}
			return false;
		}
		else if (bytes_read == 0) {
			return false;
		}
		m_read_idx += bytes_read;
	}

	//m_timer.reset(m_sockfd, TIMEOUT);
	return true;
}

bool http_conn::write()
{
	int temp = 0;
	int bytes_have_send = 0;
	int bytes2send = m_write_idx;
	if (bytes2send == 0) {
		modfd(m_epollfd, m_sockfd, EPOLLIN);
		init();
		return true;
	}

	while (1)
	{
		temp = writev(m_sockfd, m_iv, m_iv_count);
		if (temp <= -1) {
			if (errno = EAGAIN)// 输出缓冲区满
			{
				modfd(m_epollfd, m_sockfd, EPOLLOUT);
				return true;
			}
			unmap();
			return false;
		}
		bytes2send -= temp;
		bytes_have_send += temp;
		if (bytes2send <= bytes_have_send) {
			unmap();
			if (m_linger) {
				init();
				modfd(m_epollfd, m_sockfd, EPOLLIN);
				return true;
			}
			else {
				modfd(m_epollfd, m_sockfd, EPOLLIN);
				return false;
			}
		}
	}
}