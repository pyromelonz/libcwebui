
#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS 1
#include <windows.h>

#endif

#undef _POSIX_SOURCE

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#if defined (  __linux__ ) || defined ( __APPLE__ ) || defined ( __FreeBSD__ )
	#include <unistd.h>
	#include <pthread.h>
#endif

#include "webserver_api_functions.h"
#include "websocket_api.h"


#ifdef WEBSERVER_USE_WEBSOCKETS


typedef void*(*thread_function)(void*);

#ifdef _MSC_VER

#define pthread_mutex_t HANDLE 



void run_thread(thread_function func , void* arg) {

	DWORD   dwThreadIdArray;

	CreateThread(
		NULL,                   // default security attributes
		0,                      // use default stack size  
		func,       // thread function name
		arg,          // argument to thread function 
		0,                      // use default creation flags 
		&dwThreadIdArray);   // returns the thread identifier 
}

int pthread_mutex_lock(pthread_mutex_t* mutex) {

	WaitForSingleObject( mutex, INFINITE);
	return 0;

}

void pthread_mutex_unlock(pthread_mutex_t* mutex) {

	ReleaseMutex(mutex);

}

void pthread_mutex_init(pthread_mutex_t* mutex, int init) {
	*mutex = CreateMutex(0, FALSE, 0);
}

#endif


#if defined (  __linux__ ) || defined ( __APPLE__ ) || defined ( __FreeBSD__ )

static void run_thread(thread_function func , void* arg) {

	pthread_t * handle = malloc( sizeof( pthread_t ) );
	pthread_create( handle, NULL, func, arg); 
}

#endif

#include <linked_list.h>

#define OUT_BUFFER_SIZE 10000

/*******************************************************************
 * 
 *            Helper Functions
 * 
 *******************************************************************/

static int getTime(char* buffer) {
	time_t rawtime;
	struct tm * timeinfo;
	time(&rawtime);
	timeinfo = localtime(&rawtime);
	return sprintf(buffer, "%02d:%02d:%02d", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
}

static void mysleep(unsigned long ms) {

#ifdef _MSC_VER
	Sleep(seconds);
#else
	struct timespec req;
	req.tv_sec = 0;
	req.tv_nsec = 100000000;
	nanosleep(&req, 0);
#endif

}

/*******************************************************************
 * 
 *            Global Variables
 * 
 *******************************************************************/

list_t clock_clients;


pthread_mutex_t clock_mutex;


static void uhr_loop( void ) {
	char buffer[1000];

	char buffer2[2000];
	int ret;
	char* guid;
	char* remove;


	while (1) {
		getTime(buffer);
		
		if ( 0 == lockGlobalVars() ){
			setGlobalVar("time", buffer);
			if ( 0 != unlockGlobalVars() ){
				printf("error unlocking global var lock\n");
				exit(1);
			}
		}


		pthread_mutex_lock(&clock_mutex);
		
		sprintf(buffer2, "time: %s", buffer);

		guid = 0;
		remove = 0;
		
		// iterate connected clients
		ws_list_iterator_start(&clock_clients);
		while ((guid = (char*) ws_list_iterator_next(&clock_clients))) {
			
			// send text frame to client
			ret = WebsocketSendTextFrame(guid, buffer2, strlen(buffer2));
			//printf("send : %s %d\n",guid,ret);
			
			// mark client for removal if send fails
			if (ret == -1) {
				remove = guid;
				printf("remove\n");
			}
		}		
		ws_list_iterator_stop(&clock_clients);
		
		if (remove != 0) {
			ws_list_delete(&clock_clients, guid);
			WebserverFree(guid);
		}

		pthread_mutex_unlock(&clock_mutex);

		mysleep(1000);
	}
}



static void handleCommandSocket(char *guid, const char *msg) {
	char* tmp;
	if (0 == strcmp(msg, "connect_clock")) {
		int ret = 0;
		ret = pthread_mutex_lock(&clock_mutex);
		if (ret == 0) {
			
			tmp = WebserverMalloc(strlen(guid)+1);
			strcpy(tmp, guid);
			ws_list_append(&clock_clients, tmp, 0);
			pthread_mutex_unlock(&clock_mutex);
		}
	}
	if (0 == strncmp(msg, "echo:", 5)) {
		
		if ( 0 == strcmp( msg, "echo:error" ) ){
			char *msg2 = (char*)msg;
			msg2[3] = 0x88;
		}
		
		WebsocketSendTextFrame(guid, msg, strlen(msg));
	}
	
	if (0 == strcmp(msg, "ping")) {
		WebsocketSendTextFrame(guid, "pong", strlen("pong"));
	}
}

static void deleteClient(char* guid) {
	char* tmp_guid;
	int ret = 0;
	ret = pthread_mutex_lock(&clock_mutex);
	if (ret == 0) {
		ws_list_iterator_start(&clock_clients);
		
		while ((tmp_guid = (char*) ws_list_iterator_next(&clock_clients))) {
			if (0 == strcmp(tmp_guid, guid)) {
				//printf("Client entfern           : %s CommandSocket\n", guid);
				break;
			}
		}
		ws_list_iterator_stop(&clock_clients);
		
		if (tmp_guid != 0){ 
			ws_list_delete(&clock_clients, tmp_guid);
			WebserverFree(tmp_guid);
		}
		
		pthread_mutex_unlock(&clock_mutex);
	} else {
		printf("deleteClient Commandsocket Mutex Timeout\n");
	}
}


static void simpleThread(void* p) {
	char buffer[1000];
	int i, num = 1, len;
	int ret = 0;
	char *guid = (char*) p;

	for (i = 1; i < 61; i++) {
		len = snprintf(buffer, 1000, "Test%d", num++);
		ret = WebsocketSendTextFrame(guid, buffer, len);
		if (ret == -1) {
			printf("Test Thread send error   : %s\n", guid);
			WebsocketSendCloseFrame(guid);
			WebserverFree(guid);
			return;
		}
		mysleep(100);
	}
	

	printf("Websocket API Close      : %s simple\n", guid);
	WebsocketSendCloseFrame(guid);
	WebserverFree(guid);
	return;
}

static void *simple_function(void *ptr) {
	simpleThread(ptr);
	return 0;
}

DEFINE_WEBSOCKET_HANDLER( "simple" , simple_handler ) {
	unsigned long l;
	char *tmp;
	switch (signal) {
	case WEBSOCKET_CONNECT:
		printf("Websocket API Connect    : %s simple\n", guid);
		l = strlen(guid) + 1;
		tmp = WebserverMalloc(l);
		strcpy(tmp, guid);
		run_thread( simple_function, tmp );
		break;
	}
}

DEFINE_WEBSOCKET_HANDLER( "CommandSocket" , CommandSocket_handler) {
	
	unsigned long l = strlen(guid) + 1;
	char *tmp  = WebserverMalloc( l );
	strcpy(tmp, guid);
	
	//printf("command socket : %d\n",signal);
	switch (signal) {
		
		case WEBSOCKET_MSG:
			//printf("CommandSocket Handler 1 %s\n", msg);
			handleCommandSocket(tmp, msg);
		break;
		
		case WEBSOCKET_DISCONNECT:
			deleteClient(tmp);
		break;
	}
	
	WebserverFree(tmp);
}



static void *clock_start_helper(void *ptr) {
	uhr_loop();
	return 0;
}

void startApiThreads( void ) {

	ws_list_init(&clock_clients);
	
	pthread_mutex_init(&clock_mutex, 0);
	run_thread( clock_start_helper, 0 );

}


#endif
