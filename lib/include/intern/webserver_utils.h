/*

SPDX-License-Identifier: MPL-2.0

 libCWebUI
 Copyright (C) 2007 - 2019  Ramin Seyed-Moussavi

 Projekt URL : https://github.com/lordrasmus/libcwebui

 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at https://mozilla.org/MPL/2.0/. 

*/


#ifndef _WEBSERVER_UTILS_H_
#define _WEBSERVER_UTILS_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

unsigned int readInt( void );


int stringfind(const char *buffer,const char *pattern);
long stringnfind(const char *buffer,const char *pattern,unsigned int buffer_length);

int stringfindAtPos(const char *buffer,const char *pattern);

void Webserver_strncpy( char *dest, unsigned int dest_size,  const char *src, unsigned int count);
unsigned long Webserver_strlen( char *text);

/**
 * Duplicate up to max_len characters using WebserverMalloc
 * @param src Source buffer (must not be NULL)
 * @param max_len Maximum characters to read/copy (not including terminator)
 * @return Newly allocated copy or NULL on failure. Always 0-terminated.
 */
char* Webserver_strndup(const char* src, uint32_t max_len);



unsigned int getHTMLDateFormat( char* buffer,int day,int month,int year,int hour,int minute);

/*
 int pos=0; in der funktion definieren zum benutzen, in pos steht dann die string laenge
*/
#define BufferPrint( ARGS  ) pos+=snprintf((char*)retBuffer+pos,ret_length-pos, ARGS)

void 	generateGUID ( char* buf,int length );

void 	convertBinToHexString(const unsigned char* bin,int bin_length,char* text, int text_length);



#ifdef __cplusplus
}
#endif

#endif
