#include "server/server.h"

#define NOT_USE_LOG 0
#define USE_LOG 1

int main() {
	myServer& server = myServer::getInstance();
	server.runServer(USE_LOG);

	return 0;
}