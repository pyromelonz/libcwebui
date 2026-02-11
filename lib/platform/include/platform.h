/*

SPDX-License-Identifier: MPL-2.0

 libCWebUI
 Copyright (C) 2007 - 2019  Ramin Seyed-Moussavi

 Projekt URL : https://github.com/lordrasmus/libcwebui

 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at https://mozilla.org/MPL/2.0/. 

*/


#ifndef _PLATFORM_H_
#define _PLATFORM_H_

#include "platform-defines.h"
#include "platform-file-access.h"
#include "platform-sockets.h"




/********************************************************************
*																	*
*					Speicher Managment								*
*																	*
********************************************************************/

void*	PlatformMalloc(SIZE_TYPE size);
void	PlatformFree( void* mem );

/********************************************************************
*																	*
*					System Informationen							*
*																	*
********************************************************************/

void 			PlatformGetGUID(char* buf,SIZE_TYPE length);
void 			PlatformGetIPv6(char* bytes);
TIME_TYPE   	PlatformGetTick(void);
unsigned long	PlatformGetTicksPerSeconde(void);
int             PlatformGetPID(void);


/********************************************************************
*																	*
*					Thread Lock ( Mutex )							*
*																	*
********************************************************************/


int PlatformCreateMutex(WS_MUTEX* m);
int PlatformLockMutex(WS_MUTEX* m);
int PlatformUnlockMutex(WS_MUTEX* m);
int PlatformDestroyMutex(WS_MUTEX* m);

int PlatformCreateSem(WS_SEMAPHORE_TYPE* sem, int init_value);
int PlatformDestroySem(WS_SEMAPHORE_TYPE* sem);
int PlatformWaitSem(WS_SEMAPHORE_TYPE* sem);
int PlatformPostSem(WS_SEMAPHORE_TYPE* sem);

typedef void (*platform_thread_function)(void);

void PlatformCreateThread( WS_THREAD* handle, platform_thread_function func );

#endif

