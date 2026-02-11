/*

SPDX-License-Identifier: MPL-2.0

 libCWebUI
 Copyright (C) 2007 - 2019  Ramin Seyed-Moussavi

 Projekt URL : https://github.com/lordrasmus/libcwebui

 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at https://mozilla.org/MPL/2.0/. 

*/

#ifndef _WEBSERVER_SERVER_H_
#define _WEBSERVER_SERVER_H_

#include <stdarg.h>
#include <webserver.h>


#define HTTP_UNKNOWN_METHOD 0
#define HTTP_POST 1
#define HTTP_GET 2
#define HTTP_OPTIONS 3

#define MULTIPART_FORM_DATA						1
#define APPLICATION_X_WWW_FORM_URLENCODED		2


extern unsigned char *g_lastmodified;  /* Wird fur Binary File gebraucht */


#ifdef __cplusplus
extern "C" {
#endif

int getHttpRequest(socket_info* sock);
int sendHTMLFile(http_request* s, WebserverFileInfo *file);

void sendWebsocketChunk ( socket_info* sock,const unsigned char* text,const unsigned int length );
void printWebsocketChunk ( socket_info* sock,const char *fmt,... );

int isChunkListbigger(list_t* liste, int bytes);
void sendHeaderChunk(socket_info* sock,const char* text,const unsigned int length);
void sendHeaderChunkEnd(socket_info* sock);
void printHeaderChunk(socket_info* sock,const char *fmt,...) __attribute__ ((format (printf, 2, 3)))  ;
void vprintHeaderChunk(socket_info* sock, const char *fmt, va_list argptr);

int  printHTMLChunk(socket_info* sock,const char *fmt,...) __attribute__ ((format (printf, 2, 3))) ;
int vprintHTMLChunk(socket_info* sock, const char *fmt, va_list argptr);
void sendHTMLChunk(socket_info* sock,const char* text,const unsigned int length);
void sendHTMLChunkVariable(socket_info* sock,ws_variable* var);

unsigned long getChunkListSize(list_t* liste);

void generateOutputBuffer(socket_info* sock);

int sendFile(http_request* s, WebserverFileInfo *file);

int sendFileNotFound(http_request* s);
int sendAccessDenied(http_request* s);
int sendMethodNotAllowed(socket_info *sock);
int sendMethodBadRequest(socket_info *sock);
int sendMethodBadRequestLineToLong(socket_info *sock);
int sendMethodBadRequestMissingHeaderLines(socket_info *sock);
int sendInternalError(socket_info *sock);

int checkBuilinSites(http_request* s);


WebserverFileInfo *getFile(char *name);
WebserverFileInfo *getFileById(int id);

void WebserverPrintInfos( void );

void register_url_function( char* url, url_handler_func func );

#ifdef __cplusplus
}
#endif

#endif
