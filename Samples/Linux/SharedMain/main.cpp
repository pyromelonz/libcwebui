#include <iostream>	
#include <ifaddrs.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
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

void sig_pipe_hanler(int signum) {
	//printf("Sig Pipe\n");
}

int my_cors_handler( cors_infos* infos){
	//printf("CORS Handler: %s\n",ws_get_cors_type_name( infos->type ));
	switch( infos->type){
		case CORS_ALLOW_ORIGIN:
			printf("  Origin: %s\n",infos->origin);
			if ( 0 == strcmp( infos->origin, "www.google.de" ) ){
				return COND_TRUE;
			}
			
			break;
		case CORS_ALLOW_METHODS:
			printf("  Method: %s\n",infos->method);
			if ( 0 == strcmp( infos->method, "GET" ) ){
				return COND_TRUE;
			}
			if ( 0 == strcmp( infos->method, "POST" ) ){
				return COND_TRUE;
			}
			if ( 0 == strcmp( infos->method, "OPTIONS" ) ){
				return COND_TRUE;
			}
			break;
		
		case CORS_ALLOW_CREDENTIALS:
			//return COND_FALSE;
			//printf("  Allow Credential\n");
			return COND_TRUE;
		
		case CORS_ALLOW_HEADERS:
			printf("  Allow Headers\n");
			return COND_TRUE;
	}
	
	return COND_FALSE;
}


extern unsigned char data__[];
extern unsigned long allocated;

int main(int argc, char **argv) {


	if (signal(SIGINT, termination_handler) == SIG_IGN )  signal(SIGINT, SIG_IGN);
	if (signal(SIGHUP, termination_handler) == SIG_IGN )  signal(SIGHUP, SIG_IGN);
	if (signal(SIGTERM, termination_handler) == SIG_IGN ) signal(SIGTERM, SIG_IGN);
	if (signal(SIGPIPE, sig_pipe_hanler) == SIG_IGN	)     signal(SIGPIPE, SIG_IGN);

	openlog("webserver", LOG_CONS, LOG_LOCAL0);

	if (0 == WebserverInit()) {

		//WebserverAddBinaryData( data__ );

		ws_set_cors_handler( my_cors_handler );

		#ifdef WEBSERVER_USE_PYTHON

		WebserverInitPython();
		WebserverLoadPyPlugin( "../../testSite/test.py" );
		WebserverLoadPyPlugin( "../../testSite/test2.py" );

		WebserverConfigSetInt( "reload_py_modules",1);

		#endif

		WebserverConfigSetInt( "use_csp",0);

		WebserverAddFileDir("", "../../testSite/www");
		WebserverAddFileDir("img", "../../testSite/img");
		WebserverAddFileDir("css", "../../testSite/css");


		WebserverLoadPlugin("TestPlugin", "../../testSite/src/test_plugin.so");

		WebserverConfigSetInt("port",8080);
		WebserverConfigSetInt("ssl_port",4443);
		WebserverConfigSetText("ssl_file_path", "./");
		WebserverConfigSetText("ssl_key_file", "server.pem");
		WebserverConfigSetText("ssl_key_file_password", "password");
		WebserverConfigSetText("ssl_dh_file", "dh2048.pem");
		WebserverConfigSetText("ssl_ca_list_file", "root.pem");


		WebserverStart();
	}

	WebserverShutdown();
	
	printf("allocated : %ld\n",allocated);

	return 0;
}



