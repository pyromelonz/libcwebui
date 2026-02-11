/*

SPDX-License-Identifier: MPL-2.0

 libCWebUI
 Copyright (C) 2007 - 2019  Ramin Seyed-Moussavi

 Projekt URL : https://github.com/lordrasmus/libcwebui

 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at https://mozilla.org/MPL/2.0/. 

*/


#ifndef _WEBSOCKETS_H_
#define _WEBSOCKETS_H_

#include "webserver.h"

#ifdef WEBSERVER_USE_WEBSOCKETS


#define SHORT_WS_FRAMES_T 1

#ifdef SHORT_WS_FRAMES_T
	/*
          64 Bit Laenge muss auf arm auf 8 byte aligned sein
	  darum websocket frames auf 32 bit begrenzen
	*/
	#define WEBSOCK_LEN_T uint32_t
	#define WEBSOCK_LEN_ALLIGN
#else
	#define WEBSOCK_LEN_T uint64_t
	#ifdef _ARM_EABI_
		#define WEBSOCK_LEN_ALLIGN  __attribute__ ((aligned (8)))
	#else
		#define WEBSOCK_LEN_ALLIGN
	#endif
#endif

typedef struct {
	WEBSOCKET_SIGNALS signal;
	char* guid;
	char* url;
	unsigned char* msg;
	WEBSOCK_LEN_T len;
	char binary;
}websocket_queue_msg WEBSOCK_LEN_ALLIGN ;

#ifdef __cplusplus
extern "C" {
#endif



void initWebsocketApi(void);

websocket_queue_msg* create_websocket_input_queue_msg(WEBSOCKET_SIGNALS signal, const char* guid, const char* url, const WEBSOCK_LEN_T msg_length);
websocket_queue_msg* create_websocket_output_queue_msg(WEBSOCKET_SIGNALS signal, const char* guid, const WEBSOCK_LEN_T msg_length);

void insert_websocket_input_queue(websocket_queue_msg* msg);
void insert_websocket_output_queue(websocket_queue_msg* msg);

int sendWebsocketTextFrame(const char* guid, const char* in, const WEBSOCK_LEN_T length);
int sendWebsocketBinaryFrame(const char* guid, const unsigned char* in, const WEBSOCK_LEN_T length);


int checkIskWebsocketConnection(socket_info* sock);
int startWebsocketConnection(socket_info* sock);
void calcWebsocketSecKeys(socket_info* request);

char* getWebsocketStoreGUID(char* guid);
long getWebsocketStoreTimeout( char* guid );

int sendWebsocketFrameReal(const char* guid, const unsigned char* in, const WEBSOCK_LEN_T length);
int recFrame(socket_info *sock);
int closeWebsocket(char* guid);
void initWebsocketStructures(socket_info* sock);

void sendCloseFrame(socket_info *sock);
void sendCloseFrame2(const char* guid);


void websocket_event_handler(socket_info* sock, EVENT_TYPES event_type);

/* Internal streaming functions */
void initWebsocketStreaming(void);
void cleanupWebsocketStreaming(void);
websocket_stream_handler_entry* findWebsocketStreamHandler(const char* url);
int startWebsocketStream(socket_info *sock, uint64_t frame_length,
                         unsigned char opcode, unsigned char* mask,
                         websocket_stream_handler_entry* handler,
                         char fragmented);
int continueWebsocketStream(socket_info *sock);
void freeWebsocketStreamContext(struct websocket_stream_context* ctx);

#ifdef __cplusplus
}
#endif

#endif /* WEBSERVER_USE_WEBSOCKETS */

#endif
