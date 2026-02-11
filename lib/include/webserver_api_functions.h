/*

SPDX-License-Identifier: MPL-2.0

 libCWebUI
 Copyright (C) 2007 - 2019  Ramin Seyed-Moussavi

 Projekt URL : https://github.com/lordrasmus/libcwebui

 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at https://mozilla.org/MPL/2.0/. 

*/


#ifndef _WEBSERVER_API_FUNCTIONS_
#define _WEBSERVER_API_FUNCTIONS_


#include "WebserverConfig.h"
#include "webserver_api_functions_macros.h"

#if __GNUC__ > 2
	#define NEED_RESUL_CHECK __attribute__((warn_unused_result))
#else
	#define NEED_RESUL_CHECK
#endif



/* Authentifizierung über Cookies */
#define NORMAL_CHECK_OK			( 1 )
#define SSL_CHECK_OK			( 2 )
#define NOT_REGISTERED			( 0 )
#define SESSION_MISMATCH_ERROR 	( -1 )

/* Session Status */
#define SESSION_NOT_REGISTERED 			( 0 )
#define SESSION_REGISTERED			    ( 1 )


/* Session Store Typen */
#define STORE_NORMAL			( 1 )
#define STORE_SSL			    ( 2 )

/* Condition Rückgabewerte */
#define COND_TRUE			    ( 1 )
#define COND_FALSE		     	( 2 )
#define COND_ERROR		     	( 3 )

/* Websocket Signale */
#define WEBSOCKET_CONNECT 		( 1 )
#define WEBSOCKET_MSG 			( 2 )
#define WEBSOCKET_DISCONNECT 		( 3 )



#define WEBSERVER_API 6

#ifdef __cplusplus
extern "C" {
#endif


#ifndef WebserverMalloc


	#ifdef _WEBSERVER_MEMORY_DEBUG_
		void*	real_WebserverMalloc( const unsigned long size, const char* function, const char* filename, int fileline);
		#define WebserverMalloc(a) real_WebserverMalloc(a,__FUNCTION__,(char*)__BASE_FILE__, __LINE__)
	#else
		void*   real_WebserverMalloc( const unsigned long size );
		#define WebserverMalloc(a) real_WebserverMalloc(a)
	#endif

	void	WebserverFree( void *mem );

#endif



/***********************************************************************************************
*                                                                                              *
*           Ausgabe Funktionen                                                                 *
*                                                                                              *
************************************************************************************************/

/*
 		Variable in den Ausgabe Buffer schreiben
*/
void sendHTMLVariable(dummy_handler* s,dummy_var* var);

/*
		printf Funktion die in der Ausgabe Buffer schreibt
*/
#ifdef __GNUC__
	void printHTML(dummy_handler* s,const char *fmt,...) __attribute__((format(printf, 2, 3)));
#else
	void printHTML(dummy_handler* s, const char *fmt, ...);
#endif

/*
		Buffer in den Ausgabe Buffer kopieren
*/
void sendHTML(dummy_handler* s, const char* text, const unsigned int length);

/*
		Custom Response Header setzen (für WS_FILE_TYPE_CUSTOM)
		Muss vor sendHTML() aufgerufen werden.
*/
void ws_set_response_header(dummy_handler* s, const char* name, const char* value);
void ws_set_response_code(dummy_handler* s, int code);


/*
		Globalen Variablen locken / unlocken
*/
int NEED_RESUL_CHECK lockGlobalVars(void);
int NEED_RESUL_CHECK unlockGlobalVars(void);


/***********************************************************************************************
*                                                                                              *
*           Variablen verwalten                                                                *
*                                                                                              *
************************************************************************************************/

typedef enum {
	NO_FLAGS      = 0x00,
	DO_NOT_CREATE = 0x01
}WS_VAR_FLAGS;

char* 	WebsocketGetStoreGUID(char* guid);
long 	ws_get_websocket_store_timeout( char* guid );

char setSessionVarGUID(char* store_guid, int store, const char* name, const char* value);

dummy_var* getSessionVarGUID(char* store_guid, int store, const char* name,WS_VAR_FLAGS flags);

char* getSessionGUID(dummy_handler* s);

/*
		Session Variable setzen

		store = STORE_NORMAL ] STORE_SSL
*/
char setSessionVar(dummy_handler* s,int store,const char* name,const char* value);

/*
		Session Variable holen

		store = STORE_NORMAL ] STORE_SSL
*/
dummy_var* getSessionVar(dummy_handler* s,int store,const char* name,WS_VAR_FLAGS flags);


dummy_var* 	ws_get_render_var(dummy_handler* s,const char* name,WS_VAR_FLAGS flags);

void 		ws_set_render_var(dummy_handler* s,char* name,char* text);
#ifdef __GNUC__
	void 		setRenderVar_v (dummy_handler* s, char* name, char* format,...) __attribute__((format(printf, 3, 4)));
#else
	void 		setRenderVar_v(dummy_handler* s, char* name, char* format, ...);
#endif




/*
		Glole Variable setzen
*/
dummy_var* setGlobalVar(const char* name,const char* text);


/*
		Glole Variable holen
*/
dummy_var* getGlobalVar(const char* name,WS_VAR_FLAGS flags);


/***********************************************************************************************
*                                                                                              *
*           Variablen lesen / schreiben                                                        *
*                                                                                              *
************************************************************************************************/

/*
		Variable zu einem Array machen
*/
void setVariableToArray(dummy_var* var);

/*
		Neue Variable in einem Array erzeugen
*/

#define NO_CHECK_NAME_EXISTS 1

dummy_var* addToVarArray(dummy_var* var, const char* name);
dummy_var* addToVarArray2(dummy_var* var, const char* name, uint32_t flags);
dummy_var* addToVarArrayCustomData(dummy_var* var, const char* name,free_handler handle, void* data );

void addToVarArrayInt(dummy_var* var, const char* name, int value );
void addToVarArrayStr(dummy_var* var, const char* name, char* buffer );

void* getVariableAsCustomData(dummy_var* var);

/*
		Variablen Inhalt als String lesen
*/
int   getVariableAsString(dummy_var* var, char* buffer,unsigned int buffer_length);

/*
		Internen Variablen String Pointer lese

      ACHTUNG : Pointer nicht speichern da die Variablen vom webserver gelöscht werden können
*/
char* getVariableAsStringP(dummy_var* var);

/*
		Variablen Inhalt als String schreiben
*/
void setVariableAsString(dummy_var* var, const char* text);

/*
		Variablen Inhalt als Int lesen
*/
int getVariableAsInt(dummy_var* var);
uint64_t getVariableAsULong(dummy_var* var);
int64_t  getVariableAsLong(dummy_var* var);
/*
		Variablen Inhalt als Int schreiben
*/
void setVariableAsInt(dummy_var* var, int value);
void setVariableAsULong(dummy_var* var, uint64_t value);
void setVariableAsLong(dummy_var* var, int64_t value);

void setVariableToRef(dummy_var* var,dummy_var* ref);

dummy_var*	getArrayVariable(dummy_var* var, const char* name);

dummy_var*	getArrayFirst(dummy_var* var);
dummy_var*	getArrayNext(dummy_var* var);
void 		stopArrayIterate(dummy_var* var);


/***********************************************************************************************
*                                                                                              *
*          Session Authentifizierung                                                           *
*                                                                                              *
************************************************************************************************/

/*
		Session Registrierungs Status einstellen

		status = REGISTERED  |  NOT_REGISTERED
*/
char setUserRegisterStatus(dummy_handler* s,char status);

/*
      Session Register Status abrufen

      ret = NORMAL_CHECK_OK | SSL_CHECK_OK | NOT_REGISTERED | SESSION_MISMATCH_ERROR
*/
int checkUserRegisterStatus(dummy_handler* s);

/***********************************************************************************************
*                                                                                              *
*          POST File Upload                                                                    *
*                                                                                              *
************************************************************************************************/

int    getFileCount(dummy_handler* s);
char*  getFileName(dummy_handler* s,int index);
char*  getFileData(dummy_handler* s,int index);
int    getFileSize(dummy_handler* s,int index);
char*  getPostData(dummy_handler* s);
int    getPostSize(dummy_handler* s);


/***********************************************************************************************
*                                                                                              *
*          Request Status Informationen                                                        *
*                                                                                              *
************************************************************************************************/

/*
      URL des Requests
*/
const char *getRequestURL(dummy_handler* s);

/*
      Host des Requests
*/
const char *getRequestHost(dummy_handler* s);

/*
      Ob der Request über eine SSL Verbindung kam
*/
char isRequestSecure(dummy_handler *s);
char isRequestHttp1_1(dummy_handler *s);

/*
      GET Parameter des Requests abfragen
*/
dummy_var* getURLParameter(dummy_handler* s,const char* name);

/*
      HTTP Header Zeilen abfragen
*/
const char* getRequestHeader(dummy_handler *s, char* name);

/*
      Parameter die in der Templateengine mit {f:<name>:para1:para2:...} uebergeben wurden
*/
char* getEngineParameter(dummy_handler* s,int index);
void  dumpEngineParameter(dummy_handler* s);


/*
	String auf UTF-8 testen
*/
int ws_check_utf8( unsigned char *str, uint32_t len, char **message);

/*
	decode URL String 
*/
void ws_url_decode(char *line);

/*
      Text Frame über einen Websocket senden
*/
int WebsocketSendTextFrame(const char* guid, const char* in, const int length);
int WebsocketSendBinaryFrame(const char* guid, const char* in, const int length);


/*
      Close Frame über einen Websocket senden
*/
int WebsocketSendCloseFrame(const char* guid);

/*
      Dir als /<alias>/ im Webserver einblenden

      use_cache = files im ram cachen
      use_auth  = nur mit authentifizierung sichtbar
*/
void ws_add_dir(const char* alias,const char* dir, const int use_cache, const int use_auth );



void WebserverAddBinaryData(const unsigned char* data);

/*
		Datei <path> als Webserver Plugin laden
*/
int WebserverLoadPlugin(const char* name,const char* path);


/*

		Regeln um einzustellen was als Template behandelt werden soll
*/

void WebserverAddTemplateFilePostfix(const char* postfix);
void WebserverAddTemplateIgnoreFilePostfix(const char* postfix);


/* Urls die nicht im RAM gecached werden soll */
void WebserverAddNoRamCacheFile( const char* url );


/* Urls die nicht sichtbar sein sellen */
void WebserverAddBlockedFile( const char* url );

void RegisterEngineFunction(const char* name,user_api_function f,const char* file,int line);
void RegisterEngineCondition(const char* name,user_api_condition f,const char* file,int line);
void RegisterWebsocketHandler(const char* url,websocket_api_handler f,const char* file,int line);

/***********************************************************************************************
*                                                                                              *
*          WebSocket Streaming API                                                             *
*                                                                                              *
************************************************************************************************/

#ifdef WEBSERVER_USE_WEBSOCKETS

/**
 * Streaming context for large WebSocket frames.
 * Passed to all streaming callbacks.
 */
typedef struct websocket_stream_context {
    char* guid;                    /* WebSocket Connection GUID */
    char* url;                     /* Request URL */
    uint64_t total_length;         /* Total frame length (from header) */
    uint64_t received_length;      /* Bytes received so far */
    char is_binary;                /* Binary or Text frame */
    char aborted;                  /* Handler aborted the stream */
    void* user_data;               /* Handler-specific data */
} websocket_stream_context;

/**
 * Streaming callback types
 */
typedef void (*ws_stream_start_handler)(websocket_stream_context* ctx);
typedef void (*ws_stream_chunk_handler)(websocket_stream_context* ctx,
                                         const unsigned char* data,
                                         uint32_t length);
typedef void (*ws_stream_end_handler)(websocket_stream_context* ctx,
                                       int success);  /* success=0 on error/abort */

/**
 * Register a streaming handler for a WebSocket URL.
 * When registered, ALL frames on this URL use streaming (no threshold).
 */
void RegisterWebsocketStreamHandler(
    const char* url,
    ws_stream_start_handler on_start,
    ws_stream_chunk_handler on_chunk,
    ws_stream_end_handler on_end
);

/**
 * Helper functions for streaming handlers
 */
void WebsocketStreamSetUserData(websocket_stream_context* ctx, void* data);
void* WebsocketStreamGetUserData(websocket_stream_context* ctx);

/**
 * Abort the current stream (e.g., on handler error)
 */
void WebsocketStreamAbort(websocket_stream_context* ctx);

#endif /* WEBSERVER_USE_WEBSOCKETS */

void WebserverRegisterPluginErrorHandler(plugin_error_handler f);


int  WebserverInit(void);
void WebserverStart(void);
void WebserverShutdown(void);
void WebserverShutdownHandler(void);

void WebserverConfigSetInt(const char* name, int value);
int  WebserverConfigGetInt(const char* name);
void WebserverConfigSetText(const char* name, const char* text);
void WebserverConfigGetText(const char* name, char* text, int text_size);

void WebserverInjectExternFD(int fd, extern_handler handle );


void WebserverSetPostHandler( post_handler handler );


void ws_generate_guid(char* buf, int length);

typedef enum {
	WS_FILE_TYPE_PLAIN,
	WS_FILE_TYPE_HTML,
	WS_FILE_TYPE_CSS,
	WS_FILE_TYPE_JS,
	WS_FILE_TYPE_XML,
	WS_FILE_TYPE_XSL,
	WS_FILE_TYPE_SVG,
	WS_FILE_TYPE_JSON,
	WS_FILE_TYPE_CUSTOM   /* Custom headers set via setResponseHeader() */

} WS_FILE_TYPES;

typedef WS_FILE_TYPES (*url_handler_func)( dummy_handler* s, const char* url );
void ws_register_url_function( char* url, url_handler_func func );

void ws_reverse_proxy_register_uds( const char* method, const char* url, const char* path );


/*
 *		Python API
 *
 */


void WebserverInitPython( void );
int WebserverLoadPyPlugin( const char* path );


/*
 *		CORS API
 */

void  ws_set_cors_handler( cors_handler handler );
char* ws_get_cors_type_name( CORS_HEADER_TYPES type );

#include "webserver_api_functions_depricated.h"


#ifdef __cplusplus
}
#endif

#endif
