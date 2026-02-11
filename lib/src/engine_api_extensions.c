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
#include "webserver_api_functions.h"


#ifdef DMALLOC
#include <dmalloc/dmalloc.h>
#endif



hashmap_t *user_func_map;
hashmap_t *user_condition_map;
hashmap_t *websocket_handler_map;

list_t plugin_liste;
plugin_error_handler error_handler = 0;

plugin_s *current_plugin = 0;

static void free_websocket_handler( void* a );

static void free_user_func_value( void* a) {
	user_func_s* b = (user_func_s*)a;
	WebserverFree( b->name );
	WebserverFree( a );
}

static void free_user_cond_value( void* a) {
	user_condition_s *b = (user_condition_s*)a;
	WebserverFree( b->name );
	WebserverFree( a );
}

void DummyFunc( UNUSED_PARA void* a) {
}
void DummyFuncConst( UNUSED_PARA const void* a) {
}

int StrKeyComp(const void* a, const void* b) {
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

static void* free_plugin_info(const void *free_element){
	
	plugin_s *plugin = (plugin_s*)free_element;
	
	WebserverFree( plugin->name );
	WebserverFree( plugin->error );
	WebserverFree( plugin->path );
	
	WebserverFree( plugin );

	return 0;
}

void init_extension_api(void) {

#ifdef WEBSERVER_USE_WEBSOCKETS
	initWebsocketApi();
#endif

	user_func_map = hashmap_create();
	user_condition_map = hashmap_create();
	websocket_handler_map = hashmap_create();

	ws_list_init(&plugin_liste);
	ws_list_attributes_freer(&plugin_liste,free_plugin_info);

}

void free_extension_api(void) {
	hashmap_destroy(user_func_map, free_user_func_value);
	hashmap_destroy(user_condition_map, free_user_cond_value);
	hashmap_destroy(websocket_handler_map, free_websocket_handler);
	ws_list_destroy(&plugin_liste);

}

void register_function(const char* name, user_function func, const char* file, int line) {
	int len = 0;
	user_func_s *tmp = (user_func_s*) WebserverMalloc( sizeof(user_func_s) );
	len = strlen(name) + 1;
	tmp->name = (char*) WebserverMalloc( len );
	strncpy(tmp->name, name, len);

	tmp->file = file;
	tmp->line = line;
	tmp->plugin = current_plugin;

	tmp->type = 0;
	tmp->uf = func;

	hashmap_put(user_func_map, tmp->name, tmp);
}

#ifdef WEBSERVER_USE_PYTHON
void register_py_function( const char* name, PyObject * py_func, const char* file, int line) {
	int len = 0;

	user_func_s *tmp ;

	tmp = (user_func_s*) hashmap_get(user_func_map, name);
	if (tmp != NULL) {
		if ( tmp->type == 1 ) {
			printf("reregister PyFunc: %s name \n",name);
			tmp->file = file;
			tmp->line = line;

			Py_XDECREF(tmp->py_func);

			tmp->py_func = py_func;
			return;
		}
	}

	tmp = (user_func_s*) WebserverMalloc( sizeof(user_func_s) );

	len = strlen(name) + 1;
	tmp->name = (char*) WebserverMalloc( len );
	strncpy(tmp->name, name, len);

	tmp->file = file;
	tmp->line = line;
	tmp->plugin = current_plugin;

	tmp->type = 1;
	tmp->py_func = py_func;

	hashmap_put(user_func_map, tmp->name, tmp);
}
#endif

int check_platformFunction_exists(FUNCTION_PARAS* func) {
	user_func_s* uf;

	if (func->parameter[0].text == 0){
		LOG(TEMPLATE_LOG, INFO_LEVEL, 0, "%s","Platform Function Parameter 0 == 0");
		return 1;
	}

	uf = (user_func_s*) hashmap_get(user_func_map, func->parameter[0].text);
	if (uf == NULL) {
		func->platform_function = NULL;
		LOG(TEMPLATE_LOG, INFO_LEVEL, 0, "Platform Function %s not found", func->parameter[0].text);
		return 1;
	}
	func->platform_function = uf;
	return 0;
}

void engine_platformFunction(http_request *s, FUNCTION_PARAS* func) {
	user_func_s *tmp;
	char dummy[] = "compiled in";
	char *name;

	if (func->parameter[0].text == 0){
		return;
	}

	if ( func->platform_function == NULL ){
		LOG( TEMPLATE_LOG, ERROR_LEVEL, s->socket->socket, "Error platform_function not set %s",func->parameter[0].text);
		return;
	}

	tmp = (user_func_s*) func->platform_function;
#ifdef _WEBSERVER_ENGINE_PLUGINS_DEBUG_
	LOG( TEMPLATE_LOG, NOTICE_LEVEL, s->socket->socket, "Calling Plugin Function %s enter", func->parameter[0].text);
#endif

	if ((error_handler != 0) && (tmp->plugin != 0)){
		name = tmp->plugin->name;
	}else{
		name = dummy;
	}

	if (error_handler != 0) {
		error_handler(PLUGIN_FUNCTION_CALLING, name, func->parameter[0].text, "crashed");
	}

	if ( tmp->type == 0 ){
		tmp->uf(s, func);
	}

#ifdef WEBSERVER_USE_PYTHON
	if ( tmp->type == 1 && tmp->plugin != 0 )
		py_call_engine_function( s, tmp, func );
#endif

	if (error_handler != 0) {
		error_handler(PLUGIN_FUNCTION_CALLED, name, func->parameter[0].text, "crashed");
	}

#ifdef _WEBSERVER_ENGINE_PLUGINS_DEBUG_
	LOG( TEMPLATE_LOG, NOTICE_LEVEL, s->socket->socket, "Calling Plugin Function %s exit", func->parameter[0].text ? func->parameter[0].text : "(null)");
#endif

}


typedef struct {
	http_request* s;
	int print_internal;  /* 1 = print internal (plugin==0), 0 = print plugins */
} print_func_ctx_t;

static void print_registered_func_cb(const char* key, void* value, void* user_data) {
	user_func_s *uf = (user_func_s*) value;
	print_func_ctx_t *ctx = (print_func_ctx_t*) user_data;
	int status = 0;
	const char *status_text;
	const char *status_class;
	const char *type_str;
	const char *plugin_name;
	(void)key;

	/* Filter: internal vs plugin */
	if (ctx->print_internal && uf->plugin != 0) return;
	if (!ctx->print_internal && uf->plugin == 0) return;

	/* Check function status */
	plugin_name = (uf->plugin == 0) ? "internal" : uf->plugin->name;
	if (error_handler != 0) {
		status = error_handler(PLUGIN_FUNCTION_CHECK_ERROR, plugin_name, uf->name, "crashed");
	}

	if (status == 0) {
		status_text = "ok";
		status_class = "status-ok";
	} else {
		status_text = "crashed";
		status_class = "status-error";
	}

	type_str = (uf->type == 0) ? "C" : "Python";

	printHTMLChunk(ctx->s->socket,
		"<tr>"
			"<td>%s</td>"
			"<td class=\"mono\">%s</td>"
			"<td>%s</td>"
			"<td class=\"mono\">%s</td>"
			"<td>%d</td>"
			"<td><span class=\"%s\">%s</span></td>"
		"</tr>",
		plugin_name, uf->name, type_str, uf->file, uf->line, status_class, status_text);
}

void printRegisteredFunctions(http_request* s) {
	print_func_ctx_t ctx;
	ws_variable* var1 = getParameter(s, "reset_functions");

	if (var1 != 0) {
		error_handler(PLUGIN_FUNCTION_RESET_CRASHED, "", "", "");
	}

	ctx.s = s;

	/* First pass: internal functions (sorted) */
	ctx.print_internal = 1;
	hashmap_foreach_sorted(user_func_map, print_registered_func_cb, &ctx);

	/* Second pass: plugin functions (sorted) */
	ctx.print_internal = 0;
	hashmap_foreach_sorted(user_func_map, print_registered_func_cb, &ctx);
}

static void print_registered_cond_cb(const char* key, void* value, void* user_data) {
	user_condition_s *uc = (user_condition_s*) value;
	http_request *s = (http_request*) user_data;
	int status = 0;
	const char *status_text;
	const char *status_class;
	const char *plugin_name;
	(void)key;

	/* Check condition status */
	plugin_name = (uc->plugin == 0) ? "internal" : uc->plugin->name;
	if (error_handler != 0) {
		status = error_handler(PLUGIN_FUNCTION_CHECK_ERROR, plugin_name, uc->name, "crashed");
	}

	if (status == 0) {
		status_text = "ok";
		status_class = "status-ok";
	} else {
		status_text = "crashed";
		status_class = "status-error";
	}

	/* Note: Conditions don't have a type field, so we use "-" */
	printHTMLChunk(s->socket,
		"<tr>"
			"<td>%s</td>"
			"<td class=\"mono\">%s</td>"
			"<td>-</td>"
			"<td class=\"mono\">%s</td>"
			"<td>%d</td>"
			"<td><span class=\"%s\">%s</span></td>"
		"</tr>",
		plugin_name, uc->name, uc->file, uc->line, status_class, status_text);
}

void printRegisteredConditions(http_request* s) {
	hashmap_foreach_sorted(user_condition_map, print_registered_cond_cb, s);
}

CONDITION_RETURN engine_callCondition(http_request *s, FUNCTION_PARAS* func) {
	user_condition_s *tmp;
	CONDITION_RETURN cond_ret = CONDITION_ERROR;

	tmp = (user_condition_s*) hashmap_get(user_condition_map, func->parameter[0].text);
	if (tmp != NULL) {
		cond_ret = tmp->uc(s, func);
	}

	if (cond_ret == CONDITION_ERROR) {
		cond_ret = builtinConditions(s, func);
	}

	if (cond_ret == CONDITION_ERROR) {
		LOG(TEMPLATE_LOG, NOTICE_LEVEL, s->socket->socket, "Condition %s not found", func->parameter[0].text);
	}
	return cond_ret;
}

void register_condition(const char* name, user_condition f, const char* file, int line) {
	int len = 0;
	user_condition_s *tmp = (user_condition_s*) WebserverMalloc( sizeof(user_condition_s) );
	len = strlen(name) + 1;
	tmp->name = (char*) WebserverMalloc( len );
	strncpy(tmp->name, name, len);
	tmp->uc = f;
	tmp->file = file;
	tmp->line = line;
	tmp->plugin = current_plugin;
	hashmap_put(user_condition_map, tmp->name, tmp);
}


/*
	http://ctpp.havoc.ru/doc/en/


 Kein Locking fuer Websocket Handler weil Plugins im init_hook handler registrieren muessen
*/

static void free_websocket_handler( void* a ){
	websocket_handler_list_s *l = (websocket_handler_list_s*)a;
	WebserverFree( l->url );
	
	ws_list_destroy( l->handler_list );
	WebserverFree( l->handler_list );

	WebserverFree( l );
}

static void* free_websocket_handler_ele( const void* a ){
	websocket_handler_s *b = (websocket_handler_s*)a;
	
	WebserverFree( b );

	return 0;
}

void register_function_websocket_handler(const char* url, websocket_handler f, const char* file, int line) {
	websocket_handler_s *tmp;
	websocket_handler_list_s *list;

	list = (websocket_handler_list_s*) hashmap_get(websocket_handler_map, url);

	if (list == NULL) {
		list = (websocket_handler_list_s*) WebserverMalloc( sizeof(websocket_handler_list_s) );
		list->handler_list = (list_t*) WebserverMalloc( sizeof(list_t) );
		ws_list_init( list->handler_list );
		ws_list_attributes_freer( list->handler_list,free_websocket_handler_ele );
		list->url = (char*) WebserverMalloc( strlen(url) + 1 );
		strcpy(list->url, url);
		hashmap_put(websocket_handler_map, list->url, list);
	}

	tmp = (websocket_handler_s*) WebserverMalloc( sizeof(websocket_handler_s) );
	tmp->wsh = f;
	tmp->file = file;
	tmp->line = line;
	ws_list_append(list->handler_list, tmp, 0);

}

int handleWebsocketConnection(WEBSOCKET_SIGNALS signal, const char* guid, const char* url, const char binary , const char* msg, const unsigned long long len) {
	websocket_handler_s *handler;
	websocket_handler_list_s *list;

	list = (websocket_handler_list_s*) hashmap_get(websocket_handler_map, url);
	if (list != NULL) {
		ws_list_iterator_start(list->handler_list);
		while (ws_list_iterator_hasnext(list->handler_list)) {
			handler = (websocket_handler_s*) ws_list_iterator_next(list->handler_list);
			handler->wsh(signal, guid,binary, msg, len);
		}
		ws_list_iterator_stop(list->handler_list);
	}
	return 0;
}

char* (*init_webserver_plugin)(void);
int (*get_webserver_api_version)(void);

#define COPY_ERROR(a) \
		plugin->error = (char*) WebserverMalloc( strlen(a) +1 ); \
		strcpy(plugin->error, a);

int loadPlugin(const char* name, const char* path) {

#ifdef LINUX
	#define PLUGIN_SUPPORT

	int version;
	char *error_text;
	void *dl;

	plugin_s *plugin, *plugin_tmp;

	/* Pruefen on Plugin schon geladen wurde */
	ws_list_iterator_start(&plugin_liste);
	while ((plugin_tmp = (plugin_s*) ws_list_iterator_next(&plugin_liste))) {
		if (0 == strcmp(plugin_tmp->path, path)) {
			printf("Plugin %s already exists\n", path);
			ws_list_iterator_stop(&plugin_liste);
			return -1;
		}
	}
	ws_list_iterator_stop(&plugin_liste);

	/* Plugin Struktur erzeugen */
	plugin = (plugin_s*) WebserverMalloc( sizeof(plugin_s) );
	plugin->name = (char*) WebserverMalloc( strlen(name) + 1 );
	strcpy(plugin->name, name);
	plugin->path = (char*) WebserverMalloc( strlen(path) + 1 );
	strcpy(plugin->path, path);

	ws_list_append(&plugin_liste, plugin, 0);

	/* Prueen ob die Plugin Init Funktion abgestuertzt ist */
	if (error_handler != 0) {
		if (0 != error_handler(PLUGIN_CHECK_ERROR, name, "", "")) {
			COPY_ERROR("crashed")
			return -6;
		}
	}

	/* Plugin Shared Object laden */
#ifdef RTLD_DEEPBIND
	/* dlopen(path, RTLD_NOW | RTLD_GLOBAL | RTLD_DEEPBIND ); */
	dl = dlopen(path, RTLD_NOW | RTLD_GLOBAL);
#else
	dl = dlopen(path, RTLD_NOW | RTLD_GLOBAL);
#endif
	error_text = dlerror();
	if (dl == 0) {
		if (error_handler != 0) {
			error_handler(PLUGIN_LOADING, name, "", "dlopen Error");
		}
		COPY_ERROR("dlopen Error")
		printf("Error loading %s %s\n", path, error_text);
		return -2;
	}

	/* Funktion zum abfragen der API Version suchen */
	get_webserver_api_version = (int (*)(void)) dlsym(dl, "get_webserver_api_version"); /* hier muss das plugin seine template funktionen registrieren */
	error_text = dlerror();

	if (get_webserver_api_version == 0) {
		if (error_handler != 0) {
			error_handler(PLUGIN_LOADING, name, "", "No get_webserver_api_version Function");
		}
		COPY_ERROR("No get_webserver_api_version Function")
		printf("Error loading %s. No Init Function char* get_webserver_api_version(void) %s\n", path, error_text);
		dlclose( dl );
		return -3;
	}

	/* API Version abfragen */
	version = (*get_webserver_api_version)();
	if (version > WEBSERVER_API) {
		if (error_handler != 0) {
			error_handler(PLUGIN_LOADING, name, "", "API Version inkompatibel");
		}
		COPY_ERROR("API Version inkompatibel")
		LOG( TEMPLATE_LOG, ERROR_LEVEL, 0, "Plugin API Version inkompatibel ( Plugin %d > Webserver %d ) -> %s", version, WEBSERVER_API, path);
		dlclose( dl );
		return -4;
	}

	/* Plugin Init Funktion suchen */
	init_webserver_plugin = (char *(*)(void)) dlsym(dl, "init_webserver_plugin"); /* hier muss das plugin seine template funktionen registrieren */
	error_text = dlerror();

	if (init_webserver_plugin == 0) {
		if (error_handler != 0) {
			error_handler(PLUGIN_LOADING, name, "", "No init_webserver_plugin Function");
		}
		COPY_ERROR("No init_webserver_plugin Function")
		printf("Error loading %s. No Init Function char* init_webserver_plugin(void) %s \n", path, error_text);
		dlclose( dl );
		return -5;
	}


	if (error_handler != 0) {
		error_handler(PLUGIN_LOADING, name, "", "crashed on init");
	}

	current_plugin = plugin;

	plugin->type = 0;

	(*init_webserver_plugin)();

	current_plugin = 0;

	if (error_handler != 0) {
		error_handler(PLUGIN_LOADED, name, "", "crashed on init");
	}

	COPY_ERROR("loaded")

	plugin->dl = dl;

	return 0;
#endif

#ifndef PLUGIN_SUPPORT
	printf("loadPlugin not implemented\n");
	return 0;
#endif

}

void RegisterPluginErrorHandler(plugin_error_handler f) {
	error_handler = f;
}

void printRegisteredPlugins(http_request* s) {
	plugin_s *p = 0;

	ws_variable *var1;

	var1 = getParameter(s, "reset_plugins");
	if (var1 != 0) {
		error_handler(PLUGIN_RESET_CRASHED, "", "", "");
	}

	ws_list_iterator_start(&plugin_liste);
	while ((p = (plugin_s*) ws_list_iterator_next(&plugin_liste))) {
		char type[10];
		if( p->type == 0 ){
			sprintf(type,"C");
		}
		if( p->type == 1 ){
			sprintf(type,"Python");
		}

		if (0 == strcmp(p->error, "crashed")){
			printHTMLChunk(s->socket, "<tr><td>%s<td>%s<td>%s<td><font color=red>%s</font>", p->name, p->path, type, p->error);
		}else{
			printHTMLChunk(s->socket, "<tr><td>%s<td>%s<td>%s<td><font color=green>%s</font>", p->name, p->path, type, p->error);
		}
	}
	ws_list_iterator_stop(&plugin_liste);
}

