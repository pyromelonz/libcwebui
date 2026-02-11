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

#include "red_black_tree.h"


#ifdef DMALLOC
#include <dmalloc/dmalloc.h>
#endif

static list_t sock_list;
static rb_red_blk_tree *sock_tree;
static WS_MUTEX socks_mutex;

static void SockPrint(const void* a) {
	printf("%i", *(int*) a);
}
static void SockInfoPrint( UNUSED_PARA void* a) {
}
#if 0
static void SockInfoDest( UNUSED_PARA void *a) {
}
#endif
static void SockDest( UNUSED_PARA void* a) {
}

static int SockComp(const void* a, const void* b) {
	int ret;
	char *p_a = (char*) a;
	char *p_b = (char*) b;
	ret = strcmp(p_a, p_b);
	if (ret < 0){
		return -1;
	}
	if (ret > 0){
		return 1;
	}
	return 0;
}

void initSocketContainer(void) {
	PlatformCreateMutex(&socks_mutex);
	sock_tree = RBTreeCreate(SockComp, SockDest, SockDest, SockPrint, SockInfoPrint);
	ws_list_init(&sock_list);
}

void freeSocketContainer(void) {
	deleteAllSockets();
	ws_list_destroy(&sock_list);
	if ( sock_tree != 0 ){
		RBTreeDestroy(sock_tree);
	}
}

void deleteSocket(socket_info* sock) {
	
	PlatformLockMutex(&socks_mutex);

	PlatformLockMutex(&sock->socket_mutex);
#ifdef ENABLE_DEVEL_WARNINGS
	#warning mal tracen wo das aufgerufen wird
#endif

	ws_list_delete(&sock_list, sock);
#ifdef WEBSERVER_USE_WEBSOCKETS
	if (sock->isWebsocket == 1) {
		rb_red_blk_node* node;
		node = RBExactQuery(sock_tree, sock->websocket_guid);
		if ( node != 0 ){
			RBDelete(sock_tree, node);
		}else{
			printf("Error: websocket giud %s nicht gefunden\n",sock->websocket_guid);
		}
	}
#endif

	PlatformUnlockMutex(&sock->socket_mutex);

	PlatformUnlockMutex(&socks_mutex);
}

void addSocketContainer(socket_info* sock) {
	PlatformLockMutex(&socks_mutex);
	ws_list_append(&sock_list, sock,0);
	PlatformUnlockMutex(&socks_mutex);
}

#ifdef WEBSERVER_USE_WEBSOCKETS

void addSocketByGUID(socket_info* sock) {
	PlatformLockMutex(&socks_mutex);
	if (sock->isWebsocket == 1){
		RBTreeInsert(sock_tree, sock->websocket_guid, sock);
	}
	PlatformUnlockMutex(&socks_mutex);
}

socket_info* getSocketByGUID(const char* guid) {
	rb_red_blk_node* node;
	socket_info* sock = 0;

	PlatformLockMutex(&socks_mutex);

	node = RBExactQuery(sock_tree, (char*) guid);
	if (node != 0) {
		sock = (socket_info*) node->info;
		PlatformLockMutex(&sock->socket_mutex);
	}

	PlatformUnlockMutex(&socks_mutex);

	return sock;
}

#endif

void deleteAllSockets(void) {
	socket_info* sock;
	list_t del_list;

	ws_list_init(&del_list);

	PlatformLockMutex(&socks_mutex);

	ws_list_iterator_start(&sock_list);
	while (ws_list_iterator_hasnext(&sock_list)) {
		sock = (socket_info*) ws_list_iterator_next(&sock_list);
		PlatformShutdown(sock->socket);
		ws_list_append(&del_list, sock,0);
	}
	ws_list_iterator_stop(&sock_list);

	PlatformUnlockMutex(&socks_mutex);

	ws_list_iterator_start(&del_list);
	while (ws_list_iterator_hasnext(&del_list)) {
		sock = (socket_info*) ws_list_iterator_next(&del_list);
		WebserverConnectionManagerCloseRequest(sock);
	}
	ws_list_iterator_stop(&del_list);

	ws_list_destroy(&del_list);

}

unsigned long dumpSocketsSize(int *count) {
	int i = 0;
	unsigned long size = 0;
	socket_info *sock;

	PlatformLockMutex(&socks_mutex);

	ws_list_iterator_start(&sock_list);
	while ((sock = (socket_info*) ws_list_iterator_next(&sock_list))) {
		size += getSocketInfoSize(sock);
		i++;
	}
	ws_list_iterator_stop(&sock_list);
	if (count != 0){
		*count = i;
	}

	PlatformUnlockMutex(&socks_mutex);
	return size;
}

void dumpSockets(http_request* s) {
	socket_info* sock;
	const char *type_str;
	const char *ssl_str;
	const char *close_str;
	const char *ws_str;

	PlatformLockMutex(&socks_mutex);
	ws_list_iterator_start(&sock_list);

	while ((sock = (socket_info*) ws_list_iterator_next(&sock_list))) {
		/* Determine socket type */
		if (sock->server == 1) {
			type_str = "Server";
		} else if (sock->client == 1) {
			type_str = sock->client_ip_str;
		} else {
			type_str = "-";
		}

		/* SSL status */
#ifdef WEBSERVER_USE_SSL
		ssl_str = (sock->use_ssl == 1) ? "yes" : "-";
#else
		ssl_str = "-";
#endif

		/* Close status */
		close_str = (sock->closeSocket == 1) ? "yes" : "-";

		/* WebSocket status */
#ifdef WEBSERVER_USE_WEBSOCKETS
		ws_str = (sock->isWebsocket == 1) ? "yes" : "-";
#else
		ws_str = "-";
#endif

		printHTMLChunk(s->socket,
			"<tr>"
				"<td>%s</td>"
				"<td>%d</td>"
				"<td>%"PRIu32"</td>"
				"<td>%s</td>"
				"<td>%s</td>"
				"<td>%s</td>"
			"</tr>",
			type_str, sock->port, getSocketInfoSize(sock),
			ssl_str, close_str, ws_str);
	}

	ws_list_iterator_stop(&sock_list);
	PlatformUnlockMutex(&socks_mutex);
}

