/*

SPDX-License-Identifier: MPL-2.0

 libCWebUI
 Copyright (C) 2007 - 2019  Ramin Seyed-Moussavi

 Projekt URL : https://github.com/lordrasmus/libcwebui

 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at https://mozilla.org/MPL/2.0/. 

*/


#include "webserver.h"
#include "intern/system_file_access.h"


#ifdef DMALLOC
#include <dmalloc/dmalloc.h>
#endif


#define LM1 (char*)s->header->If_Modified_Since
#define LM2 (char*)info->lastmodified

#define ET1 (char*)s->header->etag
#define ET2 (char*)info->etag

static int checkCacheHeader(http_request* s, WebserverFileInfo *info) {
	if ((s == 0) || (s->header == 0) || (info == 0)  ) {
		return -1;
	}

#ifndef WEBSERVER_DISABLE_CACHE
	if (info->FileType != FILE_TYPE_HTML) {
		if (likely( ( ( s->header->etag != 0 ) && ( info->etag != 0) ) ) ) {
			/*
				ETag Header vorhanden
			*/
			if ((0 == strcmp(ET1, ET2))) {
#ifdef _WEBSERVER_CACHE_DEBUG_
				LOG(CACHE_LOG,NOTICE_LEVEL,s->socket->socket, "Cache Hit ETag %s",info->FilePath );
				LOG(CACHE_LOG,NOTICE_LEVEL,s->socket->socket, "Request %s",s->header->etag );
				LOG(CACHE_LOG,NOTICE_LEVEL,s->socket->socket, "File    %s",info->etag );
#endif
				return 1;
			} else {
#ifdef _WEBSERVER_CACHE_DEBUG_
				LOG(CACHE_LOG,NOTICE_LEVEL,s->socket->socket, "Cache Miss ETag %s",info->FilePath );
				LOG(CACHE_LOG,NOTICE_LEVEL,s->socket->socket, "Request %s",s->header->etag );
				LOG(CACHE_LOG,NOTICE_LEVEL,s->socket->socket,"File    %s",info->etag );
#endif
			}
		} else if ((s->header->If_Modified_Since != 0)
				&& (info->lastmodified != 0)) {
			/*
					Modified_Since Header vorhanden
			*/
			if ((0 == strcmp(LM1, LM2)) && (strlen(LM1) == strlen(LM2))) {
#ifdef _WEBSERVER_CACHE_DEBUG_
				LOG(CACHE_LOG,NOTICE_LEVEL,s->socket->socket,"Cache Hit Last Modified %s",info->FilePath );
				LOG(CACHE_LOG,NOTICE_LEVEL,s->socket->socket, "Request %s",s->header->If_Modified_Since );
				LOG(CACHE_LOG,NOTICE_LEVEL,s->socket->socket, "File    %s",info->lastmodified );
#endif
				return 1;
			} else {
#ifdef _WEBSERVER_CACHE_DEBUG_
				LOG(CACHE_LOG,NOTICE_LEVEL,s->socket->socket,"Cache Hit Last Modified %s",info->FilePath );
				LOG(CACHE_LOG,NOTICE_LEVEL,s->socket->socket, "Request %s",s->header->If_Modified_Since );
				LOG(CACHE_LOG,NOTICE_LEVEL,s->socket->socket, "File    %s",info->lastmodified );
#endif
			}
		} else {
#ifdef _WEBSERVER_CACHE_DEBUG_
			LOG(CACHE_LOG,NOTICE_LEVEL,s->socket->socket, "Cache Miss %s no Cache Headers",info->FilePath );
#endif
		}
	}
#endif


	return 0;

}

int sendFile(http_request* s, WebserverFileInfo *file) {
	if ((s == 0) || (s->socket == 0) || (file == 0)) {
		return -1;
	}

	switch ( file->fs_type ){
		case FS_LOCAL_FILE_SYSTEM :
			if ( 1 == local_file_system_check_file_modified( file ) ){

				#ifndef WEBSERVER_DISABLE_CACHE
				generateEtag ( file );
				#endif
			}

			break;
		case FS_WNFS:	// Inhalt von WNFS Datein kann direkt versendet werden
			break;
		case FS_BINARY :
			break;
	}

	if (likely( 1 == checkCacheHeader(s,file) )) {
		sendHeaderNotModified(s, file);
		return 0;
	}

	if (0 > sendHeader(s, file, file->DataLenght)){
		return -1;
	}

	s->socket->send_file_info = file;

	return 0;
}
