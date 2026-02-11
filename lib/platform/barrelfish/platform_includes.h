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

#ifdef BARRELFISH

#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define FILE_OFFSET long unsigned int
#define FILE_OFF_CAST(x) ((intmax_t)(x))
#define FILE_OFF_PRINT_HEX  PRIxMAX
#define FILE_OFF_PRINT_INT  PRIdMAX

#define WS_MUTEX_TYPE int
#define WS_SEMAPHORE_TYPE int

#define TIME_TYPE int
#define SIZE_TYPE long unsigned int




#endif
#endif
