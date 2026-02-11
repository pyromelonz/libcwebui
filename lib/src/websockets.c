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



/*
 Protokoll

 http://tools.ietf.org/html/draft-ietf-hybi-thewebsocketprotocol-16


 http://www.cs.uwaterloo.ca/~brecht/servers/openfiles.html
 http://www.hpl.hp.com/techreports/2000/HPL-2000-174.pdf
 http://www.kegel.com/c10k.html#top
 http://www.acme.com/software/thttpd/
 http://redmine.lighttpd.net/wiki/1/Docs:Performance
 http://jwebsocket.org/
*/

#ifdef WEBSERVER_USE_WEBSOCKETS

#ifndef WEBSOCKET_INIT_INBUFFER_SIZE
	#define WEBSOCKET_INIT_INBUFFER_SIZE 50
	#warning WEBSOCKET_INIT_INBUFFER_SIZE not defined, defaults to 50
#endif


WS_THREAD websocket_input_thread;
WS_THREAD websocket_output_thread;

static ws_MessageQueue* websocket_input_queue;
static ws_MessageQueue* websocket_output_queue;

static void websocket_input_thread_function( void ) {
	websocket_queue_msg* msg;
	while (1) {
		msg = (websocket_queue_msg*) ws_popQueue(websocket_input_queue);

		handleWebsocketConnection(msg->signal, msg->guid, msg->url, msg->binary, (char*)msg->msg, msg->len);

		WebserverFree(msg->guid);
		WebserverFree(msg->msg);
		WebserverFree(msg->url);
		WebserverFree(msg);
	}
	
}



websocket_queue_msg* create_websocket_input_queue_msg(WEBSOCKET_SIGNALS signal, const char* guid, const char* url, const WEBSOCK_LEN_T msg_length) {
	websocket_queue_msg* tmp = (websocket_queue_msg*) WebserverMalloc( sizeof(websocket_queue_msg) );

	tmp->signal = signal;

	tmp->msg = (unsigned char*) WebserverMalloc( msg_length );
	tmp->len = msg_length;

	tmp->guid = (char*) WebserverMalloc( strlen(guid) +1 );
	strcpy(tmp->guid, guid);

	tmp->url = (char*) WebserverMalloc( strlen( url) + 1 );
	strcpy(tmp->url, url);
	return tmp;
}

websocket_queue_msg* create_websocket_output_queue_msg(WEBSOCKET_SIGNALS signal, const char* guid, const WEBSOCK_LEN_T msg_length) {
	websocket_queue_msg* msg = (websocket_queue_msg*) WebserverMalloc( sizeof(websocket_queue_msg) );

	msg->signal = signal;

	msg->guid = (char*) WebserverMalloc( strlen(guid) + 1 );
	strcpy(msg->guid, guid);

	msg->msg = (unsigned char*) WebserverMalloc( msg_length );
	msg->len = msg_length;

	return msg;
}


void insert_websocket_input_queue(websocket_queue_msg* msg){
	ws_pushQueue(websocket_input_queue, (void*) msg);
}

void insert_websocket_output_queue(websocket_queue_msg* msg){
	ws_pushQueue(websocket_output_queue, (void*) msg);
}


static void websocket_output_thread_function( void ) {
	websocket_queue_msg* msg;
	while (1) {
		msg = (websocket_queue_msg*) ws_popQueue(websocket_output_queue);

		sendWebsocketFrameReal(msg->guid, msg->msg, msg->len);

		WebserverFree(msg->guid);
		WebserverFree(msg->msg);
		WebserverFree(msg);
	}
	
}

void initWebsocketApi(void) {
	websocket_input_queue = ws_createMessageQueue();
	websocket_output_queue = ws_createMessageQueue();
	PlatformCreateThread(&websocket_input_thread, websocket_input_thread_function);
	PlatformCreateThread(&websocket_output_thread, websocket_output_thread_function);
	initWebsocketStreaming();
}

void initWebsocketStructures(socket_info* sock) {
	sock->websocket_buffer = (unsigned char*) WebserverMalloc( WEBSOCKET_INIT_INBUFFER_SIZE );
	sock->websocket_guid = (char*) WebserverMalloc( WEBSERVER_GUID_LENGTH + 1);

	/* Initialize streaming state */
	sock->websocket_streaming_active = 0;
	sock->websocket_stream_fragmented = 0;
	sock->websocket_stream_remaining = 0;
	sock->websocket_stream_mask_offset = 0;
	sock->websocket_stream_ctx = NULL;
	sock->websocket_stream_handler = NULL;
}

int checkIskWebsocketConnection(socket_info* sock) {
	HttpRequestHeader* header;
		
	if (sock == 0) {
		return 0;
	}

	header = sock->header;

	if (header == 0) {
		return 0;
	}

	header->isWebsocket = 0;

	if (header->SecWebSocketKey == 0){
		return 0;
	}

	if (header->Connection == 0) {
		return 0;
	}

	if (header->Upgrade == 0) {
		return 0;
	}

	/* RFC 7230: Connection header is a comma-separated list of tokens.
	 * Check that "Upgrade" is a complete token, not a substring.
	 * Example: "keep-alive, Upgrade" or "Upgrade" */
	{
		int upgrade_found = 0;
		const char* p = header->Connection;
		while (*p) {
			/* Skip leading whitespace */
			while (*p == ' ' || *p == '\t') p++;
			/* Find end of token (comma or end of string) */
			const char* token_start = p;
			while (*p && *p != ',') p++;
			/* Calculate token length, trim trailing whitespace */
			size_t token_len = p - token_start;
			while (token_len > 0 && (token_start[token_len-1] == ' ' || token_start[token_len-1] == '\t')) {
				token_len--;
			}
			/* Check if token matches "Upgrade" (case-insensitive) */
			if (token_len == 7 &&
			    (token_start[0] == 'U' || token_start[0] == 'u') &&
			    (token_start[1] == 'p' || token_start[1] == 'P') &&
			    (token_start[2] == 'g' || token_start[2] == 'G') &&
			    (token_start[3] == 'r' || token_start[3] == 'R') &&
			    (token_start[4] == 'a' || token_start[4] == 'A') &&
			    (token_start[5] == 'd' || token_start[5] == 'D') &&
			    (token_start[6] == 'e' || token_start[6] == 'E')) {
				upgrade_found = 1;
				break;
			}
			/* Skip comma */
			if (*p == ',') p++;
		}
		if (!upgrade_found) {
			return 0;
		}
	}

	/* RFC 6455: Upgrade header value is case-insensitive */
	if (0 == strcasecmp(header->Upgrade, "websocket")) {

		if ( header->SecWebSocketVersion < 13 ){
			/* RFC 6455 Section 4.4: Send supported version(s) in response */
			printHeaderChunk(sock,"HTTP/1.1 426 Upgrade Required\r\n");
			printHeaderChunk(sock,"Sec-WebSocket-Version: 13\r\n");
			printHeaderChunk(sock,"\r\n");
			#ifdef _WEBSERVER_HEADER_DEBUG_
			LOG(HEADER_PARSER_LOG,NOTICE_LEVEL,sock->socket,"Websocket Handshake Error: Version ( %d ) is < 13",header->SecWebSocketVersion);
			#endif
			header->isWebsocket = 2; // falsche websocket version
			return 1;
		}

		header->isWebsocket = 1; // websocket ok
		return 1;
	}
	return 0;
}

int startWebsocketConnection(socket_info* sock) {
	char buffer[170];
	char buffer2[21];
	http_request *s;
	sock->active = 0;

	
	if (sock->header->SecWebSocketKey != 0) {
		strncpy( buffer, sock->header->SecWebSocketKey, 100 );
		strcat(buffer, "258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
		WebserverSHA1((unsigned char*) buffer, strlen(buffer), (unsigned char*) buffer2);
		WebserverBase64Encode((unsigned char*) buffer2, 20, sock->header->WebSocketOutHash, 40);
	}

	sendHeaderWebsocket(sock);

	s = (http_request*) WebserverMalloc( sizeof(http_request) );
	memset(s, 0, sizeof(http_request));

	s->socket = sock;
	s->header = sock->header;
	sock->websocket_buffer_offset = 0;
	sock->s = s;

	generateGUID(sock->websocket_guid, WEBSERVER_GUID_LENGTH);
	addSocketByGUID(sock);

	checkSessionCookie(s);
	restoreSession(s,0, 1 );

	if ( s->store != 0 ){
		sock->websocket_store_guid = WebserverMalloc( WEBSERVER_GUID_LENGTH + 1 );
		memset(sock->websocket_store_guid, 0, WEBSERVER_GUID_LENGTH + 1 );
		memcpy( sock->websocket_store_guid, s->store->guid, WEBSERVER_GUID_LENGTH );
		//printf("copy Store GUID : %s\n", sock->websocket_store_guid);
	}

	if (handleWebsocketConnection(WEBSOCKET_SIGNAL_CONNECT, sock->websocket_guid, sock->header->url, 0 ,0 , 0) < 0) {
		printf("\nWebsocket connection error\n\n");
		sendCloseFrame(sock);
		sock->closeSocket = 1;
	}

	return 0;
}




char* getWebsocketStoreGUID(char* guid) {
	socket_info *sock = getSocketByGUID(guid); /* Locked */
	http_request *s;
	char* ret = 0;
	if (sock == 0){
		return 0;
	}
	if (sock->closeSocket == 1){
		PlatformUnlockMutex(&sock->socket_mutex);
		return 0;
#ifdef ENABLE_DEVEL_WARNINGS
		#warning mal tracen ( unlock ist neu )
#endif
	}

	if (sock->s != 0) {
		s = (http_request*) sock->s;
		if (s->store != 0){
			ret = s->store->guid;
		}
	}

	PlatformUnlockMutex(&sock->socket_mutex);

	return ret;
}

long getWebsocketStoreTimeout ( char* guid ){
	socket_info *sock = getSocketByGUID(guid);
	if (sock == 0){
		return -1;
	}

	if (sock->closeSocket == 1) {
		PlatformUnlockMutex(&sock->socket_mutex);
		return -1;
	}

	char store_guid[ WEBSERVER_GUID_LENGTH + 1 ];
	strncpy( store_guid , sock->websocket_store_guid,WEBSERVER_GUID_LENGTH );
	store_guid[WEBSERVER_GUID_LENGTH] = '\0';

	//printf("Store GUID : %s WS : %s\n", store_guid, guid );

	PlatformUnlockMutex(&sock->socket_mutex);

	long tmp = getSessionTimeoutByGUID( store_guid , SESSION_STORE );
	if ( tmp < 0 ){
		return -1;
	}

	long session_timeout = getConfigInt("session_timeout");
	/* Check before subtraction to avoid potential overflow */
	if ( tmp > session_timeout ){
		return -1;
	}
	long timeout = session_timeout - tmp;

	/*http_request *s = sock->s;
	if ( s == 0 ) return ULONG_MAX;

	sessionStore *ss = (sessionStore*)s->store;
	if ( ss == 0 ) return ULONG_MAX;

	unsigned long diff = PlatformGetTick() - ss->last_use;



	unsigned long timeout = getConfigInt("session_timeout") - diff;*/



	return timeout;
}

int closeWebsocket(char* guid) {
	socket_info *sock = getSocketByGUID(guid); /* Locked */
	if (sock == 0){
		return 0;
	}
	if (sock != 0){
		sock->closeSocket = 1;
	}

	PlatformUnlockMutex(&sock->socket_mutex);
#ifdef ENABLE_DEVEL_WARNINGS
	#warning mal tracen ( unlock ist neu )
#endif

	return 0;
}

#endif

