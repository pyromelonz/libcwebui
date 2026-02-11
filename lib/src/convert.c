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


unsigned int getInt(const unsigned char *buffer)
{
    unsigned int tmp=0;
    tmp += (unsigned int)buffer[0];
    tmp += (unsigned int)buffer[1] << 8;
    tmp += (unsigned int)buffer[2] << 16;
    tmp += (unsigned int)buffer[3] << 24;
    return tmp;
}

unsigned char toHex(unsigned char in) {
	if ((in <= '9') && (in >= '0')) {
		return (unsigned char)(in - '0');
	}

	if ((in >= 'a') && (in <= 'f')) {
		return (unsigned char)(in - 87);
	}

	if ((in >= 'A') && (in <= 'F')) {
		return (unsigned char)(in - 55 );
	}

	return 0;
}


void url_decode(char *line) {
	unsigned char hex;
	size_t i_in=0,i_out=0;
	size_t lenght;

	lenght = strlen( line );
	for (i_in = 0; i_in <= lenght; i_in++) {
		if ( (unlikely(line[i_in]=='%')) && ( ( lenght - i_in ) > 2 ) )  {
			hex = (unsigned char)(toHex(line[i_in + 1]) << 4);
			hex =  (unsigned char)( hex + toHex(line[i_in + 2]) );
			/*  hexcode des zeichens als char speichern und 2 zeichen loeschen */
			line[i_out] = hex; 
			i_out++;
			i_in+=2;
			continue;
		}
		if ( unlikely(line[i_in] == '+') ){ /* + durch whitespace ersetzen */
			line[i_out] = ' ';
			i_out++;
			continue;
		}
		line[i_out] = line[i_in];
		i_out++;
	}
	
}


