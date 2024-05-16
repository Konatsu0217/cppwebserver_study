#include "parse_http.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <unistd.h>
#include <errno.h>


void parsehttp(int connfd) {
	while (1) {
		if (connfd < 0) {
			printf("errno is : %d\n", errno);
			sleep(300);
		}
		else {
			char buffer[BUFFER_SIZE];
			memset(buffer, '\0', sizeof buffer);
			int data_read = 0;
			int read_idx = 0;
			int checked_idx = 0;
			int startline = 0;
			MAIN_CHECK_STATE checkstate = CHECK_STATE_REQUESTLINE;
			while (1) {
				data_read = recv(connfd, buffer + read_idx, BUFFER_SIZE - read_idx, 0);
				if (data_read == -1) {
					printf("reading failed.\n");
					break;
				}
				else if (data_read == 0) {
					printf("remote client has closed ths connection.\n");
					break;
				}
				// 读取了有效的http报文，开始解析
				read_idx += data_read;
				HTTP_CODE result = parse_content(buffer, checked_idx, read_idx, checkstate, startline);

				if (result == NO_REQUEST) {
					continue;
				}
				else if (result == GET_REQUEST) {
					printf("we get a valid request.\n");
					continue;
				}
				else {
					printf("something gose wrong.\n");
					break;
				}
			}
			close(connfd);
			sleep(10);
		}
	}
}