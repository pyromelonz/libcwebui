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
#include "intern/reverse_proxy.h"

#ifdef DMALLOC
#include <dmalloc/dmalloc.h>
#endif



static void createParameter(HttpRequestHeader *header, char* name, unsigned int name_length, char* value, unsigned int value_length) {
	ws_variable *var;


	url_decode(name);
	var = newVariable(header->parameter_store, name, 0 );

	if (value != 0) {
		url_decode(value);
		/* Check for script injection in parameter values (XSS prevention) */
		if (stringfind(value, "<script>") > 0 || stringfind(value, "</script>") > 0) {
			header->error = 1;
		}
		setWSVariableString(var, value);
	}


}

enum ParameterParseStates{
	PARSE_NAME,
	PARSE_VALUE
};

static void recieveParameterFromGet(char *line, HttpRequestHeader *header, int len) {
	int  i;

	enum ParameterParseStates state = PARSE_NAME;

	char* name_start = line;
	char* name_end = line;

	char* value_start = 0;
	char* value_end = 0;

	char bak1,bak2;

	if ( len < 1 ){
		return;
	}

#ifdef _WEBSERVER_DEBUG_
	WebServerPrintf("recieveParameterFromGet : %s\n", line);
#endif


	for (i = 0; i < len; i++) {

		if ( ( state == PARSE_NAME ) && ( line[i] == '&' ) ){
			name_end = &line[i];

			if ( ( name_end - name_start ) > 0 ){

				bak1 = *name_end;
				*name_end = '\0';

				createParameter(header, name_start, name_end - name_start, 0, 0 );

				*name_end = bak1;
			}

			name_start = &line[i+1];

			continue;
		}

		if ( ( state == PARSE_NAME ) && ( line[i] == '=' ) ){
			name_end = &line[i];
			value_start= &line[i+1];
			state = PARSE_VALUE;
			continue;
		}

		if ( ( state == PARSE_VALUE ) && ( line[i] == '&' ) ){
			value_end = &line[i];

			bak1 = *name_end;
			*name_end = '\0';

			bak2 = *value_end;
			*value_end = '\0';

			createParameter(header, name_start, name_end - name_start, value_start, value_end - value_start);

			*name_end = bak1;
			*value_end = bak2;

			value_start = 0;
			value_end = 0;

			name_start = &line[i+1];
			name_end = name_start;

			state = PARSE_NAME;
			continue;
		}

	}


	if ( state == PARSE_NAME ) {
		name_end = &line[i];

		bak1 = *name_end;
		*name_end = '\0';

		createParameter(header, name_start, name_end - name_start, 0, 0 );

		*name_end = bak1;
	}

	if ( state == PARSE_VALUE ) {
		value_end = &line[i];

		bak1 = *name_end;
		*name_end = '\0';

		bak2 = *value_end;
		*value_end = '\0';

		createParameter(header, name_start, name_end - name_start, value_start, value_end - value_start);

		*name_end = bak1;
		*value_end = bak2;

	}

	/* WebServerPrintf("Anzahl Parameter : %d\n",header->paramtercount);
	 for(pos=0;pos<header->paramtercount;pos++){
	 WebServerPrintf("Parameter %d -> Name : %s \tValue : %s\n",pos,header->parameter[pos]->name,header->parameter[pos]->value);
	 }
	 */
}

static void url_sanity_check( HttpRequestHeader *header ){
	int pos;

	/* Preserve existing error (e.g., from parameter validation) */
	if (header->error == 1) {
		return;
	}
	header->error = 0;

	// Check for null byte injection BEFORE decode (%00 becomes \0 which terminates strings)
	pos = stringfind(header->url, "%00");
	if (pos > 0) {
		header->error = 1;
		return;
	}

	// URL decode FIRST, then check for malicious patterns
	// This prevents bypass via URL encoding (e.g., %3Cscript%3E)
	url_decode( header->url );

	// Check for directory traversal
	pos = stringfind(header->url, "..");
	if (pos > 0) {
		header->error = 1;
		return;
	}

	// Check for script injection (XSS)
	pos = stringfind(header->url, "<script>");
	if (pos > 0) {
		header->error = 1;
		return;
	}
	pos = stringfind(header->url, "</script>");
	if (pos > 0) {
		header->error = 1;
		return;
	}
}


static int header_attr_compare( char* text, unsigned int text_length, char* line,  unsigned int line_length ){

	unsigned int i = 0;

	if ( line_length < text_length ){
		return 0;
	}

	for( i = 0; i < line_length && text[i] ; i++){
		if ( tolower(text[i]) != tolower(line[i]) ){
			return 0;
		}
	}
	
	return 1;
}


#define CHECK_HEADER_LINE2(a,b)  h_len = strlen(a); \
if (!strncmp((char*)line2,a,h_len)) \
{ \
	int crlf_pos = stringfind(line2, "\r\n"); \
	if (crlf_pos > (int)h_len) { \
		len = crlf_pos - h_len - 1; \
		if ( header->b != 0) WebserverFree(header->b); \
		header->b = (char*)WebserverMalloc( len + 1 ); \
		Webserver_strncpy((char*)header->b,len+1,(char*)&line2[h_len],len); \
		line2 = &line2[h_len + len ]; length2 -= h_len + len ; \
	} \
	continue ; \
}

int analyseFormDataLine(socket_info* sock, char *line, unsigned int length, HttpRequestHeader *header) {

	SIZE_TYPE h_len;
	unsigned long len;
	char *line2;

	unsigned int length2;

	length2 = length;
	line2 = line;

	while( length2 > 0 ){

		if ( 0 == strncmp(line2, "\r\n\r\n",4) ){
			/* Ende des Form Multipart Header Abschnitts */
			line2+=4;
			break;
		}

		/* POST Form Data Lines */
		CHECK_HEADER_LINE2("Content-Disposition: ",Content_Disposition)

		CHECK_HEADER_LINE2("Content-Type: ",Content_Type)

		length2--;
		line2++;

	}

#ifdef ENABLE_DEVEL_WARNINGS
	#warning "Noch genauer Testen"
#endif

	return line2 - line;
}


#define CHECK_HEADER_LINE(a,b)  h_len = strlen(a); \
if (header_attr_compare( a, h_len, line, length)) \
{ \
	len = length - h_len; \
	if ( header->b != 0) WebserverFree(header->b); \
    header->b = (char*)WebserverMalloc( len + 1 ); \
    Webserver_strncpy((char*)header->b,len+1,(char*)&line[h_len],len); \
    return 0; \
}

int analyseHeaderLine(socket_info* sock, char *line, unsigned int length, HttpRequestHeader *header) {
	unsigned long len;
	SIZE_TYPE h_len;
	int pos;
	char* c_pos = 0;
#ifdef _WEBSERVER_HEADER_DEBUG_
	LOG(HEADER_PARSER_LOG,NOTICE_LEVEL,0,"Header Line : %s",line);
#endif


	if ( header->method == 0 ){

		if ( length < 12 ){
			header->error = 1;
			return -1;
		}

		if(0 == strcmp ( &line[length - 9 ], " HTTP/1.1" ) ){
			sock->header->isHttp1_1 = 1;
		}else{
			if ( 0 != strcmp ( &line[length - 9 ], " HTTP/1.0" )){
				header->error = 1;
				return -1;
			}
		}

		c_pos = &line[length - 9 ];

		//printf("%p %p  %d  %d\n",line,c_pos, ( line + length - c_pos ),  length);fflush(stdout);
	}

	if ( ( header->method == 0 ) && (!strncmp( line, "GET ", 4)) ) {
		header->method = HTTP_GET;
		header->no_websocket_support = 0;
#ifndef WEBSERVER_USE_WEBSOCKETS
		if( memcmp( &line[4], "ws://", 5 ) == 0 ){
			header->no_websocket_support = 1;
			return -1;
		}
		if( memcmp( &line[4], "wss://", 6 ) == 0 ){
			header->no_websocket_support = 1;
			return -1;
		}
#endif

		char* qmark = strchr(&line[5], '?');
		if (qmark == NULL) /* keine parameter */
		{
			pos = stringfind(&line[5], " ");
			header->url = (char *) WebserverMalloc( pos + 1 );
			Webserver_strncpy( header->url, pos + 1, &line[5], pos);
		} else /* mit parametern */
		{
			*c_pos = '\0';

			pos = qmark - &line[5];
			header->url = (char *) WebserverMalloc( pos + 1 );
			Webserver_strncpy( header->url, pos + 1, &line[5], pos);

			char* start = qmark + 1;
			int l = c_pos - start;
			recieveParameterFromGet(start , header, l);

			*c_pos = ' ';
		}

		url_sanity_check( header );

		if ( header->error == 1 ){
			return -1;
		}


#if _WEBSERVER_CONNECTION_DEBUG_ > 3
		LOG(CONNECTION_LOG,NOTICE_LEVEL,sock->socket,"%s",line);
#endif

		return 0;
	}

	if ( ( header->method == 0 ) && (!strncmp(line,"POST ",5)) ){


#if _WEBSERVER_CONNECTION_DEBUG_ > 3
		LOG(CONNECTION_LOG,NOTICE_LEVEL,sock->socket,"%s",line);
#endif

		header->method=HTTP_POST;

		char* qmark_post = strchr(&line[6], '?');
		if (qmark_post == NULL) /* keine parameter */
		{
			pos = stringfind(&line[6], " ");
			header->url = (char *) WebserverMalloc( pos + 1 );
			Webserver_strncpy( header->url, pos + 1, &line[6], pos);
		} else /* mit parametern */
		{
			*c_pos = '\0';

			pos = qmark_post - &line[6];
			header->url = (char *) WebserverMalloc( pos + 1 );
			Webserver_strncpy( header->url, pos + 1, &line[6], pos);

			char* start = qmark_post + 1;
			int l = c_pos - start;

			recieveParameterFromGet(start , header, l);

			*c_pos = ' ';
		}

		url_sanity_check( header );

		if ( header->error == 1 ){
			return -1;
		}

#ifdef ENABLE_DEVEL_WARNINGS
		#warning "Noch genauer Testen"
#endif

		return 0;
	}

	if ( ( header->method == 0 ) && (!strncmp( line, "OPTIONS ", 8)) ) {
		header->method = HTTP_OPTIONS;

		char* qmark_opt = strchr(&line[9], '?');
		if (qmark_opt == NULL) /* keine parameter */
		{
			pos = stringfind(&line[9], " ");
			header->url = (char *) WebserverMalloc( pos + 1 );
			Webserver_strncpy( header->url, pos + 1, &line[9], pos);
		} else /* mit parametern */
		{
			*c_pos = '\0';

			pos = qmark_opt - &line[9];
			header->url = (char *) WebserverMalloc( pos + 1 );
			Webserver_strncpy( header->url, pos + 1,  &line[9], pos);

			char* start = qmark_opt + 1;
			int l = c_pos - start;
			recieveParameterFromGet(start , header, l);

			*c_pos = ' ';
		}
		//printf("%s\n",header->url);

		url_sanity_check( header );

		if ( header->error == 1 ){
			return -1;
		}


#if _WEBSERVER_CONNECTION_DEBUG_ > 3
		LOG(CONNECTION_LOG,NOTICE_LEVEL,sock->socket,"%s",line);
#endif

		return 0;
	}

	if ( header->method == 0 ){
		for ( unsigned int i = 0 ; i < length; i++ ){
			if ( line[i] == ' ' ){
				int max = i;
				if ( max > ( MAX_ERROR_METHOD -1 )  )
					max = MAX_ERROR_METHOD - 1 ;

			    memset( header->error_method, 0 , MAX_ERROR_METHOD );
				memcpy( header->error_method, line, max);
				header->error_method[max] = '\0';
				break;
			}
		}
		header->error = 1;
		return -1;
	}



	/* noch nicht verarbeitete header lines */
	if (!strncasecmp( line, "Accept-Language: ", 17)) {
		return 0;
	}

	if (!strncasecmp( line, "Accept-Charset: ", 16)) {
		return 0;
	}

	if (!strncasecmp( line, "Keep-Alive: ", 12)) {
		return 0;
	}

	if (!strncasecmp( line, "Cache-Control: ", 14)) {
		return 0;
	}

	if (!strncasecmp( line, "Cookie: ", 8)) {
		parseCookies(line, length, header);
		return 0;
	}

	if ( (!strncasecmp( line, "Content-Length: ", 16)) && ( strlen( &line[15]  ) > 1  ) ) {
		const char* val = &line[16];
		/* Reject negative values and non-numeric input */
		if (*val == '-' || *val < '0' || *val > '9') {
			header->contentlenght = 0;
		} else {
			/* Note: strtoull overflow returns ULLONG_MAX which is safely rejected
			 * by the max_post_size check in check_post_header() */
			char* endptr;
			header->contentlenght = strtoull(val, &endptr, 10);
			/* Reject trailing garbage after number (HTTP Request Smuggling prevention) */
			if (*endptr != '\0' && *endptr != '\r' && *endptr != '\n' && *endptr != ' ') {
				header->contentlenght = 0;
				header->error = 1;
			}
		}
		return 0;
	}


	h_len = strlen("Host: ");
	if (!strncasecmp( line,"Host: ",h_len)){
		len = length - h_len;
		if ( header->Host != 0){
			WebserverFree(header->Host);
		}

		if ( header->HostName != 0){
			WebserverFree(header->HostName);
		}

		header->Host = (char*)WebserverMalloc( len + 1 );
		Webserver_strncpy( header->Host,len+1, &line[h_len],len);

		char* doppelpunkt = strstr(line + h_len, ":");
		if( doppelpunkt ) {

			len = doppelpunkt - ( line + h_len );

			header->HostName = (char*)WebserverMalloc( len + 1 );
			Webserver_strncpy( header->HostName,len+1, &line[h_len],len);
		}else{
			header->HostName = (char*)WebserverMalloc( len + 1 );
			Webserver_strncpy( header->HostName,len+1, &line[h_len],len);
		}

		//printf("HostName : %s ( %d )  \n",header->HostName);


		return 0;
	}

	CHECK_HEADER_LINE("Access-Control-Request-Method: ",Access_Control_Request_Method )
	CHECK_HEADER_LINE("Access-Control-Request-Headers: ",Access_Control_Request_Headers )
	
	CHECK_HEADER_LINE("Accept-Encoding: ",Accept_Encoding )

	CHECK_HEADER_LINE("If-Modified-Since: ", If_Modified_Since) /* Wed, 12 Dec 2007 13:13:08 GMT */

	CHECK_HEADER_LINE("If-None-Match: ", etag)

	CHECK_HEADER_LINE("Upgrade: ", Upgrade)

	CHECK_HEADER_LINE("Connection: ", Connection)

	/*CHECK_HEADER_LINE("Host: ", Host)*/

	CHECK_HEADER_LINE("Origin: ", Origin)
	
	CHECK_HEADER_LINE("Accept: ", Accept)
	
	CHECK_HEADER_LINE("User-Agent: ", UserAgent)
	
	CHECK_HEADER_LINE("Authorization: ", Authorization)
	
	CHECK_HEADER_LINE("Referer: ", Referer)



#ifdef WEBSERVER_USE_WEBSOCKETS

	CHECK_HEADER_LINE("Sec-WebSocket-Key: ",SecWebSocketKey)

	CHECK_HEADER_LINE("Sec-WebSocket-Protocol: ",SecWebSocketProtocol)

	/* CHECK_HEADER_LINE("Sec-WebSocket-Version: ",SecWebSocketVersion) */

	h_len = strlen("Sec-WebSocket-Version: ");
	if (!strncmp( line,"Sec-WebSocket-Version: ",h_len))
	{
		//len = length - h_len; // // clang Dead store
		sscanf(&line[h_len],"%d",&header->SecWebSocketVersion);
		return 0;
	}
#endif

	
	if ( header_attr_compare("Content-Type: ",strlen("Content-Type: "), line , length ) ){
		if(stringfind(&line[14],"multipart/form-data") > 0){
			int i2=stringfind(line,"boundary=");
			header->contenttype=MULTIPART_FORM_DATA;

			i2++;
			unsigned int i;
			for( i=i2;i<length;i++){
				if(line[i]=='\r'){
					break;
				}
			}

			header->boundary=(char*)WebserverMalloc( ( i - i2 )  + 3 ); /* + 2 für -- , +1 \0 */
			memcpy(header->boundary,"--",2);		/* nach http://www.w3.org/Protocols/rfc1341/7_2_Multipart.html */
			memcpy(header->boundary+2,&line[i2],i-i2);	/* kommt vor die boundary -- */
			header->boundary[i-i2+2]='\0';
#ifdef ENABLE_DEVEL_WARNINGS
			#warning Noch mehr Fehlerprüfungen
#endif
			return 0;
		}

		if(!strncmp(&line[14],"application/x-www-form-urlencoded",strlen("application/x-www-form-urlencoded"))){
			header->contenttype=APPLICATION_X_WWW_FORM_URLENCODED;
			return 0;
		}
	}

	return 0;

}

/*
 *
 *	Return Values
 *
 *		>0  = Header nicht zuende weiter Daten lesen
 * 		 0  = Fehler beim Parsen
 * 
 *		-6  = Keine Zeile erkannt weiter Daten lesen
 *		-4  = Methode nicht erlaubt
 *		-3  = Header zuende aber noch weitere daten vorhanden
 * 		-2  = Header ist zuende und keine weiteren Daten vorhanden
 * 		-7  = Reverse Proxy Handler
 *
 *
 *
 */

int ParseHeader(socket_info* sock, HttpRequestHeader* header, char* buffer, unsigned int length, unsigned int* bytes_parsed) {
	unsigned int i = 0;
	unsigned int last_line_end = 0;
	char* pos = buffer;
	char back;
	unsigned int line_length;

	*bytes_parsed = 0;

	if (length < 2){
		return -6;
	}



	// Wenn das im Buffer steht muss der Header zuende sein
	// Wenn eine Header Zeile verarbeitet wurde wird das \r\n am ende mit abgeschnitten,
	// also bleibt am Ende des Headers nur \r\n über
	if ((buffer[0] == '\r') && (buffer[1] == '\n')){
		*bytes_parsed = 2;

		// Das ist ein Sonderfall wenn nur \r\n hinereinander weg gesendet wird, in dem Fall das \r\n aus dem Buffer entfernen und nichts tun
		if ( header->parsed_bytes == 0 ){
			if ( length > 2 ){
				/* es sind weiterer daten im datenstrom vorhanden */
				return -3;
			}
		}


		header->parsed_bytes += *bytes_parsed;

		header->header_complete = 1;

		if ( length > 2 ){
			/* es sind weiterer daten im datenstrom vorhanden */
			return -3;
		}

		return -2;
	}

	for (i = 3; i < length; i++) {

		if ( i >= ( WEBSERVER_MAX_HEADER_LINE_LENGHT - 1 ) ) {
			return 0;
		}

		 /* Am Ende der Zeile steht bei http immer \r\n */
		if ((buffer[i] == '\n') && (buffer[i - 1] == '\r')){

			back = buffer[i - 1];
			
			if ( checkReverseProxy( sock, buffer, length ) == 0 ){
				return -7;
			}
			
			buffer[i - 1] = '\0';
			line_length = &buffer[i - 1] - pos;
			if ( analyseHeaderLine(sock, pos, line_length, header) < 0 ){
				buffer[i - 1] = back;
				return -4;
			}

			pos = &buffer[i + 1];
			last_line_end = i + 1;
			buffer[i - 1] = back;
			*bytes_parsed = last_line_end;

			header->parsed_bytes += *bytes_parsed;

			return last_line_end;
		}

		/* Ende des HTTP Headers ( \r\n\r\n ) */
		if ((buffer[i - 3] == '\r') && (buffer[i - 2] == '\n') && (buffer[i - 1] == '\r') && (buffer[i - 0] == '\n')) {

			header->header_complete = 1;
			
			*bytes_parsed = i;
			header->parsed_bytes += *bytes_parsed;
			/* es ist ein weiterer header im datenstrom gewesen */
			if ( (i + 1 ) < length){
				return -3;
			}
			return -2;

		}

	}

	return -6;
}

void clearHeader(http_request *s) {
	s->header->url = 0;
	s->header->method = 0;
	s->header->gzip = 0;
	s->header->deflate = 0;
	s->header->contentlenght = 0;
	s->header->contenttype = 0;
	clearVariables(s->header->parameter_store);
	ws_list_clear(&s->header->cookie_list);
}

ws_variable* getParameter(http_request* s, const char* name) {
	return getVariable(s->header->parameter_store, name);
}

bool checkHeaderComplete(HttpRequestHeader* header) {
	if (header->url == 0){
		return false;
	}

	return true;
}

