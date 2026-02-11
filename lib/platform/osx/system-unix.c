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

#ifdef WEBSERVER_USE_SSL
#include <openssl/rand.h>
#endif

#include "webserver.h"


#include <MacTypes.h>
#include <mach/mach_time.h>
#include <CoreFoundation/CFUUID.h>

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



/*********************************************************************

					System informationen

*********************************************************************/


TIME_TYPE PlatformGetTick ( void ) {
	enum { NANOSECONDS_IN_SEC = 1000 * 1000 * 1000 };
    static double multiply = 0;
    if (multiply == 0)
    {
        mach_timebase_info_data_t s_timebase_info;
        kern_return_t result = mach_timebase_info(&s_timebase_info);
        assert(result == noErr);
        // multiply to get value in the nano seconds
        multiply = (double)s_timebase_info.numer / (double)s_timebase_info.denom;
        // multiply to get value in the seconds
        multiply /= NANOSECONDS_IN_SEC;
    }
    return mach_absolute_time() * multiply;
}

unsigned long PlatformGetTicksPerSeconde ( void ) {
    return (unsigned long)1;
}

#ifdef WEBSERVER_USE_SESSIONS
void 	PlatformGetGUID ( char* buf,SIZE_TYPE length ) {
	
	if ( buf == 0 ){
    	return;
    }
    
    //CFAllocatorRef alloc_def = CFAllocatorGetDefault();

	CFUUIDRef uuid = CFUUIDCreate( 0 );
	CFStringRef uuid_str = CFUUIDCreateString( 0, uuid);
	
	CFStringEncoding encodingMethod = CFStringGetSystemEncoding();
	const char *uuid_c = CFStringGetCStringPtr( uuid_str, encodingMethod);
	
	int ret;
    
	memset( buf, 0 , length);
    ret = snprintf ( buf, length, "%s", uuid_c );
    buf[length-1]='\0';
    
    #warning is free correct ?
    free( (void *)uuid_str );
    free( (void *)uuid );
}
#endif






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
	*sem = dispatch_semaphore_create( init_value );
	return 0;
}

int PlatformDestroySem(WS_SEMAPHORE_TYPE* sem){
	dispatch_release( *sem );
	return 0;
}

int PlatformPostSem(WS_SEMAPHORE_TYPE* sem) {
	dispatch_semaphore_signal( *sem );
	return 0;
}

int PlatformWaitSem(WS_SEMAPHORE_TYPE* sem) {
	dispatch_semaphore_wait( *sem, DISPATCH_TIME_FOREVER );
	return 0;
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
