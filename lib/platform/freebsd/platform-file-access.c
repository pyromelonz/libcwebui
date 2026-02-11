/*

SPDX-License-Identifier: MPL-2.0

 libCWebUI
 Copyright (C) 2007 - 2019  Ramin Seyed-Moussavi

 Projekt URL : https://github.com/lordrasmus/libcwebui

 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at https://mozilla.org/MPL/2.0/. 

*/



#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>

#include "webserver.h"


#ifdef DMALLOC
#include <dmalloc/dmalloc.h>
#endif

static FILE *g_fp;
unsigned short l;


/**************************************************************
 *                                                             *
 *                   Datei Operationen                         *
 *                                                             *
 **************************************************************/

int PlatformOpenDataReadStream( const char* name ) {
	struct stat st;

	g_fp = fopen( ( char* ) name, "rb");
	if (g_fp == NULL){ 
		return false;
	}

	/* Auf normale Datei Prüfen */
	if ( 0 != fstat(fileno(g_fp), &st) ){
		printf("fstat error : %m\n");
		return false;
	}

	if (!S_ISREG(st.st_mode)){
		return false;
	}

	return fileno(g_fp);
}


int PlatformGetFileSize(void) {
	struct stat st;
	if ( 0 != fstat(fileno(g_fp), &st) ){
		printf("fstat error : %m\n");
		return 0;
	}

	return st.st_size;
}

char PlatformCloseDataStream(void) {
	fclose(g_fp);
	return true;
}

void PlatformResetDataStream(void) {
	fseek(g_fp, 0, SEEK_SET); /* Anfang der Datei */
}

int PlatformReadBytes(unsigned char *data, FILE_OFFSET lenght) {
	return fread(data, 1, lenght, g_fp);
}

void PlatformSeek(long offset) {
	if ( 0 != fseek(g_fp, offset, SEEK_CUR) ){
		printf("fseek error : %m\n");
	}
}

void PlatformSeekToPosition(long position) {
	if ( 0 != fseek(g_fp, position, SEEK_SET) ){
		printf("fseek error : %m\n");
	}
}


#if 0
struct stat {
 off_t     st_size;    // total size, in bytes
 time_t    st_atime;   // time of last access
 time_t    st_mtime;   // time of last modification
 time_t    st_ctime;   // time of last status change
 };
#endif

int PlatformGetFileInfo(WebserverFileInfo* file, int* time_changed, int *new_size) {

	struct stat st;
	struct tm ts_var;
	struct tm *ts;
	size_t len;
	char* buffer;
	time_t f_sec;
	time_t f_nsec;

	if ( 0 > stat( (char*) file->FilePath, &st) ){
		return 0;
	}

	/* FreeBSD supports POSIX.1-2008 st_mtim */
	f_sec = st.st_mtim.tv_sec;
	f_nsec = st.st_mtim.tv_nsec;

	*new_size = st.st_size;
	*time_changed = 0;

	if ( ( file->last_mod_sec != (unsigned long int)f_sec) || ( file->last_mod_nsec != (unsigned long int)f_nsec) ){

		*time_changed = 1;

		file->last_mod_sec = f_sec;
		file->last_mod_nsec = f_nsec;



		buffer = (char *) WebserverMalloc( 150 );
		ts = gmtime_r( &st.st_mtime, &ts_var);
		len = getHTMLDateFormat(buffer, ts->tm_mday, ts->tm_mon, ts->tm_year + 1900, ts->tm_hour, ts->tm_min);

		if (file->lastmodified == 0) {
			file->lastmodified = (char *) WebserverMalloc( len + 1 );
			memcpy(file->lastmodified, buffer, len + 1);
			file->lastmodifiedLength = len;
			WebserverFree(buffer);
			return true;
		} else {
			if (0 != strcmp(file->lastmodified, buffer)) {
				WebserverFree(file->lastmodified);
				file->lastmodified = (char *) WebserverMalloc( len + 1 );
				memcpy(file->lastmodified, buffer, len + 1);
				file->lastmodifiedLength = len;
				WebserverFree(buffer);
				return true;
			}
		}
		WebserverFree(buffer);
	}

	return 1;
}

