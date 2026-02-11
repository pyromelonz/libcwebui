#ifndef _WEBSERVER_CONFIG_
#define _WEBSERVER_CONFIG_


#define ENGINE_BUFFER_SIZE                   2000
#define WEBSOCKET_INIT_INBUFFER_SIZE         300
#define WEBSOCKET_MAX_INBUFFER_SIZE          65000
#define WEBSERVER_MAX_HEADER_LINE_LENGHT     ( 1024*2 )
#define WRITE_DATA_SIZE                      20000
#define WEBSERVER_MAX_POST_CONTENT_LENGTH    10000000
#define MAX_CACHE_AGE                        15

#define MAX_FUNC_PARAS 20

/************************************************************************************
*                                                                                   *
*                               GENERAL SETTINGS                                    *
*                                                                                   *
************************************************************************************/

#define SERVER_NAME                 "libcwebui"
#define SERVER_DOMAIN               "Testserver"
#define WEBSERVER_GUID_LENGTH       50


/************************************************************************************
*                                                                                   *
*                               SSL SETTINGS - mbedTLS                              *
*                                                                                   *
************************************************************************************/

#define WEBSERVER_USE_SSL
#define WEBSERVER_USE_MBEDTLS3_CRYPTO


#ifndef WEBSERVER_USE_SSL
	#undef WEBSERVER_USE_YASSL_CRYPTO
	#undef WEBSERVER_USE_OPENSSL_CRYPTO
	#undef WEBSERVER_USE_MBEDTLS2_CRYPTO
	#undef WEBSERVER_USE_MBEDTLS3_CRYPTO
#else
	#if !defined(WEBSERVER_USE_YASSL_CRYPTO) && !defined(WEBSERVER_USE_OPENSSL_CRYPTO) && !defined(WEBSERVER_USE_MBEDTLS2_CRYPTO) && !defined(WEBSERVER_USE_MBEDTLS3_CRYPTO)
		#error "kein SSL crypto definiert"
	#endif
#endif

/************************************************************************************
*                                                                                   *
*                               SOCKET SETTINGS                                     *
*                                                                                   *
************************************************************************************/

#define USE_EPOLL
#define WEBSERVER_USE_WEBSOCKETS
#define WEBSERVER_MAX_PENDING_CONNECTIONS   30


/************************************************************************************
*                                                                                   *
*                               FILESYSTEM SETTINGS                                 *
*                                                                                   *
************************************************************************************/

#define WEBSERVER_MAX_FILEID_TO_RAM  FILE_TYPE_JS

#define WEBSERVER_USE_LOCAL_FILE_SYSTEM
#define WEBSERVER_USE_BINARY_FORMAT
#define WEBSERVER_USE_WNFS


/************************************************************************************
*                                                                                   *
*                               SESSION SETTINGS                                    *
*                                                                                   *
************************************************************************************/

#define WEBSERVER_USE_SESSIONS
#define WEBSERVER_SESSION_TIMEOUT   300

#endif
