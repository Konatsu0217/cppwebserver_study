#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <unistd.h>
#include <errno.h>
#define BUFFER_SIZE 4096

enum MAIN_CHECK_STATE
{
	CHECK_STATE_REQUESTLINE = 0, CHECK_STATE_HEADER, CHECK_STATE_CONTENT
};
enum SUB_LINE_STATES
{
	// 本行解析完毕、http有误、等待更多信息
	LINE_OK = 0, LINE_BAD, LINE_OPEN
};
// http解析过程中的结果：
// 不完整、完整请求解析完毕、解析内容有错误、客户端无法访问资源、内部错误、连接已关闭
enum HTTP_CODE
{
	NO_REQUEST, GET_REQUEST, BAD_REQUEST, FORBIDDEN_REQUEST, INTERNAL_ERROR, CLOSED_CONNECTION
};

// 从状态机：解析**每一行**的内容
// GET / index.html HTTP / 1.1 </r></n>
// Host: www.example.com </n>
// User - Agent : Mozilla / 5.0 </n>
// Content - Type : text / html</r></n>
// </r></n>
// This is the message body.
// 通过 /r /n 的位置确定是否解析完成每一行

SUB_LINE_STATES parse_line(char* buffer, int& checked_idx, int& read_idx) {
	char temp;
	for (; checked_idx < read_idx; ++checked_idx) {
		temp = buffer[checked_idx];
		// check_idx = '/r' --> 检查后面还有没有东西 + 检查后面一个是不是 '\n'
		if (temp == '\r') {
			if ((checked_idx + 1) == read_idx) {
				return LINE_OPEN;
			}
			else if (buffer[checked_idx + 1] == '\n') {
				// 连续出现 /r /n，这行解析完了
				// '\0' C风格字符串的结束标记
				buffer[checked_idx++] = '\0';
				buffer[checked_idx++] = '\0';
				return LINE_OK;
			}
		}
		// check_idx = '/n' --> 检查前一个是不是'\r'，这个不用考虑后面还有没有了，只要前面是\r就肯定是一行的结束
		else if (temp == '\n') {
			if ((checked_idx > 1) && buffer[checked_idx - 1] == '\r') {
				buffer[checked_idx - 1] = '\0';
				buffer[checked_idx++] = '\0';
				return LINE_OK;
			}
			return LINE_BAD;
		}
	}
	return LINE_OPEN;
}

// 解析请求行： GET /index.html HTTP / 1.1 </r></n>
// char * temp = "GET /index.html HTTP / 1.1 </r></n>"
HTTP_CODE parse_requestline(char* temp, MAIN_CHECK_STATE& checkstates) {
	char* requestMethod;
	char* requestUrl;
	char* httpVersion;

	// char *strpbrk(char* str1, char* str2)
	// 从str1中查找str2首次出现的位置，返回指针；找不到返回 Null
	char* url = strpbrk(temp, " \t");
	if (!url) {
		return BAD_REQUEST;
	}
	// 取第一段   GET(空格)，并且将url*后移一位，字符串的开头指向 /index....
	*url++ = '\0';
	char* method = temp;
	requestMethod = method;
	// 书上的实现只做了 GET 的支持
	if (strcasecmp(method, "GET") == 0) {
		printf("Request method GET.\n");
	}
	else if (strcasecmp(method, "POST") == 0) {
		printf("Request method POST.\n");
	}
	else return BAD_REQUEST;

	url += strspn(url, " \t");
	char* version = strpbrk(url, " \t");
	if (!version) {
		return BAD_REQUEST;
	}
	*version++ = '\0';
	version += strspn(version, " \t");
	// 支持 http1.1
	if (strcasecmp(version, "HTTP/1.1") != 0) {
		return BAD_REQUEST;
	}
	httpVersion = version;
	// 比较前 7 个是不是 http://
	if (strncasecmp(url, "http://", 7) == 0) {
		url += 7;
		url = strchr(url, '/');
	}

	if (!url || url[0] != '/') {
		return BAD_REQUEST;
	}
	requestUrl = url;
	printf("The request URL is: %s\n", url);
	checkstates = CHECK_STATE_HEADER;
	return NO_REQUEST;
}
// Host: www.example.com </n>
// User - Agent : Mozilla / 5.0 </n>
// Content - Type : text / html</r></n>
HTTP_CODE parse_headers(char* text) {
	char* host;
	bool connectionAlive;
	long contentLength;

	if (text[0] == '\0') {
		return GET_REQUEST;
	}
	else if (strncasecmp(text, "Host:", 5) == 0) {
		text += 5;
		text += strspn(text, " \t");
		host = text;
		printf("Host is:%s\n", host);
	}
	else if (strncasecmp(text, "Connection:", 11) == 0)
	{
		text += 11;
		text += strspn(text, " \t");
		if (strcasecmp(text, "keep-alive") == 0)
		{
			connectionAlive = true;
		}
	}
	else if (strncasecmp(text, "Content-length:", 15) == 0)
	{
		text += 15;
		text += strspn(text, " \t");
		contentLength = atol(text);
	}
	else {}
	return NO_REQUEST;
}
// 主状态机
HTTP_CODE parse_content(char* buffer, int& checked_idx, int& read_idx,
	MAIN_CHECK_STATE& check_state, int& start_line) {

	SUB_LINE_STATES linestates = LINE_OK;
	HTTP_CODE retcode = NO_REQUEST;
	while ((linestates = parse_line(buffer, checked_idx, read_idx)) == LINE_OK) {
		char* temp = buffer + start_line;
		start_line = checked_idx;
		switch (check_state)
		{
		case CHECK_STATE_REQUESTLINE: {
			retcode = parse_requestline(temp, check_state);
			if (retcode == BAD_REQUEST) {
				return BAD_REQUEST;
			}
			break;
		}
		case CHECK_STATE_HEADER: {
			retcode = parse_headers(temp);
			if (retcode == BAD_REQUEST) {
				return BAD_REQUEST;
			}
			else if (retcode == GET_REQUEST) {
				return GET_REQUEST;
			}
			break;
		}
		case CHECK_STATE_CONTENT: {
			// todo...
			break;
		}
		default:
		{
			return INTERNAL_ERROR;
		}
		}
	}
	if (linestates == LINE_OPEN) {
		// 当前的read_idx之前的内容还不是一个完整的行，需要继续补充
		return NO_REQUEST;
	}
	else return BAD_REQUEST;
}

