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


#ifdef DMALLOC
#include <dmalloc/dmalloc.h>
#endif


unsigned int readInt(void) {
	unsigned char intbuffer[4];
	unsigned int tmp;
	int ret = PlatformReadBytes(intbuffer, 4);
	if ( ret != 4 ){
		printf("readInt: Error Reading Bytes %m\n");
		return 0;
	}
	tmp  = ((unsigned int)intbuffer[0]) << 0;
	tmp += ((unsigned int)intbuffer[1]) << 8;
	tmp += ((unsigned int)intbuffer[2]) << 16;
	tmp += ((unsigned int)intbuffer[3]) << 24;
	return tmp;
}


int stringfind(const char *buffer, const char *pattern) {
	int length;
	int i, i2 = 0;
	if ( buffer == 0){
		return 0;
	}
	length = (int) strlen((char*) buffer);
	for (i = 0; i < length; i++){
		if (buffer[i] == pattern[i2]) {
			i2++;
			if (pattern[i2] == '\0'){
				return i;
			}
		} else {
			i2 = 0;
		}
	}
	return 0;

}

long stringnfind(const char *buffer, const char *pattern, unsigned int buffer_length) {
	unsigned int i, i2 = 0;
	
	if ( ( buffer == 0 ) || ( pattern == 0 ) ){
		return -1;
	}
	
	for (i = 0; (i < buffer_length); i++){
		if (buffer[i] == pattern[i2]) {
			i2++;
			if (pattern[i2] == '\0'){
				 return i;
			}
		} else {
			i2 = 0;
		}
	}
	return -1;
}

int stringfindAtPos(const char *buffer, const char *pattern) {
	int i, i2 = 0;
	int lenght = (int) strlen((char*) pattern);
	for (i = 0; i < lenght; i++){
		if (buffer[i] == pattern[i2]) {
			i2++;
			if (pattern[i2] == '\0'){
				return i;
			}
			if (i2 == lenght){
				return i;
			}
		} else {
			return 0;
		}
	}
	return 0;

}

void Webserver_strncpy(char *dest, unsigned int dest_size, const char *src, unsigned int count) {
	if ( ( dest == 0 ) || ( src == 0 ) ) {
		return;
	}
#ifdef _MSC_VER
#define Webserver_strncpy_ok
	strncpy_s(dest,dest_size,src,count);
#endif
#ifdef __GNUC__
#define Webserver_strncpy_ok
	if ( dest_size >= count){
		strncpy(dest, src, count);
	}else{
		strncpy(dest, src, dest_size);
	}
#endif
#ifndef Webserver_strncpy_ok
#error not implemented
#endif
	if ( dest_size >= 1 ){
		dest[dest_size-1] = '\0';
	}
}

unsigned long Webserver_strlen(char *text) {
	return strlen(text);
}

static void getHTMLMonth(unsigned char month, char* buffer, SIZE_TYPE size) {
	switch (month) {
	case 1:
		snprintf( buffer, size, "Jan");
		break;
	case 2:
		snprintf( buffer, size, "Feb");
		break;
	case 3:
		snprintf( buffer, size, "Mar");
		break;
	case 4:
		snprintf( buffer, size, "Apr");
		break;
	case 5:
		snprintf( buffer, size, "May");
		break;
	case 6:
		snprintf( buffer, size, "Jun");
		break;
	case 7:
		snprintf( buffer, size, "Jul");
		break;
	case 8:
		snprintf( buffer, size, "Aug");
		break;
	case 9:
		snprintf( buffer, size, "Sep");
		break;
	case 10:
		snprintf( buffer, size, "Oct");
		break;
	case 11:
		snprintf( buffer, size, "Nov");
		break;
	case 12:
		snprintf( buffer, size, "Dec");
		break;
	default:
		snprintf( buffer, size, "Unknown Month %d", month);
		break;
	}
}

/* HTML date  format http://www.hackcraft.net/web/datetime/#rfc822} */

unsigned int getHTMLDateFormat(char* buffer, int day, int month, int year, int hour, int minute) {
	char bb[30];

	getHTMLMonth(month, bb, 30);

	return snprintf(buffer, 1000, "%02d %s %04d %02d:%02d GMT", day, bb, year, hour, minute);

}

void convertBinToHexString(const unsigned char* bin, int bin_length, char* text, int text_length) {
	int l;
	for (l = 0; l < bin_length; l++) {
		text[l * 2 + 0] = bin[l] >> 4;
		if (text[l * 2 + 0] <= 9){
			text[l * 2 + 0] += 48;
		}else{
			text[l * 2 + 0] += 55;
		}
		text[l * 2 + 1] = bin[l] & 0x0F;
		if (text[l * 2 + 1] <= 9){
			text[l * 2 + 1] += 48;
		}else{
			text[l * 2 + 1] += 55;
		}
	}
	text[text_length] = '\0';
}

void generateGUID(char* buf, int length) {

	PlatformGetGUID(buf, length);
}

char* Webserver_strndup(const char* src, uint32_t max_len) {
	uint32_t len;
	char* copy;

	if (src == NULL || max_len == 0) {
		return NULL;
	}

	/* Find string length, but never read more than max_len bytes */
	for (len = 0; len < max_len; len++) {
		if (src[len] == '\0') {
			break;
		}
	}

	/* Overflow check for len + 1 */
	if (len == UINT32_MAX) {
		return NULL;
	}

	copy = (char*)WebserverMalloc(len + 1);
	if (copy == NULL) {
		return NULL;
	}

	memcpy(copy, src, len);
	copy[len] = '\0';

	return copy;
}

