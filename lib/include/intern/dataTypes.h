/*

SPDX-License-Identifier: MPL-2.0

 libCWebUI
 Copyright (C) 2007 - 2019  Ramin Seyed-Moussavi

 Projekt URL : https://github.com/lordrasmus/libcwebui

 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at https://mozilla.org/MPL/2.0/. 

*/

#ifndef _DATATYPES_H_
#define _DATATYPES_H_

#include "platform-defines.h"

#include "linked_list.h"
#include "variable_store.h"

#ifdef WEBSERVER_USE_PYTHON
	#include <Python.h>
#endif

#define FORCE_DOWNLOAD_NAME_LENGTH 200

static const char template_v1_header[] = { "TEMPLATE_V1" };

typedef enum {
	FILE_TYPE_PLAIN, FILE_TYPE_HTML, FILE_TYPE_HTML_INC, FILE_TYPE_CSS, FILE_TYPE_JS,

	FILE_TYPE_XML, FILE_TYPE_XSL,

	FILE_TYPE_ICO, FILE_TYPE_BMP, FILE_TYPE_JPG, FILE_TYPE_PNG, FILE_TYPE_GIF,

	FILE_TYPE_MANIFEST, FILE_TYPE_SVG,

	FILE_TYPE_PDF, FILE_TYPE_JSON, FILE_TYPE_WOFF, FILE_TYPE_EOT, FILE_TYPE_TTF,
	FILE_TYPE_C_SRC,

	FILE_TYPE_CUSTOM,  /* Custom headers via ws_set_response_header() */

	FILE_TYPE_ALL
} FILE_TYPES;

typedef enum {
	EVENT_TIMEOUT = 0x01, EVENT_SIGNAL = 0x02, EVENT_READ = 0x04, EVENT_WRITE = 0x08, EVENT_PERSIST = 0x10
} EVENT_TYPES;

typedef enum {
	TEMPLATE_UNKNOWN = 0x00,
	TEMPLATE_IF = 0x01,

	/*
	TEMPLATE_SESSION_STORAGE  	= 0x02,
	TEMPLATE_GET_RENDER_VARIABLE  	= 0x03,
	TEMPLATE_SET_RENDER_VARIABLE  	= 0x04,
	TEMPLATE_LOOP_RENDER_ARRAY	= 0x05,
	*/

	TEMPLATE_GET_VARIABLE = 0x04,
	TEMPLATE_SET_VARIABLE = 0x05,
	TEMPLATE_LOOP_ARRAY = 0x06,

	/*
	TEMPLATE_GET_GLOBAL_VARIABLE  	= 0x10,
	TEMPLATE_SET_GLOBAL_VARIABLE  	= 0x11,
	TEMPLATE_LOOP_GLOBAL_ARRAY	= 0x12,
	*/

	TEMPLATE_ECHO_OFF = 0x20,
	TEMPLATE_ECHO_ON = 0x21,
	TEMPLATE_ECHO_FUNCS_OFF = 0x22,
	TEMPLATE_ECHO_FUNCS_ON = 0x23,

	TEMPLATE_INCLUDE_FILE = 0x49,
	TEMPLATE_PLATFORM_FUNCTION = 0x50,
	TEMPLATE_BUILDIN_FUNCTION = 0x51,

	TEMPLATE_RETURN = 0xF0
} ENGINE_FUNCTIONS;

typedef struct {
	int connections;
	char use_ssl;
} WebserverConfig;

typedef struct {
	char* text;
	unsigned int length;
} html_chunk;

typedef struct{
	char* text;
	uint16_t length;
} parameter_info;

/* Forward declaration */
typedef struct user_func_s user_func_s;

typedef struct {
	ENGINE_FUNCTIONS function;
	user_func_s        *platform_function;
	parameter_info parameter[MAX_FUNC_PARAS];
	unsigned char  parameter_count;
} FUNCTION_PARAS;

typedef struct {
	char *name;
	char *value;
} Parameter;

typedef struct {
	char *name;
	char *value;
} Cookie;

enum fs_types{
	FS_LOCAL_FILE_SYSTEM,
	FS_BINARY,
	FS_WNFS
};

typedef struct {
	unsigned char Id;

	const char *Url;
	unsigned int UrlLengt;

	const char *FilePath;
	unsigned int FilePathLengt;

	const char *FilePrefix;

	const unsigned char *Data;
	FILE_OFFSET DataLenght;

	FILE_TYPES FileType;

	unsigned char Compressed;
	const unsigned char *CompressedData;
	FILE_OFFSET CompressedDataLenght;
	FILE_OFFSET RealDataLenght;

	unsigned char TemplateFile;
	unsigned char RamCached;

	unsigned char NoRamCache;
	unsigned char ForceDownload;
	unsigned char ForceDownloadName[FORCE_DOWNLOAD_NAME_LENGTH];

	unsigned char auth_only;

	unsigned char Modified;

	char* lastmodified;
	unsigned int lastmodifiedLength;
	const char* etag;
	unsigned char etagLength;

	unsigned long int	last_mod_sec;
	unsigned long int   last_mod_nsec;

	enum fs_types fs_type;

} WebserverFileInfo;




#define MAX_ERROR_METHOD 30
typedef struct {
	char *url;
	short method;
	char error_method[MAX_ERROR_METHOD];
	short error;
	short gzip;
	short deflate;
	uint64_t contentlenght;
	short contenttype;
	char *If_Modified_Since; /* Wed, 12 Dec 2007 13:13:08 GMT */
	char *etag;
	char *Upgrade;
	char *Connection;
	char *Host;
	char *HostName;
	char *Origin;
	char *Access_Control_Request_Method;
	char *Access_Control_Request_Headers;
	char *Accept_Encoding;
	char isHttp1_1;
	
	char* UserAgent;
	char* Accept;
	char* Authorization;
	char* Referer;
	
	char header_complete;
	uint32_t parsed_bytes;

#ifdef WEBSERVER_USE_WEBSOCKETS
	unsigned char WebSocketOutHash[40]; /* Versionen vor 8 benutzen nur 16 byte */
	char *SecWebSocketProtocol;
	char *SecWebSocketKey;
	int  SecWebSocketVersion;
	char isWebsocket;
#endif

	char no_websocket_support;

	char* post_buffer;
	uint32_t post_buffer_pos;
	char* boundary;

	char* Content_Disposition;
	char* Content_Type;

	ws_variable_store *parameter_store;
	list_t cookie_list;

} HttpRequestHeader;

typedef struct {
	char guid[WEBSERVER_GUID_LENGTH + 1];
	unsigned char ssl;
	ws_variable_store* vars;
	unsigned long last_use;
}sessionStore;

struct ssl_store_s;



typedef struct{
	FILE_OFFSET file_send_pos;
	WebserverFileInfo *file_info;
} socket_file_infos;


typedef void ( *extern_handler ) ( int fd, void* ptr );

typedef struct{

	char *buffer;
	unsigned long buffer_size;
	FILE_OFFSET buffer_send_pos;

} output_buffer;

typedef struct{

	output_buffer header;
	output_buffer body;

	socket_file_infos file_infos;

} output_struct;

/* WebSocket Streaming - forward declaration and internal types */
struct websocket_stream_context;  /* Full definition in webserver_api_functions.h */

typedef struct websocket_stream_handler_entry {
    char* url;
    void (*on_start)(struct websocket_stream_context* ctx);
    void (*on_chunk)(struct websocket_stream_context* ctx,
                     const unsigned char* data,
                     uint32_t length);
    void (*on_end)(struct websocket_stream_context* ctx,
                   int success);
} websocket_stream_handler_entry;

typedef struct {
	int socket;
	
	int reverse_proxy_checked;
	int reverse_proxy_error;
	

#ifdef WEBSERVER_USE_IPV6
	char client_ip_str[INET6_ADDRSTRLEN];
	char client_v6_client;
#else
	char client_ip_str[INET_ADDRSTRLEN];
#endif
	unsigned int port;

	char active;
	char console;
	char client;
	char server;
	char disable_output;     /* den output der template engine ausschalten ( fuer echo off ) */
	char enable_print_funcs; /* template engine funktionen ausgeben */
	char print_func_prefix[50];
	char print_func_postfix[50];


	

#ifdef WEBSERVER_USE_SSL
	char use_ssl;
	char ssl_pending;
	// TODO werden die event flags noch gebraucht ??
	char ssl_block_event_flags;
	uint32_t ssl_event_flags;
	char run_ssl_accept;
	struct ssl_store_s *ssl_context;
	int ssl_pending_bytes;
#endif

#if defined( USE_LIBEVENT )
	struct event *my_ev;
#elif defined( USE_EPOLL )
	int registered;
#endif
	char closeSocket;

	void *s;

	WS_MUTEX socket_mutex;

	HttpRequestHeader* header;
	char *header_buffer;
	unsigned int header_buffer_pos;
	unsigned int header_buffer_size;

	list_t header_chunk_list;
	list_t html_chunk_list;

#ifdef WEBSERVER_USE_WEBSOCKETS
	char isWebsocket;

	list_t websocket_chunk_list;
	list_t websocket_fragments;
	int    websocket_send_pos;

	unsigned int websocket_fragment_start_opcode;
	unsigned int websocket_fragments_length;



	unsigned char *websocket_buffer;
	unsigned int websocket_buffer_offset;
	char *websocket_guid;
	char *websocket_store_guid;

	/* Streaming state */
	char websocket_streaming_active;           /* Are we in the middle of a stream? */
	char websocket_stream_fragmented;          /* 1 if streaming fragmented frames (fin=0 + CONTINUE) */
	uint64_t websocket_stream_remaining;       /* Bytes still to receive (0 for fragmented = unknown total) */
	unsigned char websocket_stream_mask[4];    /* Mask for unmasking */
	uint64_t websocket_stream_mask_offset;     /* Position in mask cycle */
	struct websocket_stream_context* websocket_stream_ctx;
	websocket_stream_handler_entry* websocket_stream_handler;
#endif

	WebserverFileInfo *send_file_info;

	char use_output_compression;


	list_t output_list;

	extern_handler extern_handle;
	void* extern_handle_data_ptr;

	uint32_t error_requests_on_socket;

} socket_info;

typedef struct{
	char* data;
	char* name;
	uint64_t length;
}upload_file_info;

typedef struct{
	char* name;
	char* value;
}custom_response_header;

typedef struct {
	FUNCTION_PARAS func;
	char *prefix;
	char *pagename;
	char *pagedata;
	int datalenght;
	char return_found;
}engine_infos;

#define MAX_ENGINE_RECURTION 20
typedef struct {
	char guid[WEBSERVER_GUID_LENGTH + 1];
	char guid_ssl[WEBSERVER_GUID_LENGTH+1];
	HttpRequestHeader* header;
	socket_info* socket;
	ws_variable_store* render_var_store;
	sessionStore* store;
	sessionStore* store_ssl;
	engine_infos engine_list[MAX_ENGINE_RECURTION];
	int 	     engine_index;
	engine_infos *engine_current;
	unsigned char create_cookie;
	unsigned char create_cookie_ssl;

	list_t upload_files;
	list_t custom_response_headers;
	int response_code;

}http_request;

#ifdef WEBSERVER_USE_PYTHON
struct web_py_plugin{
	PyThreadState* thread_state;
	PyObject* global_namespace;
	PyObject* local_namespace;
	char* path;
	char* exec_path;
	char plugin_name[60];

	unsigned long int	last_mod_sec;
	unsigned long int   last_mod_nsec;
	int size;
};
#endif

typedef struct {
	char* path;

	char* name;
	char* error;

	int type;

	void *dl;

#ifdef WEBSERVER_USE_PYTHON
	struct web_py_plugin* py_plugin;
#endif

} plugin_s;


typedef enum {
	CONDITION_TRUE = 1, CONDITION_FALSE = 2, CONDITION_ERROR = 3
} CONDITION_RETURN;

typedef CONDITION_RETURN (*user_condition)(http_request* s, FUNCTION_PARAS* func);

typedef struct {
	user_condition uc;
	char* name;
	plugin_s* plugin;
	const char* file;
	int line;
} user_condition_s;

typedef void (*user_function)(http_request *s, FUNCTION_PARAS* func);

struct user_func_s {

	char* name;
	plugin_s* plugin;
	const char* file;
	int line;
	int type;

#ifdef WEBSERVER_USE_PYTHON
	PyObject * py_func;
#endif

	user_function uf;

};

typedef enum {
	WEBSOCKET_SIGNAL_CONNECT = 1, WEBSOCKET_SIGNAL_MSG = 2, WEBSOCKET_SIGNAL_DISCONNECT = 3
} WEBSOCKET_SIGNALS;

typedef void (*websocket_handler)(WEBSOCKET_SIGNALS signal, const char* guid, const char binary, const char* msg, const unsigned long long len);

typedef struct {
	char* url;
	list_t *handler_list;
} websocket_handler_list_s;

typedef struct {
	websocket_handler wsh;
	const char* file;
	int line;
} websocket_handler_s;



void dumpStore(http_request* s, ws_variable_store* store);
void dumpStoreText(http_request* s, ws_variable_store* store, int tabs);

int check_url_functions(http_request* s);



#if __GNUC__ > 2

	#define DEFINE_FUNCTION_INT( a ) 	const char*			ws_ef_##a##_df = __FILE__; \
										const int			ws_ef_##a##_dl = __LINE__; \
										void 				ws_ef_##a ( http_request *s,FUNCTION_PARAS* func ); \
										void 				ws_ef_##a ( http_request *s,FUNCTION_PARAS* func )

	#define REGISTER_LOCAL_FUNCTION_INT( a ) 	register_function ( #a,ws_ef_##a,ws_ef_##a##_df,ws_ef_##a##_dl );

	#define REGISTER_FUNCTION_INT( a ){ \
		extern const char*	ws_ef_##a##_df; \
		extern const int 	ws_ef_##a##_dl; \
		extern void 		ws_ef_##a ( http_request *s,FUNCTION_PARAS* func ) ; \
		register_function ( #a,ws_ef_##a,ws_ef_##a##_df,ws_ef_##a##_dl ); }

#else
	#define DEFINE_FUNCTION_INT( a ) 	void 				ws_ef_##a ( http_request *s,FUNCTION_PARAS* func )
	#define REGISTER_FUNCTION_INT( a ) 	register_function ( #a,ws_ef_##a,"",0 );
	#define REGISTER_LOCAL_FUNCTION_INT( a ) 	register_function ( #a,ws_ef_##a,"",0 );

#endif

#endif

