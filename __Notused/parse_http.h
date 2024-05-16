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
	// ���н�����ϡ�http���󡢵ȴ�������Ϣ
	LINE_OK = 0, LINE_BAD, LINE_OPEN
};
// http���������еĽ����
// ���������������������ϡ����������д��󡢿ͻ����޷�������Դ���ڲ����������ѹر�
enum HTTP_CODE
{
	NO_REQUEST, GET_REQUEST, BAD_REQUEST, FORBIDDEN_REQUEST, INTERNAL_ERROR, CLOSED_CONNECTION
};

// ��״̬��������**ÿһ��**������
// GET / index.html HTTP / 1.1 </r></n>
// Host: www.example.com </n>
// User - Agent : Mozilla / 5.0 </n>
// Content - Type : text / html</r></n>
// </r></n>
// This is the message body.
// ͨ�� /r /n ��λ��ȷ���Ƿ�������ÿһ��

SUB_LINE_STATES parse_line(char* buffer, int& checked_idx, int& read_idx) {
	char temp;
	for (; checked_idx < read_idx; ++checked_idx) {
		temp = buffer[checked_idx];
		// check_idx = '/r' --> �����滹��û�ж��� + ������һ���ǲ��� '\n'
		if (temp == '\r') {
			if ((checked_idx + 1) == read_idx) {
				return LINE_OPEN;
			}
			else if (buffer[checked_idx + 1] == '\n') {
				// �������� /r /n�����н�������
				// '\0' C����ַ����Ľ������
				buffer[checked_idx++] = '\0';
				buffer[checked_idx++] = '\0';
				return LINE_OK;
			}
		}
		// check_idx = '/n' --> ���ǰһ���ǲ���'\r'��������ÿ��Ǻ��滹��û���ˣ�ֻҪǰ����\r�Ϳ϶���һ�еĽ���
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

// ���������У� GET /index.html HTTP / 1.1 </r></n>
// char * temp = "GET /index.html HTTP / 1.1 </r></n>"
HTTP_CODE parse_requestline(char* temp, MAIN_CHECK_STATE& checkstates) {
	char* requestMethod;
	char* requestUrl;
	char* httpVersion;

	// char *strpbrk(char* str1, char* str2)
	// ��str1�в���str2�״γ��ֵ�λ�ã�����ָ�룻�Ҳ������� Null
	char* url = strpbrk(temp, " \t");
	if (!url) {
		return BAD_REQUEST;
	}
	// ȡ��һ��   GET(�ո�)�����ҽ�url*����һλ���ַ����Ŀ�ͷָ�� /index....
	*url++ = '\0';
	char* method = temp;
	requestMethod = method;
	// ���ϵ�ʵ��ֻ���� GET ��֧��
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
	// ֧�� http1.1
	if (strcasecmp(version, "HTTP/1.1") != 0) {
		return BAD_REQUEST;
	}
	httpVersion = version;
	// �Ƚ�ǰ 7 ���ǲ��� http://
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
// ��״̬��
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
		// ��ǰ��read_idx֮ǰ�����ݻ�����һ���������У���Ҫ��������
		return NO_REQUEST;
	}
	else return BAD_REQUEST;
}

