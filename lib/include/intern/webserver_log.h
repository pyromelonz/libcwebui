/*

SPDX-License-Identifier: MPL-2.0

 libCWebUI
 Copyright (C) 2007 - 2019  Ramin Seyed-Moussavi

 Projekt URL : https://github.com/lordrasmus/libcwebui

 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at https://mozilla.org/MPL/2.0/. 

*/



#ifndef _WEBSERVER_LOG_H_
#define _WEBSERVER_LOG_H_

#include "webserver.h"

typedef enum {
	MESSAGE_LOG,
	CONNECTION_LOG,
	HEADER_PARSER_LOG,
	TEMPLATE_LOG,
	PLATFORM_LOG,
	SOCKET_LOG,
	HANDLER_LOG,
	FILESYSTEM_LOG,
	CACHE_LOG,
	SSL_LOG,
	WEBSOCKET_LOG,
	VARIABLE_LOG,
	CONFIG_LOG,
	PROXY_LOG,
	SESSION_LOG,
	HASHMAP_LOG
}LogChannels;

typedef enum {
	DEBUG_LEVEL,
	NOTICE_LEVEL,
	INFO_LEVEL,
	WARNING_LEVEL,
	ERROR_LEVEL
}LogLevels;


#ifdef __cplusplus
extern "C" {
#endif


#ifdef _MSC_VER
	#define LOG(a,b,c,...) addLog(a,b,__FILE__, __LINE__,__FUNCTION__,c, __VA_ARGS__)
#endif
#ifdef __GNUC__
	#if __GNUC__ > 2
		#define LOG(a,b,c,d,ARGS...) addLog(a,b,(char*)__BASE_FILE__, __LINE__,__FUNCTION__,c,(char*)d, ##ARGS)
	#else
		#define LOG(a,b,c,d,ARGS...)
	#endif
#endif



void addLog(LogChannels channel,LogLevels level,char* filename,int fileline,const char* function,int socket,char* text,...) __attribute__ ((format (printf, 7, 8)));

#ifdef __cplusplus
}
#endif


#endif

