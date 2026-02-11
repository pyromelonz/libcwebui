/*

SPDX-License-Identifier: MPL-2.0

 libCWebUI
 Copyright (C) 2007 - 2019  Ramin Seyed-Moussavi

 Projekt URL : https://github.com/lordrasmus/libcwebui

 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at https://mozilla.org/MPL/2.0/. 

*/


#ifndef _PLATFORM_INCLUDES_H_
#define _PLATFORM_INCLUDES_H_

#ifndef __ZEPHYR__
	#error not a zephyr system
#endif

//#include <posix/time.h> // geht nicht mit #include <time.h>
//#include <posix/semaphore.h>

#include <kernel.h>

#include <ctype.h>
#include <inttypes.h>
#include <net/net_ip.h>
#include <net/socket.h>


#define SIZE_TYPE size_t
#define TIME_TYPE time_t

#define FILE_OFFSET off_t
#define FILE_OFF_CAST(x) ((intmax_t)(x))
#define FILE_OFF_PRINT_HEX  PRIxMAX
#define FILE_OFF_PRINT_INT  PRIdMAX

#define WS_MUTEX_TYPE		struct k_mutex
#define WS_SEMAPHORE_TYPE	struct k_sem 
//#define WS_THREAD			pthread_t

#define WebServerPrintf printf


#endif
