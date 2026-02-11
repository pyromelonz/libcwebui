/*

SPDX-License-Identifier: MPL-2.0

 libCWebUI
 Copyright (C) 2007 - 2019  Ramin Seyed-Moussavi

 Projekt URL : https://github.com/lordrasmus/libcwebui

 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at https://mozilla.org/MPL/2.0/. 

*/


/**
 *  @file	webserver_api_functions.c
 *  @brief	external API
 *  @details 	external API Functions<br>
 * 				<br>
 * 				dummy_handler is an internel pointer, the underlaying struct should !! NEVER !! be use directly<br>
 *              the pointer is passed to user funtions in the DEFINE_FUNCTION macro as s
 */


#include "webserver.h"

#include "intern/system_file_access.h"
#include "intern/reverse_proxy.h"
#include "is_utf8.h"



#ifdef DMALLOC
#include <dmalloc/dmalloc.h>
#endif

#if __GNUC__ > 2
	#pragma GCC visibility push(default)
#endif




static void register_engine_function(const char* name, user_api_function f, const char* file, int line) {
	/* void register_function(const char* name,user_function f,const char* file,int line) */
	register_function(name, (user_function) f, file, line);
}

static void register_engine_condition(const char* name, user_api_condition f,	const char* file, int line) {
	/* void register_condition(const char* name,user_condition f,const char* file,int line) */
	register_condition(name, (user_condition) f, file, line);
}

static void register_websocket_handler(const char* url, websocket_api_handler f, const char* file, int line) {
	/* void register_function_websocket_handler(const char* url,websocket_handler f,const char* file,int line) */
	register_function_websocket_handler(url, (websocket_handler) f, file, line);
}

/*****************************************************************************
 *
 *						Ausgabe Funktionen
 *
 *****************************************************************************/

void sendHTMLVariable(dummy_handler* s, dummy_var* var) {
	sendHTMLChunkVariable(((http_request*) s)->socket, (ws_variable*) var);
}

/* printHTMLChunk ( socket_info* sock,const char *fmt,... ) */
void printHTML(dummy_handler* s, const char *fmt, ...) {
	va_list arg;
	va_start ( arg, fmt );
	vprintHTMLChunk(((http_request*)s)->socket,fmt,arg);
	va_end ( arg );
}

void sendHTML(dummy_handler* s, const char* text, const unsigned int length){
	/* void sendHTMLChunk(socket_info* sock, const char* text, const unsigned int length) */
	sendHTMLChunk( ((http_request*) s)->socket, text, length);
}

void ws_set_response_header(dummy_handler* s, const char* name, const char* value){
	http_request* req = (http_request*) s;
	custom_response_header* header = WebserverMalloc(sizeof(custom_response_header));
	header->name = WebserverMalloc(strlen(name) + 1);
	strcpy(header->name, name);
	header->value = WebserverMalloc(strlen(value) + 1);
	strcpy(header->value, value);
	ws_list_append(&req->custom_response_headers, header, 0);
}

void ws_set_response_code(dummy_handler* s, int code){
	http_request* req = (http_request*) s;
	req->response_code = code;
}


/*****************************************************************************
 *
 *						Variablen
 *
 *****************************************************************************/



char* WebsocketGetStoreGUID(char* guid){
	#ifdef WEBSERVER_USE_WEBSOCKETS
	return getWebsocketStoreGUID(guid);
	#else
	return "";
	#endif
}

long ws_get_websocket_store_timeout( char* guid ){
	#ifdef WEBSERVER_USE_WEBSOCKETS
		return getWebsocketStoreTimeout( guid );
	#else
		return -1;
	#endif
}


char setSessionVarGUID(char* store_guid, int store, const char* name, const char* value){
	return setSessionValueByGUID(store_guid, (STORE_TYPES)store, name, value);
}

dummy_var* getSessionVarGUID(char* store_guid, int store, const char* name, UNUSED_PARA WS_VAR_FLAGS flags){
	return (dummy_var*) getSessionValueByGUID(store_guid, (STORE_TYPES)store, name);
}

char* getSessionGUID(dummy_handler* s){
	return ((http_request*) s)->guid;

}


char setSessionVar(dummy_handler* s, int store, const char* name, const char* value) {
	/* char setSessionValue(http_request* s,STORE_TYPES store,const char* name,const char* value) */
	return setSessionValue((http_request*) s, (STORE_TYPES) store, name, value);
}

dummy_var* getSessionVar(dummy_handler* s, int store, const char* name,UNUSED_PARA WS_VAR_FLAGS flags) {
	/* ws_variable* getSessionValue(http_request* s,STORE_TYPES store,const char* name) */
	ws_variable* ret = getSessionValue((http_request*) s, (STORE_TYPES) store,	name);

	if ( (  flags & DO_NOT_CREATE ) == 0 ){
		if (ret == 0){
			ret = addSessionValue((http_request*)s,(STORE_TYPES) store, name);
		}
	}
	return (dummy_var*) ret;
}

/**
* @brief gets a render variable
*
* @param dummy_handler [in] internal context handler
* @param name [in] name of the variable
* @param 	flags [in] if set to DO_NOT_CREATE, NULL is returned if variable does not exists<br>
* 			otherwise the variable is created
* @return pointer to the variable
* @details 	this function gets a render variable from the current http request context
*/
dummy_var* ws_get_render_var(dummy_handler* s, const char* name,WS_VAR_FLAGS flags) {
	ws_variable* ret = getVariable(((http_request*)s)->render_var_store,name);
	if ( (  flags & DO_NOT_CREATE ) == 0 ){
		if (ret == 0){
			ret = newVariable(((http_request*)s)->render_var_store,name, 0 );
		}
	}
	return (dummy_var*) ret;
}

/**
* @brief sets a render variable
*
* @param dummy_handler [in] internal context handler
* @param name [in] name of the variable
* @param text [in] value of the variable
* @details 	this function sets a render variable in the current http request context
*/
void ws_set_render_var(dummy_handler* s, char* name, char* text) {
	setRenderVariable((http_request*) s, name, text);
}

void setRenderVar_v(dummy_handler* s, char* name, char* format,...) {
	char *buffer;
	va_list arg;

	va_start(arg, format);
	buffer = WebserverMalloc( vsnprintf(0, 0, format, arg) + 1);
	va_end(arg);

	va_start(arg, format);
	vsprintf(buffer, format, arg);
	va_end(arg);

	setRenderVariable((http_request*) s, name, buffer);
	WebserverFree( buffer );
}


dummy_var* getGlobalVar(const char* name,WS_VAR_FLAGS flags) {
	if ( (  flags & DO_NOT_CREATE ) == DO_NOT_CREATE ){
		return (dummy_var*) getExistentGlobalVariable(name);
	}else{
		return (dummy_var*) getGlobalVariable(name);
	}
}

dummy_var* setGlobalVar(const char* name, const char* text) {
	return (dummy_var*) setGlobalVariable(name, text);
}

void setVariableToArray(dummy_var* var) {
	setWSVariableArray((ws_variable*) var);
}

dummy_var* addToVarArray(dummy_var* var, const char* name) {
	return (dummy_var*) addWSVariableArray((ws_variable*) var, name, 0 );
}

dummy_var* addToVarArray2(dummy_var* var, const char* name, uint32_t flags) {
	return (dummy_var*) addWSVariableArray((ws_variable*) var, name, flags );
}

dummy_var* addToVarArrayCustomData(dummy_var* var, const char* name,free_handler handle, void* data ){
	ws_variable* ret = addWSVariableArray((ws_variable*) var, name, 0 );
	setWSVariableCustomData(ret, handle, data);
	return (dummy_var*)ret;
}

void* getVariableAsCustomData(dummy_var* var){
	ws_variable* var2 = (ws_variable*) var;
	return var2->val.value_p;
}

void addToVarArrayInt(dummy_var* var, const char* name,int value){
	dummy_var* var2 = addToVarArray(var, name);
	setVariableAsInt(var2, value);
}

void addToVarArrayStr(dummy_var* var, const char* name, char* buffer ){
	dummy_var* var2 = addToVarArray(var, name);
	setVariableAsString(var2, buffer );
}

int getVariableAsString(dummy_var* var, char* buffer, unsigned int buffer_length) {
	return getWSVariableString((ws_variable*) var, buffer, buffer_length);
}

char* getVariableAsStringP(dummy_var* var){
	ws_variable* var2 = (ws_variable*) var;
	if ( var2->type == VAR_TYPE_STRING ){
		return var2->val.value_string;
	}
	return 0;
}

void setVariableAsString(dummy_var* var, const char* text) {
	setWSVariableString((ws_variable*) var, text);
}


int getVariableAsInt(dummy_var* var) {
	return getWSVariableInt((ws_variable*) var);
}

void setVariableAsInt(dummy_var* var, int value){
	setWSVariableInt((ws_variable*) var, value);
}


uint64_t getVariableAsULong(dummy_var* var){
	return getWSVariableULong((ws_variable*) var);
}

void setVariableAsULong(dummy_var* var, uint64_t value){
	setWSVariableULong((ws_variable*) var, value);
}


int64_t getVariableAsLong(dummy_var* var){
	return getWSVariableLong((ws_variable*) var);
}

void setVariableAsLong(dummy_var* var, int64_t value){
	setWSVariableLong((ws_variable*) var, value);
}


void setVariableToRef(dummy_var* var,dummy_var* ref){
	setWSVariableRef((ws_variable*)var,(ws_variable*)ref);
}

int lockGlobalVars(void){
	return lockGlobals();
}
int unlockGlobalVars(void){
	return unlockGlobals();
}

dummy_var*	getArrayVariable(dummy_var* var, const char* name){
	return (dummy_var*)getWSVariableArray( (ws_variable*) var ,name);
}

dummy_var*	getArrayFirst(dummy_var* var){
	return (dummy_var*)	getWSVariableArrayFirst((ws_variable*) var);

}
dummy_var*	getArrayNext(dummy_var* var){
	return (dummy_var*) getWSVariableArrayNext((ws_variable*) var);

}
void 		stopArrayIterate(dummy_var* var){
	stopWSVariableArrayIterate((ws_variable*) var);
}



/*****************************************************************************
 *
 *						Authentifizierung über Cookies
 *
 *****************************************************************************/

char setUserRegisterStatus(dummy_handler* s, char status) {
	/* char setUserRegistered(http_request* s,char status) */
	return setUserRegistered((http_request*) s, status);
}

int checkUserRegisterStatus(dummy_handler* s) {
	/* int checkUserRegistered(http_request* s)  */
	return checkUserRegistered((http_request*) s);
}

/***********************************************************************************************
*                                                                                              *
*          POST File Upload                                                                    *
*                                                                                              *
************************************************************************************************/

int  getFileCount(dummy_handler* s){
	return ws_list_size( &((http_request*) s)->upload_files);
}

char*     getFileName(dummy_handler* s,int index){
	upload_file_info* tmp = ws_list_get_at( &((http_request*) s)->upload_files, index );
	if ( ! tmp ){
		return 0;
	}
	return tmp->name;
}

char*     getFileData(dummy_handler* s,int index){
	upload_file_info* tmp = ws_list_get_at( &((http_request*) s)->upload_files, index );
	if ( ! tmp ){
		return 0;
	}
	return tmp->data;
}

int     getFileSize(dummy_handler* s,int index){
	upload_file_info* tmp = ws_list_get_at( &((http_request*) s)->upload_files, index );
	if ( ! tmp ){
		return 0;
	}
	return tmp->length;
}

char*     getPostData(dummy_handler* s){
	return ((http_request*) s)->header->post_buffer;
}

int    getPostSize(dummy_handler* s){
	return ((http_request*) s)->header->contentlenght;
}

/*****************************************************************************
 *
 *						Request Status Informationen
 *
 *****************************************************************************/

const char *getRequestURL(dummy_handler* s) {
	http_request* s1 = (http_request*) s;
	return s1->header->url;
}

const char *getRequestHost(dummy_handler* s) {
	http_request* s1 = (http_request*) s;
	return s1->header->Host;
}

char isRequestSecure(dummy_handler *s) {
#ifdef WEBSERVER_USE_SSL
	return ((http_request*) s)->socket->use_ssl;
#else
	return 0;
#endif
}

char isRequestHttp1_1(dummy_handler *s) {
	return ((http_request*) s)->header->isHttp1_1;
}

dummy_var* getURLParameter(dummy_handler* s,const char* name) {
	return (dummy_var*) getParameter((http_request*) s, name);
}

const char* getRequestHeader(dummy_handler *s, char* name){
	http_request* sess = (http_request*) s;
	
	HttpRequestHeader* header = sess->header;
	
	
	if( 0 == strcmp( name, "Host" ) ){			return header->Host;			}
	if( 0 == strcmp( name, "User-Agent" ) ){	return header->UserAgent;		}
	if( 0 == strcmp( name, "Accept" ) ){		return header->Accept;			}
	if( 0 == strcmp( name, "Authorization" ) ){	return header->Authorization;	}
	if( 0 == strcmp( name, "Origin" ) ){		return header->Origin;			}
	if( 0 == strcmp( name, "Connection" ) ){	return header->Connection;		}
	if( 0 == strcmp( name, "Upgrade" ) ){		return header->Upgrade;			}
	if( 0 == strcmp( name, "Referer" ) ){		return header->Referer;			}
	
	
	return 0;
	
	
}

char* getEngineParameter(dummy_handler* s, int index) {
	FUNCTION_PARAS* f = &((http_request*)s)->engine_current->func;
	if ( index >= MAX_FUNC_PARAS ){
		return 0;
	}

	return f->parameter[index].text;
}

void  dumpEngineParameter(dummy_handler* s){
	int i;
	for( i = 0; i < MAX_FUNC_PARAS ; i++ ){

		if (getEngineParameter(s, i) != 0){
			printHTML(s,"Para%d : %s<br>", i, getEngineParameter(s, i));
		}
	}
}

int ws_check_utf8( unsigned char *str, uint32_t len, char **message){
	return  is_utf8( str, len, message);
}

int WebsocketSendTextFrame(const char* guid, const char* in, const int length) {
#ifdef WEBSERVER_USE_WEBSOCKETS
	return sendWebsocketTextFrame(guid, in, length);
#else
	return 0;
#endif
}

int WebsocketSendBinaryFrame(const char* guid, const char* in, const int length) {
#ifdef WEBSERVER_USE_WEBSOCKETS
	return sendWebsocketBinaryFrame(guid, (const unsigned char*)in, length);
#else
	return 0;
#endif
}

int WebsocketSendCloseFrame(const char* guid){
	#ifdef WEBSERVER_USE_WEBSOCKETS
	sendCloseFrame2(guid);
	#endif
	return 0;
}

int  WebserverLoadPlugin(const char* name,const char* path){
	return loadPlugin(name,path);
}

void RegisterEngineFunction(const char* name,user_api_function f,const char* file,int line){
	register_engine_function(name,f,file,line);
}
void RegisterEngineCondition(const char* name,user_api_condition f,const char* file,int line){
	register_engine_condition(name,f,file,line);
}
void RegisterWebsocketHandler(const char* url,websocket_api_handler f,const char* file,int line){
	register_websocket_handler(url,f,file,line);
}

void ws_add_dir(const char* alias,const char* dir, const int use_cache, const int use_auth ){
	add_local_file_system_dir(alias,dir,use_cache,use_auth);
}



void WebserverAddBinaryData(const unsigned char* data){
	#ifdef WEBSERVER_USE_BINARY_FORMAT
	read_binary_data( data );
	#endif
}


void WebserverAddTemplateFilePostfix(const char* postfix){
	#ifndef DISABLE_OLD_TEMPLATE_SYSTEM
		addTemplateFilePostfix(postfix);
	#else
		printf("WebserverAddTemplateFilePostfix disabled\n");
	#endif
}

void WebserverAddTemplateIgnoreFilePostfix(const char* postfix){
	#ifndef DISABLE_OLD_TEMPLATE_SYSTEM
		addTemplateIgnoreFilePostfix(postfix);
	#else
		printf("WebserverAddTemplateIgnoreFilePostfix disabled\n");
	#endif
}


int WebserverInit(void){
	if(initWebserver() != 0){
		/* asm (".symver WebserverShutdownHandler1__,WebserverShutdownHandler@@VERS_1.0"); */
		return 0;
	}
	return -1;
}

void WebserverStart(void){
	startWebServer();
}

void WebserverShutdown(void){
	shutdownWebserver();
}

void WebserverShutdownHandler(void){
	breakEvents();
}



void WebserverConfigSetInt(const char* name,int value){
	setConfigInt(name,value);
}

int  WebserverConfigGetInt(const char* name){
	return getConfigInt(name);
}

void WebserverConfigSetText(const char* name, const char* text){
	setConfigText(name,text);
}

void WebserverConfigGetText(const char* name, char* text, int text_size){
    
    char *value = getConfigText(name);
    
    snprintf( text, text_size, "%s", value );
}

void WebserverRegisterPluginErrorHandler(plugin_error_handler f){
	RegisterPluginErrorHandler(f);
}

void WebserverInjectExternFD(int fd, extern_handler handle ){
	socket_info* sock = WebserverMallocSocketInfo();
	sock->extern_handle = handle;
	sock->socket = fd;
	addEventSocketReadPersist(sock);
}


extern post_handler post_handle_func;

void WebserverSetPostHandler( post_handler handler ){
	post_handle_func = handler;
}

/*
 *
 *		Utils
 *
 */

void ws_generate_guid(char* buf, int length){
	generateGUID(buf,length);
}

/*
 *		Python API
 *
 */

#ifdef WEBSERVER_USE_PYTHON


void WebserverInitPython( void ){
	py_init_modules();
}

int WebserverLoadPyPlugin( const char* path ){
	return py_load_python_plugin( path );
}

#endif

/*
 *		CORS API
 */

void ws_set_cors_handler( cors_handler handler ){
	setCORS_Handler( handler );
}

char* ws_get_cors_type_name( CORS_HEADER_TYPES type ){
	return get_cors_type_name( type );
}

void ws_url_decode(char *line){
	url_decode( line );
}

void ws_register_url_function( char* url, url_handler_func func ){
	register_url_function( url, func );
}


/*
 *		Reverse Proxy API
 */

void ws_reverse_proxy_register_uds(const char* method, const char* url_prefix, const char* backend_path){
	reverse_proxy_register_uds_internal( method, url_prefix, backend_path );
}


#include "webserver_api_functions_depricated.c"


#if __GNUC__ > 2
	#pragma GCC visibility pop
#endif
