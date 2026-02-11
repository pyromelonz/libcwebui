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

static int var_seeker(const void *el, const void *key) {
	/* let's assume el and key being always != NULL */
	ws_variable *var = (ws_variable *) el;
	if ( var == 0){
		return 0;
	}
	if ( key == 0){
		return 0;
	}

	if ((var->type == VAR_TYPE_REF) && (var->name == 0)) {
		if (0 == strcmp((char*) key, var->val.value_ref->name)){
			return 1;
		}
	} else {
		if (0 == strcmp(var->name, (char*) key)){
			return 1;
		}
	}
	return 0;
}


static char initVariableStore(ws_variable_store* store) {
	ws_list_init(&store->var_list);
	ws_list_attributes_seeker(&store->var_list, var_seeker);
	PlatformCreateMutex(&store->lock);
	return 0;
}

ws_variable_store* createVariableStore(void) {
	ws_variable_store *ret = (ws_variable_store*)WebserverMalloc( sizeof(ws_variable_store) );
	initVariableStore(ret);
	return ret;
}

void lockStore(ws_variable_store* store) {
	PlatformLockMutex(&store->lock);
}

void unlockStore(ws_variable_store* store) {
	PlatformUnlockMutex(&store->lock);
}

void deleteVariableStore(ws_variable_store* store) {
	clearVariables(store);
	PlatformDestroyMutex(&store->lock);
	ws_list_destroy(&store->var_list);
	WebserverFree(store);
}

unsigned int getVariableStoreSize(ws_variable_store* store) {
	unsigned int ret = 0;
	ws_variable* var = 0;
	if (store == 0){
		return 0;
	}

	var = getFirstVariable(store);
	while (var != 0) {
		ret += getWSVariableSize(var);
		var = getNextVariable(store);
	}

	ret += sizeof(ws_variable_store);

	return ret;
}

ws_variable* searchVariable(ws_variable_store* store, const char* name) {
	ws_variable* var = 0;
	if (store == 0){
		return 0;
	}

	var = (ws_variable *) ws_list_seek(&store->var_list, name);

	return var;
}

ws_variable* newVariable(ws_variable_store* store, const char* name, uint32_t flags ) {
	ws_variable* var = 0;
	if (store == 0){
		return 0;
	}

	if (name != 0) {
		if ( flags & NO_CHECK_NAME_EXISTS ){
				var = newWSVariable(name);
				addVariable(store, var);
		}else{
			var = searchVariable(store, name);
			if (var == 0) {
				var = newWSVariable(name);
				addVariable(store, var);
			}
		}
	} else {
		var = newWSVariable("");
		addVariable(store, var);
	}
	return var;
}

ws_variable* getVariable(ws_variable_store* store, const char* name) {
	if (store == 0){
		return 0;
	}
	return searchVariable(store, name);
}

void addVariable(ws_variable_store* store, ws_variable* var) {
	if (store == 0){
		return;
	}
	ws_list_append(&store->var_list, var,0);
}

void delVariable(ws_variable_store* store, const char* name) {
	ws_variable* var;
	if (store == 0){
		return;
	}
	var = searchVariable(store, name);
	freeVariable(store,var);

}

ws_variable* refVariable(ws_variable_store* store, ws_variable* ref, const char* new_name) {
	ws_variable *ret;
	ret = newWSVariable(new_name);
	setWSVariableRef(ret, ref);
	addVariable(store, ret);
	return ret;
}

void freeVariable(ws_variable_store* store, ws_variable* var) {
	if (var == 0){
		return;
	}
	if (store == 0){
		return;
	}

	// TODO warum wird hier der seeker entfernt ??
	ws_list_attributes_seeker(&store->var_list, 0);
	ws_list_delete(&store->var_list, var);
	ws_list_attributes_seeker(&store->var_list, var_seeker);
	freeWSVariable(var);

}

void clearVariables(ws_variable_store* store) {
	ws_variable *var;
	if (store == 0){
		return;
	}

	var = getFirstVariable(store);
	while (var != 0) {
		freeWSVariable(var);
		var = getNextVariable(store);
	}
	stopIterateVariable(store);
	ws_list_clear(&store->var_list);
}

ws_variable* getFirstVariable(ws_variable_store* store) {
	if (store == 0){
		return 0;
	}

	if (ws_list_size(&store->var_list) == 0){
		return 0;
	}

	// TODO hier kann man auch ws_list_get_at() verwenden
	ws_list_iterator_start(&store->var_list);
	if (0 == ws_list_iterator_hasnext(&store->var_list)) {
		ws_list_iterator_stop(&store->var_list);
		return 0;
	}
	return (ws_variable*) ws_list_iterator_next(&store->var_list);
}

ws_variable* getNextVariable(ws_variable_store* store) {
	if (store == 0){
		return 0;
	}
	if (0 == ws_list_iterator_hasnext(&store->var_list)) {
		ws_list_iterator_stop(&store->var_list);
		return 0;
	}
	return (ws_variable*) ws_list_iterator_next(&store->var_list);
}

void stopIterateVariable(ws_variable_store* store) {
	if (store == 0){
		return;
	}
	ws_list_iterator_stop(&store->var_list);
}

void dumpStore(http_request* s, ws_variable_store* store) {
#ifndef SIZE_TYPE_PRINT_DEZ
	#error SIZE_TYPE_PRINT_DEZ not defined for platform
#endif

	ws_variable *var;
	if (store != 0) {
		printHTMLChunk(s->socket,
			"<table>"
				"<tr><th>Name</th><th>Value</th><th>Size</th></tr>");
		var = getFirstVariable(store);
		while (var != 0) {
			printHTMLChunk(s->socket,
				"<tr>"
					"<td class=\"mono\">%s</td>"
					"<td>", var->name);
			sendHTMLChunkVariable(s->socket, var);
			printHTMLChunk(s->socket,
					"</td>"
					"<td>%"SIZE_TYPE_PRINT_DEZ"</td>"
				"</tr>", getWSVariableSize(var));
			var = getNextVariable(store);
		}
		printHTMLChunk(s->socket, "</table>");
	} else {
		printHTMLChunk(s->socket, "<span class=\"text-muted\">No store variables</span>");
	}
}

void dumpStoreText(http_request* s, ws_variable_store* store, int tabs) {
	ws_variable *var;
	int i;
	
	var = getFirstVariable(store);
	while (var != 0) {

		for( i = 0 ; i < tabs ; i++ ){
			printHTMLChunk(s->socket, " ");
		}
		
		if ( var->type == VAR_TYPE_ARRAY ){
			printHTMLChunk(s->socket, "%s ( Array )\n", var->name);
			dumpStoreText(s, var->val.value_array, tabs + 5 );
			printHTMLChunk(s->socket, "\n");
		}else{
			printHTMLChunk(s->socket, "%s : ", var->name);
			sendHTMLChunkVariable(s->socket, var);
			printHTMLChunk(s->socket, "\n");
		}
		
		var = getNextVariable(store);
	}
	
}

