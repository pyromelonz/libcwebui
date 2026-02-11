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


#define FREEBSD

#include <stdio.h>
#include <ctype.h>
#include <dlfcn.h>
#include <errno.h>

#include <limits.h>
#include <unistd.h>

#include <inttypes.h>


#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>



#include <string.h>
#include <strings.h>

#include <sys/types.h>
#include <unistd.h>

#include <netinet/in.h>

#include <sys/ioctl.h>
#include <sys/select.h>


#define SIZE_TYPE size_t
#define SIZE_TYPE_PRINT_DEZ "zu"

#define TIME_TYPE time_t

#define FILE_OFFSET off_t
#define FILE_OFF_CAST(x) ((intmax_t)(x))
#define FILE_OFF_PRINT_HEX PRIxMAX
#define FILE_OFF_PRINT_INT PRIdMAX

#define WS_MUTEX_TYPE		pthread_mutex_t
#define WS_SEMAPHORE_TYPE	sem_t
#define WS_THREAD			pthread_t

#define WebServerPrintf printf



#endif
