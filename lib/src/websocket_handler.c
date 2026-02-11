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

#ifdef __CDT_PARSER__
	#define __BASE_FILE__
#endif


#ifdef DMALLOC
#include <dmalloc/dmalloc.h>
#endif

#ifdef WEBSERVER_USE_WEBSOCKETS

static int handleWebsocket(socket_info* sock, EVENT_TYPES type) {
	html_chunk* chunk;
	int send_bytes;
	int to_send;
	SOCKET_SEND_STATUS status;



	switch (type) {

		case EVENT_TIMEOUT:
			LOG( WEBSOCKET_LOG, ERROR_LEVEL, 0, "%s","Event Type EVENT_TIMEOUT not handled");
			return -1;
		case EVENT_SIGNAL:
			LOG( WEBSOCKET_LOG, ERROR_LEVEL, 0,"%s", "Event Type EVENT_SIGNAL not handled");
			return -1;
		case EVENT_PERSIST:
			LOG( WEBSOCKET_LOG, ERROR_LEVEL, 0, "%s","Event Type EVENT_PERSIST not handled");
			return -1;

		case EVENT_READ: /* READ */
			/*
			  Read muss nicht gelockt werden da nicht auf die websocket_chunk_list zugegriffen wird
			  ausserdem liest nur der Hauptthread vom socket
			*/
			#if _WEBSERVER_WEBSOCKET_DEBUG_ > 4
				LOG( WEBSOCKET_LOG, ERROR_LEVEL, 0, "Event Type EVENT_READ");
			#endif
			if (recFrame(sock) < 0) {
				return -1;
			}
			break;

		case EVENT_WRITE: /* WRITE */

			#if _WEBSERVER_WEBSOCKET_DEBUG_ > 4
				LOG( WEBSOCKET_LOG, ERROR_LEVEL, 0, "Event Type EVENT_WRITE");
			#endif

			PlatformLockMutex(&sock->socket_mutex);

			while (1) {

				if (unlikely(ws_list_empty(&sock->websocket_chunk_list) == 1)){
					break;
				}

				chunk = (html_chunk*) ws_list_get_at(&sock->websocket_chunk_list, 0);

				if (unlikely(chunk == 0)){
					break;
				}

				to_send = chunk->length - sock->websocket_send_pos;
				status = WebserverSend(sock, (unsigned char*) &chunk->text[sock->websocket_send_pos], to_send, 0, &send_bytes);

				if (likely (send_bytes == to_send) ) {
					sock->websocket_send_pos = 0;
					ws_list_delete(&sock->websocket_chunk_list, chunk);
					WebserverFreeHtml_chunk(chunk);
					if (likely ( status == SOCKET_SEND_NO_MORE_DATA ) ){
						continue;
					}
				}

				if ((status == SOCKET_SEND_UNKNOWN_ERROR) || (status == SOCKET_SEND_CLIENT_DISCONNECTED)) {
					PlatformUnlockMutex(&sock->socket_mutex);
					return -1;
				}

				/* Der Buffer konnte nur teilweise gesendet werden */
				if ((status == SOCKET_SEND_NO_MORE_DATA) && (send_bytes < to_send)) {
					sock->websocket_send_pos += send_bytes;
					break;
				}
				
				if ( status == SSL_PROTOCOL_ERROR ){
					PlatformUnlockMutex(&sock->socket_mutex);
					return -1;
				}				
				
				LOG( WEBSOCKET_LOG, ERROR_LEVEL, sock->socket, "Darf hier niemals ankommen  status: %d  send_bytes: %d  to_send: %d", status,send_bytes,to_send);
				PlatformUnlockMutex(&sock->socket_mutex);
				return -1;
			}

			if (ws_list_empty(&sock->websocket_chunk_list) == 1) {
				delEventSocketAll(sock);
				addEventSocketReadPersist(sock);
			}

			PlatformUnlockMutex(&sock->socket_mutex);
			break;
	}

	return 0;
}

void websocket_event_handler(socket_info* sock, EVENT_TYPES event_type) {
	int ret;
	PlatformLockMutex(&sock->socket_mutex);
	ret = ws_list_empty(&sock->websocket_chunk_list);
	PlatformUnlockMutex(&sock->socket_mutex);

	/* Socket nur schliessen wenn alle Daten gesendet wurden */
	if ((sock->closeSocket == 1) && (ret == 1)) {
		WebserverConnectionManagerCloseRequest(sock);
		return;
	}

	ret = handleWebsocket(sock, event_type);

	if (ret < 0) {
		WebserverConnectionManagerCloseRequest(sock);
		return;
	}
}

#endif
