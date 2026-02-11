/*

SPDX-License-Identifier: MPL-2.0

 libCWebUI
 Copyright (C) 2007 - 2019  Ramin Seyed-Moussavi

 Projekt URL : https://github.com/lordrasmus/libcwebui

 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at https://mozilla.org/MPL/2.0/. 

*/


#ifndef _PLATFORM_DEFINES_H_
#define _PLATFORM_DEFINES_H_


#ifndef bool
	#define bool unsigned char
	#define true 	1
	#define false	0
#endif

#ifdef _WIN32
	#include "../win/platform_includes.h"
	#define PLATFORM_DETECTED
#endif

#if defined( __NuttX__ ) && ! defined( PLATFORM_DETECTED )
    #include "../nuttx/platform_includes.h"
	#define PLATFORM_DETECTED
#endif


#if defined( __linux__ ) && ! defined( PLATFORM_DETECTED )
	#include "../linux/platform_includes.h"
	#define PLATFORM_DETECTED
#endif

#if defined ( __APPLE__ ) && ! defined( PLATFORM_DETECTED )
	#include "../osx/platform_includes.h"
	#define PLATFORM_DETECTED
#endif

#if defined( __FreeBSD__ ) && ! defined( PLATFORM_DETECTED )
	#include "../freebsd/platform_includes.h"
	#define PLATFORM_DETECTED
#endif

#if defined( BARRELFISH ) && ! defined( PLATFORM_DETECTED )
	#include "../barrelfish/platform_includes.h"
	#define PLATFORM_DETECTED
#endif

#if defined( __ZEPHYR__ ) && ! defined( PLATFORM_DETECTED )
	#include "../zephyr/platform_includes.h"
	#define PLATFORM_DETECTED
#endif

#if defined( BSD ) && ! defined( PLATFORM_DETECTED )
#define WebServerPrintf printf
#include <sys/socket.h>
#include <stdlib.h>
#include <unistd.h>
#endif


#if defined( DSTni )  && ! defined( PLATFORM_DETECTED )
#include "config.h"
#include "mt.h"
#include "dsttypes.h"
#define WebServerPrintf ConsolePrintf
#endif

#if defined( NET_OS ) && ! defined( PLATFORM_DETECTED )
#define WebServerPrintf printf
#endif

#ifndef PLATFORM_DETECTED
	#error build platform unknown
#endif


#ifndef WDT_RESET
	#define WDT_RESET
#endif


typedef struct {
	char locked;
	WS_MUTEX_TYPE handle;
}WS_MUTEX;


#endif

