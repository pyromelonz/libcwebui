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






void engine_includeFile(http_request *s, const char* prefix, FUNCTION_PARAS* func) {
	WebserverFileInfo *file;

	if ( func->parameter[0].text == 0 ){
		return;
	}

	file = getFile(func->parameter[0].text);
	if (file == 0) {
		#ifdef _WEBSERVER_TEMPLATE_DEBUG_
		WebServerPrintf("File : %s not found\n",func->parameter[0].text);
		#endif
		printHTMLChunk(s->socket, "<font color=\"#FF0000\" >include %s not found</font>", func->parameter[0].text);
		return;
	}

	if ( 0 == prepare_file_content( file ) ){
		#ifdef _WEBSERVER_TEMPLATE_DEBUG_
		WebServerPrintf("File : %s error loading content \n",func->parameter[0].text);
		#endif

		printHTMLChunk(s->socket, "<font color=\"#FF0000\" >include %s read error</font>", func->parameter[0].text);
		return;
	}

	if ( file->TemplateFile == 0){
		printf("Warning Engine Template Header <%s> not found: %s \n",template_v1_header,file->FilePath);
	}

	processHTML(s, prefix, file->Url, (char*) file->Data, file->DataLenght);

	release_file_content( file );

}


#if 0
void engine_printWSVar(http_request *s, ws_variable* var) {
	ws_variable *tmp;
	switch (var->type) {
	case VAR_TYPE_STRING:
		sendHTMLChunk(s->socket, var->val.value_string, var->extra.str_len);
		break;
	case VAR_TYPE_INT:
		printHTMLChunk(s->socket, "%d", var->val.value_int);
		break;
	case VAR_TYPE_ARRAY:
		printHTMLChunk(s->socket, "%s Array", var->name);
		break;
	case VAR_TYPE_REF:
		tmp = var->val.value_ref;
		engine_printWSVar(s, tmp);
		break;
	default:
		printHTMLChunk(s->socket, "<font color=\"#FF0000\" >engine_printWSVar : %s Unsuportet Type %d</font> ", var->name, var->type);
		break;
	}
}
#endif

void engine_getVariable(http_request *s, FUNCTION_PARAS* func) {
	ws_variable* var = parseVariable(s, func->parameter[0].text);
	sendHTMLChunkVariable(s->socket, var);
	freeWSVariable(var);
}

void engine_setVariable(http_request *s, FUNCTION_PARAS* func) {
	ws_variable* var = parseVariable(s, func->parameter[0].text);
	ws_variable* var2;
	ws_variable* set_value_var;

	if ( var == 0 ){
		return;
	}

	set_value_var = parseVariable(s, func->parameter[1].text);

	if ( set_value_var == 0){
		freeWSVariable( var );
		return;
	}

	if (var->type != VAR_TYPE_REF) {
		var2 = getRenderVariable(s, var->name);
		freeWSVariable(var);
		var = var2;
		switch (set_value_var->type) {
		case VAR_TYPE_STRING:
			setWSVariableString(var, set_value_var->val.value_string);
			break;
		case VAR_TYPE_INT:
			setWSVariableInt(var, set_value_var->val.value_int);
			break;
		default:
			printHTMLChunk(s->socket,"<font color=\"#FF0000\" >engine_setVariable Typ %d noch behandeln</font>",set_value_var->type);
			break;
		}
		freeWSVariable(set_value_var);

		return;
	} else {
		switch (set_value_var->type) {
		case VAR_TYPE_STRING:
			setWSVariableString(var->val.value_ref, set_value_var->val.value_string);
			break;
		case VAR_TYPE_INT:
			setWSVariableInt(var->val.value_ref, set_value_var->val.value_int);
			break;
		default:
			printf("engine_setVariable Typ %d noch behandeln\n",set_value_var->type);
			break;
		}
		freeWSVariable(set_value_var);
		freeWSVariable(var);
		return;
	}
}

