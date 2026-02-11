/*

SPDX-License-Identifier: MPL-2.0

 libCWebUI
 Copyright (C) 2007 - 2019  Ramin Seyed-Moussavi

 Projekt URL : https://github.com/lordrasmus/libcwebui

 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at https://mozilla.org/MPL/2.0/. 

*/


#include "platform_includes.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#include <stdio.h>
#include <errno.h>

#include <linux/unistd.h>       /* for _syscallX macros/related stuff */
#ifndef __MUSL__
	#include <linux/kernel.h>       /* for struct sysinfo */
#endif
#include <linux/types.h>

#include <sys/sysinfo.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>


#ifdef WEBSERVER_USE_SSL
#include <openssl/rand.h>
#endif

#include "webserver.h"


#ifdef DMALLOC
#include <dmalloc/dmalloc.h>
#endif

/* http://gcc.gnu.org/onlinedocs/cpp/Common-Predefined-Macros.html
 http://gcc.gnu.org/onlinedocs/cpp/Standard-Predefined-Macros.html
*/


/**************************************************************
*                                                             *
*                   Speicher Verwaltung                       *
*                                                             *
**************************************************************/
/*#define MALLOC_DEBUG*/
#ifdef MALLOC_DEBUG
static int mallocs;
#endif
void*	PlatformMalloc ( SIZE_TYPE size ) {
#ifdef MALLOC_DEBUG
    unsigned char *p;
	/*printf ( "Malloc 1 %X (%d) %d\r\n",0,size,mallocs );
	malloc_stats(NULL);*/
    p = malloc ( size+2 );
    p[0]=size;
    p[1]=size>>8;
    printf ( "Malloc  %X (%d) %d\r\n",p+2,size,++mallocs );
    return p+2;
#else
    return malloc ( size );
#endif
}

void	PlatformFree ( void *mem ) {
#ifdef MALLOC_DEBUG
    unsigned char *p=mem-2;
	//malloc_stats(NULL);
    int l = p[0] + ( p[1]<<8 );
    printf ( "Free %X (%d) %d\r\n",mem,l,mallocs-- );
    free ( p );
#else
    free ( mem );
#endif
}

// void	WebserverPrintMemInfo ( void ) {}			/* gibt auf dem DSTni verfuegbaren Arbeitspeicher aus */



/*********************************************************************

					System informationen

*********************************************************************/



TIME_TYPE PlatformGetTick ( void ) {
	struct sysinfo s_info;
	int error;

	error = sysinfo(&s_info);
	if(error != 0){
		printf("code error = %d\n", error);
	}
	return s_info.uptime;
}

unsigned long PlatformGetTicksPerSeconde ( void ) {
    return (unsigned long)1; /* Konstante HZ benutzen ?? */
}


void 	PlatformGetGUID ( char* buf,SIZE_TYPE length ) {

	int i,off=0;
	unsigned char uuid[16];

	memset( buf, 0 , length);

	int fd = open("/dev/urandom",O_RDONLY);
	if ( fd < 0 ){
		printf("\nERROR: cant open /dev/urandom\n\n");
		exit(1);
	}
	int ret = 0;
	while( ret < 16 ){
		ssize_t n = read( fd, &uuid[ret], 16 - ret );
		if ( n < 0 ) {
			if (errno == EINTR) continue;  /* Signal interrupt, retry */
			printf("\nERROR: read /dev/urandom failed: %s\n\n", strerror(errno));
			close(fd);
			exit(1);
		}
		if ( n == 0 ) {
			printf("\nERROR: /dev/urandom unexpected EOF\n\n");
			close(fd);
			exit(1);
		}
		ret += n;
	}
	close( fd );

	// UUID Version 4
	uuid[6] = (uuid[6] & 0x0F) | 0x40;

	// UUID Variant DCE
	uuid[8] = (uuid[8] & 0x3F) | 0x80;
	
	for( i = 0 ; i < 4; i++ ){ off += sprintf( &buf[off],"%02X", uuid[i]); }
	off += sprintf( &buf[off],"-");

	for( ; i < 6; i++ ){ off += sprintf( &buf[off],"%02X", uuid[i]); }
	off += sprintf( &buf[off],"-");

	for( ; i < 8; i++ ){ off += sprintf( &buf[off],"%02X", uuid[i]); }
	off += sprintf( &buf[off],"-");

	for( ; i < 10; i++ ){ off += sprintf( &buf[off],"%02X", uuid[i]); }
	off += sprintf( &buf[off],"-");

	for( ; i < 16; i++ ){ off += sprintf( &buf[off],"%02X", uuid[i]); }

}


/********************************************************************
*																	*
*					Thread Lock ( Mutex )							*
*																	*
********************************************************************/

int PlatformCreateMutex(WS_MUTEX* m){
	pthread_mutexattr_t att;

	if ( m == 0 ){
		printf("PlatformLockMutex wurde nicht initialisiert\n");
		return EINVAL;
	}

	pthread_mutexattr_init(&att);
/*	pthread_mutexattr_settype(&att,PTHREAD_MUTEX_ERRORCHECK_NP); */
	m->locked = 0;
	return pthread_mutex_init(&m->handle,&att);
}

int PlatformLockMutex(WS_MUTEX* m){
	/*printf("Locking %X\n",m);*/
	int ret;	
	if ( m == 0 ){
		printf("PlatformLockMutex wurde nicht initialisiert\n");
		return EINVAL;
	}
	ret =  pthread_mutex_lock(&m->handle);
	m->locked++;
	if ( m->locked > 1){
		printf("PlatformLockMutex wurde doppelt gelocked\n");
	}
	return ret;
}

int PlatformUnlockMutex(WS_MUTEX* m){
	/*printf("Unlocking %X\n",m);*/
	if ( m == 0 ){
		printf("PlatformLockMutex wurde nicht initialisiert\n");
		return EINVAL;
	}

	/* coverity[missing_lock] - locked is debug-only counter, caller holds lock */
	m->locked--;
	return pthread_mutex_unlock( &m->handle );
}

int PlatformDestroyMutex(WS_MUTEX* m){
	/*printf("Destroy Mutex\n");*/
	if ( m == 0 ){
		return EINVAL;
	}
	if ( m->locked != 0){
		printf("PlatformDestroyMutex Mutex ist gelocked\n");
	}
	return pthread_mutex_destroy(&m->handle);
}



int PlatformCreateSem(WS_SEMAPHORE_TYPE* sem, int init_value){
	return sem_init( sem, 0, init_value);
}

int PlatformDestroySem(WS_SEMAPHORE_TYPE* sem){
	return sem_destroy( sem );
}

int PlatformPostSem(WS_SEMAPHORE_TYPE* sem) {
	return sem_post( sem );
}

int PlatformWaitSem(WS_SEMAPHORE_TYPE* sem) {
	int r;

    do {
            r = sem_wait( sem );
    } while (r == -1 && errno == EINTR);
    
	return r;
}

static void* run_thread( void* ptr ){
	 platform_thread_function func = (platform_thread_function) ptr;
	 func();
	 return 0;
}

void PlatformCreateThread( WS_THREAD* handle, platform_thread_function func ){
	 pthread_create( handle, NULL, run_thread, func);
}


int		PlatformClose(int socket){
	return close( socket );
}


int PlatformGetPID( void ){
	return getpid();
}

