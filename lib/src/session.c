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

#include "stack.h"


#ifdef DMALLOC
#include <dmalloc/dmalloc.h>
#endif

/* * Session Managment *
// http://de.wikipedia.org/wiki/Session-ID
// Formulare mit versteckten Feldern
// Status in der URI
// HTTP Cookies  -  umgesetzt

// http://www.erich-kachel.de/?p=368  ->  Session-Angriffe - eine Analyse an PHP

// * Web Based Session Management
// * Best practices in managing HTTP-based client sessions
// http://www.technicalinfo.net/papers/WebBasedSessionManagement.html

// * generierung von session-ids *
// http://www.scip.ch/?labs.20101013

// * optimierungen *
// beim erzeugen eines session stores für eine session immer eine neue session-id generieren ( session fixation )
*/

static rb_red_blk_tree* session_store_tree;
static WS_MUTEX session_mutex;
static unsigned long last_timeout_check = 0;


static void SessionFreerFunc(void* a){
	sessionStore* ss=(sessionStore*)a;
	WebserverFreeSessionStore(ss);
}



static ws_variable* int_getSessionValue(http_request* s, STORE_TYPES store, const char* name);
static char int_setSessionValue(http_request* s, STORE_TYPES store, const char* name, const char* value);
static ws_variable* int_addSessionValue(http_request* s, STORE_TYPES store, const char* name );

void initSessions(void) {
	session_store_tree = RBTreeCreate(StrKeyComp, DummyFunc, SessionFreerFunc, DummyFuncConst, DummyFunc);

	PlatformCreateMutex( &session_mutex );
}

void freeSessions(void){
	RBTreeDestroy(session_store_tree);
}

#if 0
static void DummFreeFunc( UNUSED_PARA void * a){

}
#endif

static void checkSessionTimeout(void) {
	sessionStore* ss;
	unsigned long diff;
	unsigned int deleted = 0;
	stk_stack* stack;
	rb_red_blk_node* node;
	list_t del_list;

	/* nur alle 10 sekunden auf timeouts testen */
	diff = PlatformGetTick() - last_timeout_check;
	if (diff < (10 * PlatformGetTicksPerSeconde())){
		return;
	}

	last_timeout_check = PlatformGetTick();

	PlatformLockMutex( &session_mutex );

	ws_list_init(&del_list);
	stack = RBEnumerate(session_store_tree, (void*)"0", (void*)"z");

	while (0 != StackNotEmpty(stack)) {
		int timeout;

		node = (rb_red_blk_node*) StackPop(stack);
		ss = (sessionStore*) node->info;
		diff = PlatformGetTick() - ss->last_use;

		timeout = getConfigInt("session_timeout");

		if ( ( timeout != 0 ) && ( diff > timeout * PlatformGetTicksPerSeconde() ) ) {
#ifdef _WEBSERVER_SESSION_DEBUG_
			LOG(SESSION_LOG, DEBUG_LEVEL, 0, "Delete SessionStore GUID : %s", ss->guid);
#endif
			ws_list_append(&del_list, node, 0);
			deleted++;
		}
	}
	free(stack);

	if (ws_list_size(&del_list) > 0) {
		ws_list_iterator_start(&del_list);
		while ((node = (rb_red_blk_node*) ws_list_iterator_next(&del_list))) {

			/*
			 Freigeben der Elemente ist in der RBTree Freer Function eingetragen ( SessionFreerFunc )
			*/
			RBDelete(session_store_tree, node);
		}
		ws_list_iterator_stop(&del_list);
	}
	ws_list_destroy(&del_list);

	PlatformUnlockMutex( &session_mutex );

#ifdef _WEBSERVER_SESSION_DEBUG_
	if (deleted > 0){
		LOG(SESSION_LOG, DEBUG_LEVEL, 0, "Deleted Sessions %d", deleted);
	}
#endif

}

/* Timing-safe comparison to prevent timing attacks on session IDs.
 * Compares actual string length (UUID is 36 chars, not WEBSERVER_GUID_LENGTH buffer size). */
static char guid_cmp(const char* a, const char* b) {
	size_t len_a, len_b, i;
	unsigned char result = 0;

	if (a[0] == 0){
		return 0;
	}

	len_a = strlen(a);
	len_b = strlen(b);

	/* Length mismatch */
	if (len_a != len_b) {
		return 0;
	}

	/* XOR all bytes - any difference sets bits in result */
	for (i = 0; i < len_a; i++) {
		result |= (unsigned char)(a[i] ^ b[i]);
	}

	/* result is 0 only if all bytes matched */
	return (result == 0) ? 1 : 0;
}

int checkUserRegistered(http_request* s) {

	bool true_normal, got_giud;

	ws_variable* var;
	ws_variable* guid_var;

#ifdef WEBSERVER_USE_SSL

	bool true_ssl, got_normal, got_guid_ssl, true_both, guid_match;

	ws_variable* guid_var_ssl;

	true_ssl = false;
	true_both = false;
	got_guid_ssl = false;
	guid_match = false;
	got_normal = false;   

#endif

	true_normal = false;
	got_giud = false;

	PlatformLockMutex( &session_mutex );

	var = int_getSessionValue(s, SESSION_STORE, (char*) "registered");
	if (var != 0) {
#ifdef WEBSERVER_USE_SSL
		got_normal = true;
#endif
		if (var->type == VAR_TYPE_STRING) {
			if (0 == strcmp(var->val.value_string, "true")) {
				true_normal = true;
			} else {
				PlatformUnlockMutex( &session_mutex );
				return NOT_REGISTERED;
			}
		} else {
			PlatformUnlockMutex( &session_mutex );
			return NOT_REGISTERED;
		}
	} else {
		PlatformUnlockMutex( &session_mutex );
		return NOT_REGISTERED;
	}

#ifdef WEBSERVER_USE_SSL
	if (s->socket->use_ssl == 1) {
		var = int_getSessionValue(s, SESSION_STORE_SSL, (char*) "registered");
		if (var != 0) {
			if (var->type == VAR_TYPE_STRING) {
				if (0 == strcmp(var->val.value_string, "true")) {
					true_ssl = true;
					if (true_normal){
						true_both = true;
					}
				}
			}
		}
	}
#endif

	guid_var = int_getSessionValue(s, SESSION_STORE, (char*) "session-id");
	if (guid_var != 0) {
		got_giud = true;
	} else {
		PlatformUnlockMutex( &session_mutex );
		return NOT_REGISTERED;
	}

#ifdef WEBSERVER_USE_SSL
	if (s->socket->use_ssl == 1) {
		guid_var_ssl = int_getSessionValue(s, SESSION_STORE_SSL, (char*) "session-id-ssl");
		if (0 != guid_var_ssl) {
			got_guid_ssl = true;
#ifdef _WEBSERVER_SESSION_DEBUG_
			LOG(SESSION_LOG, DEBUG_LEVEL, s->socket->socket, "guid_var->val.value_string     : %s", guid_var->val.value_string);
			LOG(SESSION_LOG, DEBUG_LEVEL, s->socket->socket, "guid_var_ssl->val.value_string : %s", guid_var_ssl->val.value_string);
#endif
			if (guid_cmp(guid_var->val.value_string, guid_var_ssl->val.value_string)) {
				guid_match = true;
				if (guid_match && true_both){
					PlatformUnlockMutex( &session_mutex );
					return SSL_CHECK_OK;
				}
			}
		}
	}

	PlatformUnlockMutex( &session_mutex );

	if (s->socket->use_ssl == 1) {
#ifdef _WEBSERVER_SESSION_DEBUG_
		LOG(SESSION_LOG, DEBUG_LEVEL, s->socket->socket, "checkUserRegistered SSL: got_guid_ssl=%d got_normal=%d got_giud=%d true_both=%d true_normal=%d true_ssl=%d guid_match=%d",
			got_guid_ssl, got_normal, got_giud, true_both, true_normal, true_ssl, guid_match);
#endif

		if (got_guid_ssl != got_giud){
#ifdef _WEBSERVER_SESSION_DEBUG_
			LOG(SESSION_LOG, DEBUG_LEVEL, s->socket->socket, "checkUserRegistered: SESSION_MISMATCH_ERROR (got_guid_ssl != got_giud)");
#endif
			return SESSION_MISMATCH_ERROR;
		}

		if (guid_match != true_both){
#ifdef _WEBSERVER_SESSION_DEBUG_
			LOG(SESSION_LOG, DEBUG_LEVEL, s->socket->socket, "checkUserRegistered: SESSION_MISMATCH_ERROR (guid_match != true_both)");
#endif
			return SESSION_MISMATCH_ERROR;
		}

		if (got_guid_ssl && got_normal && (true_normal != true_ssl)){
#ifdef _WEBSERVER_SESSION_DEBUG_
			LOG(SESSION_LOG, DEBUG_LEVEL, s->socket->socket, "checkUserRegistered: SESSION_MISMATCH_ERROR (true_normal != true_ssl)");
#endif
			return SESSION_MISMATCH_ERROR;
		}
	}
#endif



	if (got_giud && true_normal){
#ifdef _WEBSERVER_SESSION_DEBUG_
		LOG(SESSION_LOG, DEBUG_LEVEL, s->socket->socket, "checkUserRegistered: NORMAL_CHECK_OK");
#endif
		return NORMAL_CHECK_OK;
	}

#ifdef _WEBSERVER_SESSION_DEBUG_
	LOG(SESSION_LOG, DEBUG_LEVEL, s->socket->socket, "checkUserRegistered: NOT_REGISTERED (got_giud=%d true_normal=%d)", got_giud, true_normal);
#endif
	return NOT_REGISTERED;
}

char setUserRegistered(http_request* s, char status) {
	ws_variable* var;
	static char buf1[WEBSERVER_GUID_LENGTH + 1];



	if(	status==false) {
		if (NOT_REGISTERED!=checkUserRegistered(s)) {
			PlatformLockMutex( &session_mutex );
			memset(buf1,0,WEBSERVER_GUID_LENGTH);
			int_setSessionValue(s,SESSION_STORE,(char*)"session-id",buf1);
			int_setSessionValue(s,SESSION_STORE_SSL,(char*)"session-id-ssl",buf1);
			int_setSessionValue(s,SESSION_STORE,(char*)"registered",(char*)"false");
			int_setSessionValue(s,SESSION_STORE_SSL,(char*)"registered",(char*)"false");
			PlatformUnlockMutex( &session_mutex );
		}

		return true;
	}

	if (status == true) {
		if (SSL_CHECK_OK == checkUserRegistered(s)) {
			return true;
		} else {
			generateGUID(buf1, WEBSERVER_GUID_LENGTH);
#ifdef _WEBSERVER_SESSION_DEBUG_
			LOG(SESSION_LOG, DEBUG_LEVEL, s->socket->socket, "setUserRegistered: buf1 = %s", buf1);
			LOG(SESSION_LOG, DEBUG_LEVEL, s->socket->socket, "setUserRegistered: s->store = %p, s->store_ssl = %p", (void*)s->store, (void*)s->store_ssl);
#ifdef WEBSERVER_USE_SSL
			LOG(SESSION_LOG, DEBUG_LEVEL, s->socket->socket, "setUserRegistered: s->socket->use_ssl = %d", s->socket->use_ssl);
#else
			LOG(SESSION_LOG, DEBUG_LEVEL, s->socket->socket, "setUserRegistered: SSL disabled");
#endif
#endif
			PlatformLockMutex( &session_mutex );
			int_setSessionValue(s, SESSION_STORE, (char*) "session-id", buf1);
			int_setSessionValue(s, SESSION_STORE_SSL, (char*) "session-id-ssl", buf1);
			int_setSessionValue(s, SESSION_STORE, (char*) "registered", (char*) "true");
			int_setSessionValue(s, SESSION_STORE_SSL, (char*) "registered", (char*) "true");

			/* Zur sicherheit die Variablen inhalte nochmal pruefen */
			var = int_getSessionValue(s, SESSION_STORE, (char*) "session-id");
			if (var != 0) {
				if (guid_cmp(var->val.value_string, buf1)) {
					var = int_getSessionValue(s, SESSION_STORE_SSL, (char*) "session-id-ssl");
					if (var != 0) {
						if (guid_cmp(var->val.value_string, buf1)){
							PlatformUnlockMutex( &session_mutex );
							return true;
						}
					}
					PlatformUnlockMutex( &session_mutex );
					return false;
				}
			}

			PlatformUnlockMutex( &session_mutex );
		}
	}

	return false;
}

void createSession(http_request* s, unsigned char ssl_store) {
	sessionStore* ss;

	ss = WebserverMallocSessionStore();


	if (ssl_store == 0) {
		memcpy(ss->guid, s->guid, WEBSERVER_GUID_LENGTH); /* strlen((char*)s->guid)); */
#ifdef _WEBSERVER_SESSION_DEBUG_
			LOG(SESSION_LOG, DEBUG_LEVEL, s->socket->socket, "Create SessionStore GUID : %s", s->guid);
#endif
		ss->ssl = 0;
		s->store = ss;
	}
	if (ssl_store == 1) {
		memcpy(ss->guid, s->guid_ssl, WEBSERVER_GUID_LENGTH); /* strlen((char*)s->guid)); */
#ifdef _WEBSERVER_SESSION_DEBUG_
			LOG(SESSION_LOG, DEBUG_LEVEL, s->socket->socket, "Create SessionStore SSL GUID : %s", s->guid_ssl);
#endif
		ss->ssl = 1;
		s->store_ssl = ss;
	}

	ss->last_use = PlatformGetTick();

	PlatformLockMutex( &session_mutex );

	RBTreeInsert(session_store_tree, ss->guid, ss);

	PlatformUnlockMutex( &session_mutex );

}

static char findSessionStore(http_request* s, unsigned char ssl_store) {
	sessionStore* store;
	rb_red_blk_node* node;

	checkSessionTimeout();

	PlatformLockMutex( &session_mutex );

	if (ssl_store == 0) {
		node = RBExactQuery(session_store_tree, s->guid);
		if (node != 0) {
			store = (sessionStore*) node->info;
			if (store->ssl == 0) {
				s->store = store;
				store->last_use = PlatformGetTick();
				PlatformUnlockMutex( &session_mutex );
				return true;
			}
		}
	} else {
		node = RBExactQuery(session_store_tree, s->guid_ssl);
		if (node != 0) {
			store = (sessionStore*) node->info;
			if (store->ssl == 1) {
				s->store_ssl = store;
				store->last_use = PlatformGetTick();
				PlatformUnlockMutex( &session_mutex );
				return true;
			}
		}
	}

	PlatformUnlockMutex( &session_mutex );
	return false;
}

void restoreSession(http_request* s,int lock_stores, int create_session ) {
	checkSessionTimeout();
	if (s->store == 0){
		if (s->create_cookie == 0) {
			if (false == findSessionStore(s, 0)) {
				if ( create_session == 1){
					createSession(s, 0);
				}
			}
		} else {
			if ( create_session == 1){
				createSession(s, 0);
			}
		}
		if ( ( lock_stores ) &&  ( s->store != 0 ) ){
			lockStore(s->store->vars);
		}
	}

#ifdef WEBSERVER_USE_SSL
	if (s->store_ssl == 0){
		if (s->socket->use_ssl == 1) {
			if (s->create_cookie_ssl == 0) {
				if (false == findSessionStore(s, 1)) {
					if ( create_session == 1){
						createSession(s, 1);
					}
				}
			} else {
				if ( create_session == 1){
					createSession(s, 1);
				}
			}
			if ( ( lock_stores ) &&  ( s->store_ssl != 0 ) ){
				lockStore(s->store_ssl->vars);
			}
		}
	}
#endif

}

#if 0
static ws_variable* getSessionValueItem(sessionStore* ss, const char* name) {
	return getVariable(ss->vars, name);
}
#endif

char setSessionValueByGUID(char* store_guid, STORE_TYPES store, const char* name, const char* value) {
	sessionStore* ss;
	ws_variable* var = 0;
	rb_red_blk_node* node;

	PlatformLockMutex( &session_mutex );

	node = RBExactQuery(session_store_tree, store_guid);
	if (node != 0) {
		ss = (sessionStore*) node->info;
		if ((store == SESSION_STORE) && (ss->ssl == 0)) {
			var = newVariable(ss->vars, name, 0 );
			setWSVariableString(var, value);
			PlatformUnlockMutex( &session_mutex );
			return 1;
		}
		if ((store == SESSION_STORE_SSL) && (ss->ssl == 1)) {
			var = newVariable(ss->vars, name, 0 );
			setWSVariableString(var, value);
			PlatformUnlockMutex( &session_mutex );
			return 1;
		}
	}

	PlatformUnlockMutex( &session_mutex );

	return 0;
}

ws_variable* getSessionValueByGUID(char* store_guid, STORE_TYPES store, const char* name) {
	sessionStore* ss;
	rb_red_blk_node* node;
	ws_variable* ret = 0;

	PlatformLockMutex( &session_mutex );

	node = RBExactQuery(session_store_tree, store_guid);
	if (node != 0) {
		ss = (sessionStore*) node->info;
		if ((store == SESSION_STORE) && (ss->ssl == 0)) {
			ret = newVariable(ss->vars, name, 0 );

		}
		if ((store == SESSION_STORE_SSL) && (ss->ssl == 1)) {
			ret = newVariable(ss->vars, name, 0 );
		}
	}

	PlatformUnlockMutex( &session_mutex );

	return ret;
}

long getSessionTimeoutByGUID(char* store_guid, STORE_TYPES store ) {
	sessionStore* ss;
	rb_red_blk_node* node;

	PlatformLockMutex( &session_mutex );

	node = RBExactQuery(session_store_tree, store_guid);
	//printf("Session %p\n",node);
	if (node != 0) {
		ss = (sessionStore*) node->info;
		if ((store == SESSION_STORE) && (ss->ssl == 0)) {
			PlatformUnlockMutex( &session_mutex );
			return PlatformGetTick() - ss->last_use;

		}
		if ((store == SESSION_STORE_SSL) && (ss->ssl == 1)) {
			PlatformUnlockMutex( &session_mutex );
			return PlatformGetTick() - ss->last_use;
		}
	}
	PlatformUnlockMutex( &session_mutex );
	return -1;
}



char setSessionValue(http_request* s, STORE_TYPES store, const char* name, const char* value) {
	if (0 == strcmp("registered", name)){
		return false;
	}
	if (0 == strcmp("session-id", name)){
		return false;
	}
	if (0 == strcmp("session-id-ssl", name)){
		return false;
	}

	PlatformLockMutex( &session_mutex );

	char ret = int_setSessionValue(s, store, name, value);

	PlatformUnlockMutex( &session_mutex );

	return ret;
}

static char int_setSessionValue(http_request* s, STORE_TYPES store, const char* name, const char* value) {
	ws_variable* var = 0;
	if (store == SESSION_STORE) {
		if (s->store == 0){
			return false;
		}
		var = newVariable(s->store->vars, name, 0 );
		setWSVariableString(var, value);
	}
#ifdef WEBSERVER_USE_SSL
	if (store == SESSION_STORE_SSL) {
		if (s->store_ssl == 0){
			return false;
		}
		if (s->socket == 0){
			return false;
		}
		if (s->socket->use_ssl == 0){
			return false;
		}
		var = newVariable(s->store_ssl->vars, name, 0 );
		setWSVariableString(var, value);
	}
#endif

	return true;
}

ws_variable* addSessionValue(http_request* s, STORE_TYPES store, const char* name) {
	if (0 == strcmp("registered", name)){
		return 0;
	}
	if (0 == strcmp("session-id", name)){
		return 0;
	}
	if (0 == strcmp("session-id-ssl", name)){
		return 0;
		
	}

	PlatformLockMutex( &session_mutex );

	ws_variable* ret = int_addSessionValue(s, store, name);

	PlatformUnlockMutex( &session_mutex );

	return ret;
}

static ws_variable* int_addSessionValue(http_request* s, STORE_TYPES store, const char* name ) {
	ws_variable* var = 0;

	if ( s == 0 ){
		return 0;
	}

	if (store == SESSION_STORE) {
		if (s->store == 0){
			return 0;
		}
		var = newVariable(s->store->vars, name, 0 );
	}
#ifdef WEBSERVER_USE_SSL
	if (store == SESSION_STORE_SSL) {
		if (s->store_ssl == 0){
			return 0;
		}
		if (s->socket == 0){
			return 0;
		}
		if (s->socket->use_ssl == 0){
			return 0;
		}
		var = newVariable(s->store_ssl->vars, name, 0 );
	}
#endif

	return var;
}


static ws_variable* int_getSessionValue(http_request* s, STORE_TYPES store, const char* name) {
	ws_variable* var = 0;

	if (s == 0){
		return 0;
	}

	if (store == SESSION_STORE) {
		if (s->store == 0){
			return 0; /* snprintf(value,value_size,"value %s not found",name); */
		}
		var = getVariable(s->store->vars, name);
	}
	if (store == SESSION_STORE_SSL) {
#ifdef WEBSERVER_USE_SSL
		if (s->socket == 0){
			return 0; /* snprintf(value,value_size,"value %s not found",name); */
		}
		if (s->socket->use_ssl == 0){
			return 0; /*snprintf(value,value_size,"value %s not found",name); */
		}
		if (s->store_ssl == 0){
			return 0; /*snprintf(value,value_size,"value %s not found",name); */
		}
		var = getVariable(s->store_ssl->vars, name);
#else
		return 0; /* kein SSL verfügbar */
#endif
	}

	return var;
}

ws_variable* getSessionValue(http_request* s, STORE_TYPES store, const char* name) {
	
	if (s == 0){
		return 0;
	}

	if (0 == strcmp("session-id", name)){
		return 0;
	}
	if (0 == strcmp("session-id-ssl", name)){
		return 0;
	}

	PlatformLockMutex( &session_mutex );

	ws_variable* ret =  int_getSessionValue(s, store, name);

	PlatformUnlockMutex( &session_mutex );

	return ret;
}

void printSessionValue(http_request* s, STORE_TYPES store, char* name) {
	if (0 == strcmp("session-id", name)){
		return;
	}
	if (0 == strcmp("session-id-ssl", name)){
		return;
	}
}

char removeSessionValue(http_request* s, STORE_TYPES store, char* name) {

	ws_variable_store* var_store = 0;



	if (store == SESSION_STORE ) {
		if (s->store == 0){
			return false;
		}
		if (s->store->vars == 0){
			return false;
		}

		PlatformLockMutex( &session_mutex );

		var_store = s->store->vars;
	}
#ifdef WEBSERVER_USE_SSL
	if (store == SESSION_STORE_SSL ) {
		if (s->socket->use_ssl==0){
			return false;
		}
		if (s->store_ssl == 0){
			return false;
		}
		if (s->store_ssl->vars == 0){
			return false;
		}

		PlatformLockMutex( &session_mutex );

		var_store = s->store_ssl->vars;
	}
#endif

	delVariable(var_store,name);

	PlatformUnlockMutex( &session_mutex );

#ifdef ENABLE_DEVEL_WARNINGS
	#warning ("noch testen")
#endif
	return true;
}

void checkSessionCookie(http_request* s) {
	if (0 == checkCookie((char*) "session-id", s->guid, s->header)) {
		s->create_cookie = 1;
		generateGUID(s->guid, WEBSERVER_GUID_LENGTH);
#ifdef _WEBSERVER_SESSION_DEBUG_
		LOG(SESSION_LOG, DEBUG_LEVEL, s->socket->socket, "Create Cookie GUID : %s", s->guid);
#endif
	}
#ifdef WEBSERVER_USE_SSL
	if (s->socket->use_ssl == 1) {
		if (0 == checkCookie((char*) "session-id-ssl", s->guid_ssl, s->header)) {
			s->create_cookie_ssl = 1;
			generateGUID(s->guid_ssl, WEBSERVER_GUID_LENGTH);
#ifdef _WEBSERVER_SESSION_DEBUG_
			LOG(SESSION_LOG, DEBUG_LEVEL, s->socket->socket, "Create Cookie SSL GUID : %s", s->guid_ssl);
#endif
		}
	}
#endif
}


int dumpSessionStore( http_request* s ) {
	uint32_t s1 = 0;
	uint32_t s2 = 0;

	if (s->store != 0) {
		s1 = getVariableStoreSize(s->store->vars);
	}

	if (s->store_ssl != 0) {
		s2 = getVariableStoreSize(s->store_ssl->vars);
	}

	/* Two-column layout for Store and SSL Store */
	printHTMLChunk(s->socket,
		"<table>"
			"<tr>"
				"<th>Store (%"PRIu32" bytes)</th>"
				"<th>SSL Store (%"PRIu32" bytes)</th>"
			"</tr>"
			"<tr valign=\"top\"><td>",
		s1, s2);

	if (s->store != 0) {
		dumpStore(s, s->store->vars);
	} else {
		printHTMLChunk(s->socket, "<span class=\"text-muted\">No session variables</span>");
	}

	printHTMLChunk(s->socket, "</td><td>");

	if (s->store_ssl != 0) {
		dumpStore(s, s->store_ssl->vars);
	} else {
		printHTMLChunk(s->socket, "<span class=\"text-muted\">No SSL session variables</span>");
	}

	printHTMLChunk(s->socket, "</td></tr></table>");

	return 0;
}

void dumpSessions(http_request* s) {
	sessionStore* ss;
	uint32_t size, ticks;
	ws_variable *var;
	stk_stack* stack;
	rb_red_blk_node* node;
	int first = 1;

	stack = RBEnumerate(session_store_tree, (void*)"0", (void*)"z");

	/* Table header */
	printHTMLChunk(s->socket,
		"<table>"
			"<tr>"
				"<th>Session ID</th>"
				"<th>Type</th>"
				"<th>Size</th>"
				"<th>Timeout</th>"
				"<th>Variables</th>"
			"</tr>");

	while (0 != StackNotEmpty(stack)) {
		node = (rb_red_blk_node*) StackPop(stack);
		ss = (sessionStore*) node->info;
		size = getVariableStoreSize(ss->vars);
		size += sizeof(sessionStore);

		/* Calculate remaining timeout */
		ticks = PlatformGetTick() - ss->last_use;
		ticks = (getConfigInt("session_timeout") * PlatformGetTicksPerSeconde()) - ticks;
		ticks /= PlatformGetTicksPerSeconde();

		/* Session row */
		printHTMLChunk(s->socket,
			"<tr>"
				"<td class=\"mono\">%s</td>"
				"<td>%s</td>"
				"<td>%"PRIu32" B</td>"
				"<td>%"PRIu32" s</td>"
				"<td>",
			ss->guid,
			ss->ssl ? "SSL" : "HTTP",
			size, ticks);

		/* Variables as name=value pairs, one per line */
		var = getFirstVariable(ss->vars);
		first = 1;
		while (var != 0) {
			printHTMLChunk(s->socket, "<span class=\"mono\">%s</span>=", var->name);
			sendHTMLChunkVariable(s->socket, var);
			printHTMLChunk(s->socket, "<br>");
			first = 0;
			var = getNextVariable(ss->vars);
		}
		if (first) {
			printHTMLChunk(s->socket, "<span class=\"text-muted\">-</span>");
		}

		printHTMLChunk(s->socket, "</td></tr>");
	}

	printHTMLChunk(s->socket, "</table>");
	free(stack);
}

unsigned long dumpSessionsSize(int* count) {
	sessionStore* ss;
	unsigned long size = 0;
	int i = 0;
	stk_stack* stack;
	rb_red_blk_node* node;

	stack = RBEnumerate(session_store_tree, (void*)"0", (void*)"z");
	while (0 != StackNotEmpty(stack)) {
		node = (rb_red_blk_node*) StackPop(stack);
		ss = (sessionStore*) node->info;
		size += getVariableStoreSize(ss->vars);
		i++;
	}
	free(stack);
	if (count != 0){
		*count = i;
	}
	return size;
}
