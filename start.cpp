#include "server/server.h"

#define DO_NOT_USE_LOG 0
#define USE_LOG 1

int main() {
	myServer& server = myServer::getInstance();
	server.runServer(DO_NOT_USE_LOG);

	return 0;
}