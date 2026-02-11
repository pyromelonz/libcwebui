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
#pragma GCC push_options
#pragma GCC optimize ("O0")
#warning "Optimierung fuer Engine Parser deaktiviert"
*/

static void template_engine_start(http_request *s) {
	s->engine_current = &s->engine_list[s->engine_index++];
}

static void template_engine_stop(http_request *s) {
	if (s->engine_index == 0) {
		s->engine_current = 0;
		return;
	}
	s->engine_index--;
	memset(&s->engine_list[s->engine_index], 0, sizeof(engine_infos));
	if ( s->engine_index != 0 ){
		s->engine_current = &s->engine_list[s->engine_index - 1];
	}else{
		s->engine_current = 0;
	}
}

void getFunction(const unsigned char *para, int *function, int *id) {
	*function = para[1];
	*id = para[3] - '0';
}

void engine_loop_array(http_request *s, const char* prefix, const char *pagename, const char *pagedata, const int datalenght,
		FUNCTION_PARAS* func, int* i) {
	ws_variable *var_array, *tmp, *var_value_name = 0, *var_key_name = 0, *tmp_value, *tmp_key;
	int pos1 = find_tag_end_pos((char*) pagedata, datalenght, "{loop:", "{endloop}");
	*i += pos1;
	pos1 -= 9;

	tmp_key = 0;

	tmp = parseVariable(s, func->parameter[0].text);
	if (tmp == 0) {
		return;
	}
	if (tmp->type != VAR_TYPE_REF) {
		sendHTMLChunkVariable(s->socket, tmp);
		freeWSVariable( tmp );
		return;
	}

	var_array = tmp->val.value_ref;
	freeWSVariable(tmp);

	var_value_name = parseVariable(s, func->parameter[1].text);
	if (var_value_name->type != VAR_TYPE_STRING) {
		freeWSVariable(var_value_name);
		return;
	}

	var_key_name = parseVariable(s, func->parameter[2].text);

	tmp_value = getWSVariableArrayFirst(var_array);

	tmp = getRenderVariable(s, var_value_name->val.value_string);
	if ((var_key_name != 0) && (var_key_name->type == VAR_TYPE_STRING)) {
		tmp_key = getRenderVariable(s, var_key_name->val.value_string);
	}

	while (tmp_value != 0) {

		setWSVariableRef(tmp, tmp_value);

		if ((var_key_name != 0) && (var_key_name->type == VAR_TYPE_STRING)) {
			if ((0 == strcmp(tmp_value->name, "")) && (tmp_value->type == VAR_TYPE_REF)){
				setWSVariableString(tmp_key, tmp_value->val.value_ref->name);
			}else{
				setWSVariableString(tmp_key, tmp_value->name);
			}
		}

		processHTML(s, prefix, pagename, pagedata, pos1);

		tmp_value = getWSVariableArrayNext(var_array);
	}

	freeWSVariable(var_key_name);
	freeWSVariable(var_value_name);

	#if 0
	if ((s->socket->enable_print_funcs == 1) && (s->engine_current->func.function != TEMPLATE_ECHO_FUNCS_OFF)) {
		sendHTMLChunk(s->socket, s->socket->print_func_prefix, strlen(s->socket->print_func_prefix));
		int pos1 = stringnfind ( ( char* ) pagedata ,"{endloop}",datalenght );
		pos1 -= 8;
		sendHTMLChunk(s->socket, &pagedata[pos1], 9);	/* {} mit ausgeben */
		sendHTMLChunk(s->socket, s->socket->print_func_postfix, strlen(s->socket->print_func_postfix));
	}
	#endif

}

#define FUNC_CMP(a,c) { int b = strlen(a); if ( ( 0 == strncmp((char*)&buffer2[1],a,b) ) && ( buffer2[b+1] == '}' ) ){ 	return c; } }

#define FUNC_CMP_OPTS(a,c) { int b = strlen(a); if ( ( 0 == strncmp((char*)&buffer2[1],a,b) ) && ( ( buffer2[b+1] == ':' ) || ( buffer2[b+1] == '}' ) ) ){ return c; }}

static ENGINE_FUNCTIONS func_cmp_extendet(const char *buffer, int length, const char*name, ENGINE_FUNCTIONS func) {
	int b = strlen(name);

	if ( length < b ){
		return TEMPLATE_UNKNOWN;
	}

	if ( 0 != strncmp((char*) &buffer[1], name, b) ){
		return TEMPLATE_UNKNOWN;
	}

	if ( buffer[b + 1] != ':' ){
		return TEMPLATE_UNKNOWN;
	}

	return func;
}

static ENGINE_FUNCTIONS getEngineFunctionCode(const char *buffer2, int length) {
	ENGINE_FUNCTIONS ret;

	if (buffer2[0] != '{') {
		return TEMPLATE_UNKNOWN;
	}
	if ((buffer2[1] == 'f') && (buffer2[2] == ':')) {
		return TEMPLATE_PLATFORM_FUNCTION;
	}
	if ((buffer2[1] == 'b') && (buffer2[2] == ':')) {
		return TEMPLATE_BUILDIN_FUNCTION;
	}

	FUNC_CMP( "echo_off", TEMPLATE_ECHO_OFF)
	FUNC_CMP( "echo_on", TEMPLATE_ECHO_ON)

	FUNC_CMP_OPTS( "echo_funcs_off", TEMPLATE_ECHO_FUNCS_OFF)
	FUNC_CMP_OPTS( "echo_funcs_on", TEMPLATE_ECHO_FUNCS_ON)

	FUNC_CMP( "return", TEMPLATE_RETURN)

	ret = func_cmp_extendet( buffer2, length, "get", TEMPLATE_GET_VARIABLE );
	if (ret == TEMPLATE_GET_VARIABLE) {
		return TEMPLATE_GET_VARIABLE;
	}
	ret = func_cmp_extendet( buffer2, length, "set", TEMPLATE_SET_VARIABLE );
	if (ret == TEMPLATE_SET_VARIABLE) {
		return TEMPLATE_SET_VARIABLE;
	}
	FUNC_CMP_OPTS( "if", TEMPLATE_IF)
	FUNC_CMP_OPTS( "include", TEMPLATE_INCLUDE_FILE)
	FUNC_CMP_OPTS( "loop", TEMPLATE_LOOP_ARRAY)



	return TEMPLATE_UNKNOWN;
}

static int checkParameterString(parameter_info* para) {
	int i;
	for (i = 0; i < para->length; i++) {
		if (para->text[i] == '"'){
			continue;
		}
		if (para->text[i] == ' '){
			continue;
		}
		if (para->text[i] == '-'){
			continue;
		}
		if (para->text[i] == '.'){
			continue;
		}
		if (para->text[i] == '='){
			continue;
		}
		if (para->text[i] == ';'){
			continue;
		}
		if (para->text[i] == '_'){
			continue;
		}
		if (para->text[i] == '['){
			continue;
		}
		if (para->text[i] == ']'){
			continue;
		}
		if (para->text[i] == '/'){
			continue;
		}
		if (para->text[i] == '<'){
			continue;
		}
		if (para->text[i] == '>'){
			continue;
		}
		if (para->text[i] < '0'){
			return 1;
		}
		if (para->text[i] > 'z'){
			return 1;
		}
		if ((para->text[i] > '9') && (para->text[i] < 'A')){
			return 1;
		}
		if ((para->text[i] > 'Z') && (para->text[i] < 'a')){
			return 1;
		}
	}
	return 0;
}

static void parseFunction(engine_infos* engine, const char* buffer, int length) {
	int i2;
	int i, para_start, para_ende;
	FUNCTION_PARAS* func = &engine->func;
	ENGINE_FUNCTIONS tmp;

	tmp = getEngineFunctionCode(buffer, length);
	if (tmp == TEMPLATE_UNKNOWN) {
		func->function = tmp;
		return;
	} else {
		para_start = 0;
		//para_ende = 0;  // clang Dead store
		func->function = tmp;
	}

	for (i = 0, i2 = 0; i <= length; i++) {
		if ((buffer[i] == ':') || (buffer[i] == '}')) {
			if (para_start == 0) {
				para_start = i + 1;
			} else {
				para_ende = i;
				for (; i2 < MAX_FUNC_PARAS; i2++) {
					if (func->parameter[i2].text == 0) {
						int l = para_ende - para_start;
						func->parameter[i2].text = (char*) WebserverMalloc ( l + 1 );
						func->parameter[i2].length = (uint16_t)(l);
						memcpy(func->parameter[i2].text, &buffer[para_start], func->parameter[i2].length);
						func->parameter[i2].text[func->parameter[i2].length] = '\0';
						i2++;
						break;
					}
				}
				para_start = i + 1;
			}
		}
	}
	i = 0;
	while (1) {
		if (func->parameter[i].text == 0){
			break;
		}
		i++;
	}
	func->parameter_count = i;

	for (i = 0; i < func->parameter_count; i++) {
		if (0 != checkParameterString(&func->parameter[i])) {
			func->function = TEMPLATE_UNKNOWN;
			printf("Template Error : Parameter Format Error %s\n", func->parameter[i].text);
		}
	}

	switch (func->function) {
		case TEMPLATE_PLATFORM_FUNCTION:
		case TEMPLATE_BUILDIN_FUNCTION:
			if ( 0 != check_platformFunction_exists(func)){
				func->function = TEMPLATE_UNKNOWN;
			}
			break;
		default:
			break;
	}

}

static void freeFunction(engine_infos* engine) {
	int i;
	FUNCTION_PARAS* func = &engine->func;

	if ( func == 0 ){
		return;
	}

	for (i = 0; i < MAX_FUNC_PARAS; i++) {
		if (func->parameter[i].text != 0){
			WebserverFree(func->parameter[i].text);
		}
		func->parameter[i].text = 0;
	}
}

#ifdef _WEBSERVER_TEMPLATE_DEBUG_

static void printFoundEngineFunction(FUNCTION_PARAS* func) {
	char buffer[100];
	switch(func->function) {
		case TEMPLATE_PLATFORM_FUNCTION:
		sprintf(buffer,"TEMPLATE_PLATFORM_FUNCTION");
		break;
		case TEMPLATE_BUILDIN_FUNCTION:
		sprintf(buffer,"TEMPLATE_BUILDIN_FUNCTION");
		break;
		case TEMPLATE_IF:
		sprintf(buffer,"TEMPLATE_IF");
		break;

		case TEMPLATE_INCLUDE_FILE:
		sprintf(buffer,"TEMPLATE_INCLUDE_FILE");
		break;
		case TEMPLATE_ECHO_OFF:
		sprintf(buffer,"TEMPLATE_ECHO_OFF");
		break;
		case TEMPLATE_ECHO_ON:
		sprintf(buffer,"TEMPLATE_ECHO_ON");
		break;

		case TEMPLATE_GET_VARIABLE:
		sprintf(buffer,"TEMPLATE_GET_VARIABLE");
		break;
		case TEMPLATE_SET_VARIABLE:
		sprintf(buffer,"TEMPLATE_SET_VARIABLE");
		break;

		case TEMPLATE_UNKNOWN:
		sprintf(buffer,"TEMPLATE_UNKNOWN");
		break;
		default:
		sprintf(buffer,"TEMPLATE FUNCTION ERROR");
		break;
	}
	LOG (TEMPLATE_LOG,NOTICE_LEVEL,0, "Engine found : %s %s",buffer, func->parameter[0].text ? func->parameter[0].text : "(null)" );
}

#endif

/*int __attribute__((optimize("O0"))) */



int processHTML(http_request* s, const char* prefix, const char *pagename, const char *pagedata, int datalenght) {
	int i, last_chunk_send_pos;
	int end_pos;
	int function_start_pos;
	int function_end_pos;
	int function_length;
	char return_found = 0;


	template_engine_start(s);
	s->engine_current->prefix = (char*) prefix;
	s->engine_current->pagename = (char*) pagename;
	s->engine_current->pagedata = (char*) pagedata;
	s->engine_current->datalenght = datalenght;

	if ( 0 == memcmp( template_v1_header, pagedata, sizeof( template_v1_header ) -1 ) ){
		pagedata += sizeof( template_v1_header );
		datalenght -= sizeof( template_v1_header );
		//printf("Engine Template V1 Header found\n");
	}

	last_chunk_send_pos = 0;
	for (i = 0; i < datalenght; i++) {

		if (pagedata[i] == '{') {
			function_start_pos = i;
			end_pos = i;

			while ((end_pos < datalenght) && (pagedata[end_pos++] != '}')){}
			if (pagedata[end_pos - 1] != '}') {
				continue;
			}

			function_end_pos = end_pos - 1;
			function_length = function_end_pos - function_start_pos;
			parseFunction(s->engine_current, &pagedata[function_start_pos], function_length);

			if (s->engine_current->func.function == TEMPLATE_UNKNOWN) {		/* JS Klammer ignorieren */
				freeFunction(s->engine_current);
				i++;
				continue;
			}

			if (last_chunk_send_pos != i) {
				if (i > last_chunk_send_pos) {
					sendHTMLChunk(s->socket, (char*) &pagedata[last_chunk_send_pos], i - last_chunk_send_pos);
				}
			}

			if ((s->socket->enable_print_funcs == 1) && ( ! ( (s->engine_current->func.function == TEMPLATE_ECHO_FUNCS_ON) || (s->engine_current->func.function == TEMPLATE_ECHO_FUNCS_OFF ) ) ) ) {
				sendHTMLChunk(s->socket, s->socket->print_func_prefix, strlen(s->socket->print_func_prefix));
				sendHTMLChunk(s->socket, &pagedata[function_start_pos], function_length +1);	/* {} mit ausgeben */
				sendHTMLChunk(s->socket, s->socket->print_func_postfix, strlen(s->socket->print_func_postfix));
			}

			i = end_pos - 1;


#ifdef _WEBSERVER_TEMPLATE_DEBUG_
			printFoundEngineFunction(&s->engine_current->func);
#endif


			switch (s->engine_current->func.function) {

			case TEMPLATE_BUILDIN_FUNCTION:
				engine_platformFunction(s, &s->engine_current->func);
				last_chunk_send_pos = i + 1;
				break;
			case TEMPLATE_PLATFORM_FUNCTION:
				engine_platformFunction(s, &s->engine_current->func);
				last_chunk_send_pos = i + 1;
				break;
			case TEMPLATE_INCLUDE_FILE:
				engine_includeFile(s, prefix, &s->engine_current->func);
				last_chunk_send_pos = i + 1;
				break;

			case TEMPLATE_GET_VARIABLE:
				engine_getVariable(s, &s->engine_current->func);
				last_chunk_send_pos = i + 1;
				break;
			case TEMPLATE_SET_VARIABLE:
				engine_setVariable(s, &s->engine_current->func);
				last_chunk_send_pos = i + 1;
				break;
			case TEMPLATE_LOOP_ARRAY:
				engine_loop_array(s, prefix, pagename, &pagedata[i + 1], datalenght - i, &s->engine_current->func, &i);
				last_chunk_send_pos = i + 1;
				break;

			case TEMPLATE_RETURN:
				i = datalenght;
				return_found = 1;
				last_chunk_send_pos = datalenght;
				s->engine_current->return_found = 1;
				break;

			case TEMPLATE_ECHO_OFF:
				s->socket->disable_output = 1;
				last_chunk_send_pos = i + 1;
				break;
			case TEMPLATE_ECHO_ON:
				s->socket->disable_output = 0;
				last_chunk_send_pos = i + 1;
				break;

			case TEMPLATE_ECHO_FUNCS_OFF:
				s->socket->enable_print_funcs = 0;
				last_chunk_send_pos = i + 1;
				break;
			case TEMPLATE_ECHO_FUNCS_ON:
				s->socket->enable_print_funcs = 1;

				if (s->engine_current->func.parameter[0].text != 0){
					strncpy(s->socket->print_func_prefix, s->engine_current->func.parameter[0].text, 49);
					s->socket->print_func_prefix[49] = 0;
				}else{
					strncpy(s->socket->print_func_prefix, "", 50);
				}

				if (s->engine_current->func.parameter[1].text != 0){
					strncpy(s->socket->print_func_postfix, s->engine_current->func.parameter[1].text, 49);
					s->socket->print_func_postfix[49] = 0;
				}else{
					strncpy(s->socket->print_func_postfix, "", 50);
				}

				last_chunk_send_pos = i + 1;
				break;

			case TEMPLATE_IF:
				i++;
				engine_TemplateIF(s, prefix, pagename, &pagedata[i], datalenght - i, &s->engine_current->func, &i);
				last_chunk_send_pos = i;
				if ( s->engine_current->return_found == 1 ){
					i = datalenght;
					last_chunk_send_pos = datalenght;
					return_found = 1;
				}
				break;

			default:
				break;
			}

			freeFunction(s->engine_current);

		}
	}

	/* bei einem if bei dem  hinter dem {else} keine bytes mehr sind springt auf  datalenght + 1 */
	if (i > (datalenght + 1)) {
		char* tmp = (char*) WebserverMalloc(datalenght + 1 );
		strncpy(tmp, pagedata, datalenght);
		tmp[datalenght] = '\0';
		LOG(TEMPLATE_LOG, ERROR_LEVEL, s->socket->socket, "Fatal Error i (%d) > datalenght (%d) %s -> %s", i, datalenght, pagename, tmp);
		WebserverFree(tmp);
	}

	sendHTMLChunk(s->socket, (char*) &pagedata[last_chunk_send_pos], datalenght - last_chunk_send_pos);


	template_engine_stop(s);

	if ( ( s->header->Accept_Encoding != 0 ) && ( s->engine_index == 0 ) ){
		//printf("s->engine_index : %d\n",s->engine_index);
		//printf("Accept_Encoding: %s\n",s->header->Accept_Encoding);
		
		if ( 0 != strstr( s->header->Accept_Encoding, "deflate" ) ){

			if ( 1 == isChunkListbigger(&s->socket->html_chunk_list, 1000) ){
				s->socket->use_output_compression = 1;
			}
		}
		
	}


	return return_found;
}

/*
#pragma GCC pop_options
http://ctpp.havoc.ru/doc/en/
*/

