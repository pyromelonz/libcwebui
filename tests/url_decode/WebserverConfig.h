#ifndef _WEBSERVER_CONFIG_
#define _WEBSERVER_CONFIG_


#define ENGINE_BUFFER_SIZE                   2000
#define WEBSOCKET_INIT_INBUFFER_SIZE         300  
#define WEBSOCKET_MAX_INBUFFER_SIZE          65000
#define WEBSERVER_MAX_HEADER_LINE_LENGHT     ( 1024*2 )  // max length of one header line
#define WRITE_DATA_SIZE                      20000
#define WEBSERVER_MAX_POST_CONTENT_LENGTH    10000000    // 10 MB
#define MAX_CACHE_AGE                        15          // zeit in sekunden bis der client daten neu abrufen soll



//#define WEBSERVER_ONLY_SSL_COOKIES                     // cookies nur über ssl verschicken ( benötigt SSL )


#define MAX_FUNC_PARAS 20

/************************************************************************************
*                                                                                   *
*								GENERAL SETTINGS									*
*                                                                                   *
************************************************************************************/

#define SERVER_NAME                 "libcwebui"
#define SERVER_DOMAIN				"Testserver"
#define WEBSERVER_GUID_LENGTH		50					// länge der guids für die sessions


//#define WEBSERVER_USE_PYTHON							// Python Support

#define WEBSERVER_USE_SSL							// HTTPS ( SSL ) Support
//#define WEBSERVER_USE_YASSL_CRYPTO					// SSL mit yaSSL
#define WEBSERVER_USE_OPENSSL_CRYPTO				// SSL mit openssl
//#define WEBSERVER_USE_MBEDTLS_CRYPTO				// SSL mit mbed tls



#ifndef WEBSERVER_USE_SSL
	#undef WEBSERVER_USE_YASSL_CRYPTO
	#undef WEBSERVER_USE_OPENSSL_CRYPTO
	#undef WEBSERVER_USE_MBEDTLS_CRYPTO
#else
	#if !defined(WEBSERVER_USE_YASSL_CRYPTO) && !defined(WEBSERVER_USE_OPENSSL_CRYPTO) && !defined(WEBSERVER_USE_MBEDTLS_CRYPTO)
		#error "kein SSL crypto definiert"
	#endif
#endif

/************************************************************************************
*                                                                                   *
*								SOCKET SETTINGS  									*
*                                                                                   *
************************************************************************************/

#define USE_LIBEVENT
#define WEBSERVER_USE_WEBSOCKETS
//#define WEBSERVER_USE_IPV6
#define WEBSERVER_MAX_PENDING_CONNECTIONS	30


/************************************************************************************
*                                                                                   *
*								FILESYSTEM SETTINGS									*
*                                                                                   *
************************************************************************************/

//#define DISABLE_OLD_TEMPLATE_SYSTEM					// altes Template Erkennungssystem deaktivieren

#define WEBSERVER_MAX_FILEID_TO_RAM	 FILE_TYPE_JS   // welche datein im ram gecached werden ( datatypes.h )

//#define WEBSERVER_DISABLE_CACHE						// deaktiviert die http cache header
//#define WEBSERVER_DISABLE_FILE_UPDATE					// deaktiviert den file update check


#define WEBSERVER_USE_LOCAL_FILE_SYSTEM				// Das Normale File System aktivieren
#define WEBSERVER_USE_BINARY_FORMAT					// Das Binary File System aktivieren
#define WEBSERVER_USE_WNFS							// Das Webserver Network File System aktivieren



/************************************************************************************
*                                                                                   *
*								SESSION SETTINGS									*
*                                                                                   *
************************************************************************************/

#define WEBSERVER_USE_SESSIONS
#define WEBSERVER_SESSION_TIMEOUT	300

//#define _WEBSERVER_DEBUG_                         // Debugausgaben für den Webserver aktivieren
//#define _WEBSERVER_MEMORY_DEBUG_                  // Debugausgaben für den Webserver Speichermanager aktivieren
//#define _WEBSERVER_PARAMETER_DEBUG_               // Debugausgaben für den Webseiten Parameter aktivieren
//#define _WEBSERVER_TEMPLATE_DEBUG_                // Debugausgaben für die Webserver Template Engine aktivieren
//#define _WEBSERVER_FILESYSTEM_DEBUG_              // Debugausgaben für die Webserver Filesystem aktivieren
//#define _WEBSERVER_FILESYSTEM_CACHE_DEBUG_        // Debugausgaben für die Webserver Filesystem Cache aktivieren
//#define _WEBSERVER_HEADER_DEBUG_                	// Debugausgaben für die HTTP Header aktivieren
//#define _WEBSERVER_BODY_DEBUG_                	// Debugausgaben für die HTTP Bodys aktivieren
//#define _WEBSERVER_COOKIE_DEBUG_                	// Debugausgaben für die HTTP Cookies aktivieren
//#define _WEBSERVER_SESSION_DEBUG_                	// Debugausgaben für die HTTP Session aktivieren
//#define _WEBSERVER_SOCKET_DEBUG_		5
//#define _WEBSERVER_CONNECTION_DEBUG_              // Debugausgaben für die HTTP Connections aktivieren
//#define _WEBSERVER_CONNECTION_SEND_DEBUG_         // Debugausgaben für die HTTP Connection Sende Routinen aktivieren
//#define _WEBSERVER_CACHE_DEBUG_					// Debugausgaben für die HTTP Cache Mechanismus aktivieren
//#define _WEBSERVER_HANDLER_DEBUG_					// Debugausgaben fuer die Socket Handler



#endif
