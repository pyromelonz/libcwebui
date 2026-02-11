/*

SPDX-License-Identifier: MPL-2.0

 libCWebUI
 Copyright (C) 2007 - 2019  Ramin Seyed-Moussavi

 Projekt URL : https://github.com/lordrasmus/libcwebui

 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at https://mozilla.org/MPL/2.0/. 

*/

#ifdef __CDT_PARSER__
	#define __BASE_FILE__
#endif

#include "webserver.h"

#ifdef DMALLOC
#include <dmalloc/dmalloc.h>
#endif


int cmpVariableStoreName(void* var, char* name) {
	ws_variable* sv;

	if (var == 0) {
		return -1;
	}

	sv = (ws_variable*) var;

#ifdef _WEBSERVER_TEMPLATE_DEBUG_
	LOG(TEMPLATE_LOG,NOTICE_LEVEL,0,"%s<->%s",name,sv->name);
#endif

	if (0 == strcmp( name, sv->name)) {
#ifdef _WEBSERVER_TEMPLATE_DEBUG_
		LOG(TEMPLATE_LOG,NOTICE_LEVEL,0,"%s found",name);
#endif
		return 0;
	}

#ifdef _WEBSERVER_TEMPLATE_DEBUG_
	LOG(TEMPLATE_LOG,NOTICE_LEVEL,0,"%s not found",name);
#endif
	return -1;
}

void freeWSVariable(ws_variable* var) {
	if (var == 0){
		return;
	}

	if (var->name != 0) {
		WebserverFree(var->name);
		var->name = 0;
	}

	freeWSVariableValue(var);
	WebserverFree(var);
}

ws_variable* newWSVariable(const char* name) {
	ws_variable* ret = WebserverMallocVariable_store();
	if (name != 0) {
		SIZE_TYPE length = strlen((char*) name);
		ret->name = (char*) WebserverMalloc( length + 1 );
		ret->name_len = length;
		Webserver_strncpy( ret->name, length + 1, (char*) name, length);
	}
	return ret;
}

ws_variable* newWSArray(const char* name) {

	ws_variable* ret = newWSVariable( name );

	setWSVariableArray( ret );

	return ret;
}

void freeWSVariableValue(ws_variable* var) {
	if (var == 0) {
		return;
	}
	switch (var->type) {
	case VAR_TYPE_ARRAY:
		deleteVariableStore(var->val.value_array);
		break;
	case VAR_TYPE_STRING:
		WebserverFree(var->val.value_p);
		break;
	case VAR_TYPE_INT:
		break;
	case VAR_TYPE_ULONG:
		break;
    case VAR_TYPE_LONG:
		break;
	case VAR_TYPE_EMPTY:
		break;
	case VAR_TYPE_REF:
		break;
	case VAR_TYPE_CUSTOM_DATA:
		var->extra.handle(var->val.value_p);
		break;

	}
	var->val.value_p = 0;
	var->type = VAR_TYPE_EMPTY;
}

SIZE_TYPE getWSVariableSize(ws_variable* var) {
	SIZE_TYPE ret = 0;

	if (var == 0) {
		return 0;
	}

	ret = sizeof(ws_variable);
	ret += var->name_len + 1;

	if (var->type == VAR_TYPE_STRING){
		ret += var->extra.str_len + 1;
	}
	if (var->type == VAR_TYPE_ARRAY){
		ret += getVariableStoreSize(var->val.value_array);
	}
	return ret;
}

void setWSVariableString(ws_variable* var, const char* text) {
	SIZE_TYPE length;
	int ret;

	if (var == 0){
		return;
	}

	freeWSVariableValue(var);
	var->type = VAR_TYPE_STRING;
	if (text != 0) {

#ifdef _WEBSERVER_TEMPLATE_DEBUG_
		LOG (TEMPLATE_LOG,NOTICE_LEVEL,0,"setWSVariableString %s %s",var->name, text);
#endif
		length = strlen(text);
		var->val.value_string = (char*) WebserverMalloc( length + 1 );
		Webserver_strncpy(var->val.value_string, length + 1, text, length);
		var->extra.str_len = length;
	} else {
#ifdef _WEBSERVER_TEMPLATE_DEBUG_
		LOG (TEMPLATE_LOG,NOTICE_LEVEL,0,"setWSVariableString %s %s",var->name, text ? text : "(null)");
#endif
		var->val.value_string = (char*) WebserverMalloc( 100 );
		ret = snprintf(var->val.value_string, 100, "Var %s text=0x0",var->name);
		if ( ret  < 0 ){
			perror("setWSVariableString: snprintf failed\n");
			var->val.value_string[0] = '\0';
			return;
		}
		var->extra.str_len = ret;
	}
}

void setWSVariableCustomData(ws_variable* var, var_free_handler handle, void* data ) {
	if (var == 0){
		return;
	}

	freeWSVariableValue(var);

	var->type = VAR_TYPE_CUSTOM_DATA;
	var->val.value_p = data;
	var->extra.handle = handle;
}

int getWSVariableString(ws_variable* var, char* buffer,	unsigned int buffer_length) {
	
	if( buffer_length > 0 ){
		buffer[0] = 0;
	}
	
	if (var == 0){
		return -1;
	}

	if ((var->val.value_p == 0) && (var->type != VAR_TYPE_INT)) {
		return snprintf(buffer, buffer_length, "Empty");
	}

	switch (var->type) {
		case VAR_TYPE_EMPTY:
			return snprintf(buffer, buffer_length, "Empty");

		case VAR_TYPE_ARRAY:
			return snprintf(buffer, buffer_length, "Array");

		case VAR_TYPE_STRING:
			return snprintf(buffer, buffer_length, "%s", var->val.value_string);

		case VAR_TYPE_INT:
			return snprintf(buffer, buffer_length, "%d", var->val.value_int);
		
        case VAR_TYPE_ULONG:
			return snprintf(buffer, buffer_length, "%"PRIu64, var->val.value_uint64_t);
        
        case VAR_TYPE_LONG:
			return snprintf(buffer, buffer_length, "%"PRId64, var->val.value_int64_t);

		case VAR_TYPE_CUSTOM_DATA:
			return snprintf(buffer, buffer_length, "custom_data");

		case VAR_TYPE_REF:
#ifdef ENABLE_DEVEL_WARNINGS
			#warning noch testen
#endif
			return getWSVariableString(var->val.value_ref, buffer, buffer_length);

	}

	return snprintf(buffer, buffer_length, "Unknown Type %d", var->type);

}

void setWSVariableInt(ws_variable* var, int value) {
	if (var == 0){
		LOG( VARIABLE_LOG,ERROR_LEVEL,0,"%s","var pointer is 0" );
		return;
	}
	freeWSVariableValue(var);
	var->type = VAR_TYPE_INT;
	var->val.value_int = value;
}

void setWSVariableULong(ws_variable* var, uint64_t value) {
	if (var == 0){
		LOG( VARIABLE_LOG,ERROR_LEVEL,0,"%s","var pointer is 0");
		return;
	}
	freeWSVariableValue(var);
	var->type = VAR_TYPE_ULONG;
	var->val.value_uint64_t = value;
}

void setWSVariableLong(ws_variable* var, int64_t value) {
	if (var == 0){
		LOG( VARIABLE_LOG,ERROR_LEVEL,0,"%s","var pointer is 0");
		return;
	}
	freeWSVariableValue(var);
	var->type = VAR_TYPE_LONG;
	var->val.value_int64_t = value;
}

int getWSVariableInt(ws_variable* var) {
	int ret;
	if ( var == 0 ){
		return 0;
	}
	switch (var->type) {
		case VAR_TYPE_ARRAY:
		case VAR_TYPE_EMPTY:
		case VAR_TYPE_CUSTOM_DATA:
		case VAR_TYPE_REF:
			return 0;
		case VAR_TYPE_STRING:
			sscanf(var->val.value_string, "%d", &ret);
			return ret;
		case VAR_TYPE_INT:
			return var->val.value_int;
		case VAR_TYPE_ULONG:
			return var->val.value_uint64_t;
        case VAR_TYPE_LONG:
			return var->val.value_int64_t;
	}

	return 0;

}

uint64_t getWSVariableULong(ws_variable* var) {
	uint64_t ret;
	if ( var == 0 ){
		return 0;
	}
	switch (var->type) {
		case VAR_TYPE_ARRAY:
		case VAR_TYPE_EMPTY:
		case VAR_TYPE_CUSTOM_DATA:
		case VAR_TYPE_REF:
			return 0;
		case VAR_TYPE_STRING:
			sscanf(var->val.value_string, "%"PRIu64, &ret);
			return ret;
		case VAR_TYPE_INT:
			return var->val.value_int;
		case VAR_TYPE_ULONG:
			return var->val.value_uint64_t;
        case VAR_TYPE_LONG:
			return (uint64_t)var->val.value_int64_t;
	}

	return 0;

}

int64_t getWSVariableLong(ws_variable* var) {
	int64_t ret;
	if ( var == 0 ){
		return 0;
	}
	switch (var->type) {
		case VAR_TYPE_ARRAY:
		case VAR_TYPE_EMPTY:
		case VAR_TYPE_CUSTOM_DATA:
		case VAR_TYPE_REF:
			return 0;
		case VAR_TYPE_STRING:
			sscanf(var->val.value_string, "%"PRId64, &ret);
			return ret;
		case VAR_TYPE_INT:
			return var->val.value_int;
		case VAR_TYPE_ULONG:
			return (int64_t)var->val.value_uint64_t;
        case VAR_TYPE_LONG:
			return var->val.value_int64_t;
	}

	return 0;

}

void setWSVariableRef(ws_variable* var, ws_variable* ref) {
	if ( var == 0 ){
		return;
	}

	freeWSVariableValue(var);
	var->type = VAR_TYPE_REF;
	var->val.value_ref = ref;
}

void setWSVariableArray(ws_variable* var) {
	if (var == 0){
		return;
	}

	freeWSVariableValue(var);
	var->type = VAR_TYPE_ARRAY;
	var->val.value_array = createVariableStore();
}

ws_variable* getWSVariableArray(ws_variable* var, const char* name) {
	if ( var == 0 ){
		return 0;
	}
	if (var->type == VAR_TYPE_ARRAY){
		return getVariable(var->val.value_array, name);
	}

	if ((var->type == VAR_TYPE_REF) && (var->val.value_ref->type == VAR_TYPE_ARRAY)){
		return getVariable(var->val.value_ref->val.value_array, name);
	}
	return 0;
}

ws_variable* addWSVariableArray(ws_variable* var, const char* name, uint32_t flags ) {
	if ( var == 0 ){
		return 0;
	}
	if (var->type == VAR_TYPE_ARRAY){
		return newVariable(var->val.value_array, name, flags );
	}
	if ( (var->type == VAR_TYPE_REF) && (var->val.value_ref->type == VAR_TYPE_ARRAY) ) {
#ifdef ENABLE_DEVEL_WARNINGS
		#warning noch testen
#endif
		return addWSVariableArray(var->val.value_ref,name, flags );
	}

	LOG( VARIABLE_LOG,ERROR_LEVEL,0,"var %s is not an array",var->name );

	return 0;
}

ws_variable* refWSVariableArray(ws_variable* var, ws_variable* ref) {
	ws_variable *ret = newVariable(var->val.value_array, 0, 0 );
	setWSVariableRef(ret, ref);
	return ret;
}

void delWSVariableArray(ws_variable* var, const char* name) {
	ws_variable *tmp;
	if (var == 0){
		return;
	}
	if (var->type != VAR_TYPE_ARRAY){
		return;
	}
	tmp = getWSVariableArray(var, name);
	freeVariable(var->val.value_array, tmp);
}

ws_variable VISIBLE_ATTR * getWSVariableArrayFirst(ws_variable* var) {
	if ( var == 0 ){
		return 0;
	}
	if (var->type != VAR_TYPE_ARRAY){
		return 0;
	}

	return getFirstVariable(var->val.value_array);
}

ws_variable VISIBLE_ATTR * getWSVariableArrayNext(ws_variable* var) {
	if ( var == 0 ){
		return 0;
	}
	if (var->type != VAR_TYPE_ARRAY){
		return 0;
	}

	return getNextVariable(var->val.value_array);
}

void VISIBLE_ATTR stopWSVariableArrayIterate(ws_variable* var) {
	if ( var == 0 ){
		return;
	}
	if (var->type != VAR_TYPE_ARRAY){
		return;
	}
	stopIterateVariable(var->val.value_array);
}

ws_variable* getWSVariableArrayIndex(ws_variable* var, unsigned int index) {
	ws_variable *tmp;
	unsigned int i = 0;

	if ( var == 0 ){
		return 0;
	}

	if (var->type != VAR_TYPE_ARRAY){
		return 0;
	}

	tmp = getFirstVariable(var->val.value_array);
	while (tmp != 0) {
		if (i++ == index) {
			stopIterateVariable(var->val.value_array);
			return tmp;
		}
		tmp = getNextVariable(var->val.value_array);
	}
	return 0;
}

ws_variable* addWSVariableArrayIndex(ws_variable* var,	unsigned int index) {
	ws_variable *tmp;
	unsigned int i = 0;
	char name_buf[10];

	if ( var == 0 ){
		return 0;
	}

	if (var->type != VAR_TYPE_ARRAY){
		return 0;
	}

	tmp = getFirstVariable(var->val.value_array);
	while (tmp != 0) {
		if (i == index){
			return tmp;
		}
		i++;
		tmp = getNextVariable(var->val.value_array);
	}
	for (; i <= index; i++) {
		snprintf(name_buf, 10, "%d", i);
		tmp = newVariable(var->val.value_array, name_buf, 0 );
	}

	return tmp;
}

