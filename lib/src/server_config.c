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


static ws_variable_store *config_v_store;

void initConfig(void) {
	config_v_store = createVariableStore();
	setConfigInt( "port",80);
	setConfigInt( "ssl_port",-1);
	setConfigInt( "max_post_size",2 * 1024 * 1024); // Default 2 MB Max Post Size
	setConfigInt( "use_csp",0);
	setConfigInt( "reload_py_modules",0);
	#ifdef WEBSERVER_SESSION_TIMEOUT
		setConfigInt( "session_timeout" , WEBSERVER_SESSION_TIMEOUT );
	#else
		setConfigInt( "session_timeout" , 300 );
	#endif

	setConfigInt( "ssl_disable_SSLv2",1);
	setConfigInt( "ssl_disable_SSLv3",1);
	setConfigInt( "ssl_disable_TLSv1.0",0);
	setConfigInt( "ssl_disable_TLSv1.1",0);

	//setConfigText( "server_ip","127.0.0.1");
}

void freeConfig(void){
	deleteVariableStore(config_v_store);
}

void setConfigText(const char* name, const char* text) {
	ws_variable *var;
	PlatformLockMutex(&config_v_store->lock);
	var = newVariable(config_v_store, name, 0 );
	setWSVariableString(var, text);
	PlatformUnlockMutex(&config_v_store->lock);
}

void setConfigInt(const char* name, const int value) {
	ws_variable *var;
	if (value < -1) {
		LOG(CONFIG_LOG, WARNING_LEVEL, 0, "Config '%s' has suspicious negative value: %d", name, value);
	}
	PlatformLockMutex(&config_v_store->lock);
	var = newVariable(config_v_store, name, 0 );
	setWSVariableInt(var, value);
	PlatformUnlockMutex(&config_v_store->lock);
}

char* getConfigText(const char* name) {
	ws_variable* var;
	var = getVariable(config_v_store, name);
	if (var != 0){
		return var->val.value_string;
	}
	return 0;
}

int getConfigInt(const char* name) {
	ws_variable* var = getVariable(config_v_store, name);
	if (var != 0){
		return getWSVariableInt(var);
	}
	return 0;
}
