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


#include "webserver_api_functions.h"

static void termination_handler(int signum) {
	WebserverShutdownHandler();
}

static void sig_pipe_hanler(int signum) {
	//printf("Sig Pipe\n");
}



int main(int argc, char **argv) {


	if (signal(SIGINT, termination_handler) == SIG_IGN )  signal(SIGINT, SIG_IGN);
	if (signal(SIGHUP, termination_handler) == SIG_IGN )  signal(SIGHUP, SIG_IGN);
	if (signal(SIGTERM, termination_handler) == SIG_IGN ) signal(SIGTERM, SIG_IGN);
	if (signal(SIGPIPE, sig_pipe_hanler) == SIG_IGN	)     signal(SIGPIPE, SIG_IGN);

	openlog("webserver", LOG_CONS, LOG_LOCAL0);

	if (0 == WebserverInit()) {

		
		
		WebserverConfigSetInt( "use_csp",0);

		WebserverConfigSetInt("port",8080);
		
		WebserverConfigSetInt("ssl_port",4443);
		WebserverConfigSetText("ssl_file_path", "./");
		WebserverConfigSetText("ssl_key_file", "server.pem");
		WebserverConfigSetText("ssl_key_file_password", "password");
		WebserverConfigSetText("ssl_dh_file", "dh1024.pem");
		WebserverConfigSetText("ssl_ca_list_file", "root.pem");
		

		WebserverStart();
	}

	WebserverShutdown();

	return 0;
}



