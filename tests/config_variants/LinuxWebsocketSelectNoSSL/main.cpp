#include <iostream>
#include <ifaddrs.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <syslog.h>
#include <dlfcn.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>

#include "webserver_api_functions.h"

static void termination_handler(int signum) {
	WebserverShutdownHandler();
}

static void sig_pipe_hanler(int signum) {
	//printf("Sig Pipe\n");
}


DEFINE_WEBSOCKET_HANDLER( "TestSocket" , TestSocket ) {
	
	switch (signal) {
		case WEBSOCKET_CONNECT:
			printf("Websocket API  Connect TestSocket    : %s \n", guid);
			break;
		case WEBSOCKET_DISCONNECT:
			printf("Websocket API  Disconnect TestSocket : %s \n", guid);
			break;
		
		case WEBSOCKET_MSG:
			printf("Websocket API  TestSocket Msg 1 %s\n", msg);
			WebsocketSendTextFrame(guid, msg, strlen(msg));
			break;
	}
}

int main(int argc, char **argv) {


	if (signal(SIGINT, termination_handler) == SIG_IGN )  signal(SIGINT, SIG_IGN);
	if (signal(SIGHUP, termination_handler) == SIG_IGN )  signal(SIGHUP, SIG_IGN);
	if (signal(SIGTERM, termination_handler) == SIG_IGN ) signal(SIGTERM, SIG_IGN);
	if (signal(SIGPIPE, sig_pipe_hanler) == SIG_IGN	)     signal(SIGPIPE, SIG_IGN);

	
	if (0 == WebserverInit()) {
		
		WebserverAddFileDir("", "www");
		
		WebserverConfigSetInt("port",8080);
		
		REGISTER_WEBSOCKET_HANDLER ( TestSocket );

		WebserverStart();
	}

	WebserverShutdown();

	return 0;
}



