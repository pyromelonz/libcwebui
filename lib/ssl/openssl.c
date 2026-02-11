/*

SPDX-License-Identifier: MPL-2.0

 libCWebUI
 Copyright (C) 2007 - 2019  Ramin Seyed-Moussavi

 Projekt URL : https://github.com/lordrasmus/libcwebui

 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at https://mozilla.org/MPL/2.0/. 

*/



// http://www.ietf.org/rfc/rfc2818.txt		RFC 2818 HTTP Over TLS May 2000

// http://stevehuston.wordpress.com/2009/12/29/how-to-use-schannel-for-ssl-sockets-on-windows/
// http://en.wikipedia.org/wiki/Cryptographic_Application_Programming_Interface
// http://publib.boulder.ibm.com/infocenter/tpfhelp/current/index.jsp?topic=/com.ibm.ztpf-ztpfdf.doc_put.cur/gtpc2/cpp_ssl_ctx_set_cipher_list.html
// http://www.openssl.org/docs/apps/ciphers.html#TLS_v1_0_cipher_suites_

//LOG : printSSLErrorQueue : src/openssl.c 249   ( 1022 ) : error:1408F119:SSL routines:SSL3_GET_RECORD:decryption failed or bad record mac

// nss vielleicht auch ?
// yassl

// * SSL Infos *
// http://www.g-sec.lu/products.html

//#ifdef WEBSERVER_USE_SSL


#include "WebserverConfig.h"

#ifdef WEBSERVER_USE_OPENSSL_CRYPTO

#ifdef __GNUC__
#include <openssl/md5.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>
#include "webserver.h"
#endif

#ifdef DMALLOC
#include <dmalloc/dmalloc.h>
#endif


#if SHA_DIGEST_LENGTH != SSL_SHA_DIG_LEN
	#error "SHA Digest Length mismatch"
#endif

#if (OPENSSL_VERSION_NUMBER < 0x00905100L)
	#error "Must use OpenSSL 0.9.6 or later"
#endif

#ifdef __CDT_PARSER__
	#define __BASE_FILE__
#endif


struct ssl_store_s {
	SSL *ssl;
	BIO *sbio;
};

struct sha_context {
	SHA_CTX *sha_ctx;
};

SSL_CTX *g_ctx;


BIO *bio_err = 0;

static char *pass;

static long sess_cache_mode;


SSL_CTX *initialize_ctx( char *keyfile, char* keyfile_backup, char* ca, char *password );
int load_dh_params(SSL_CTX *ctx, char *file);
void printSSLErrorQueue(socket_info* s);

int initOpenSSL(void) {

	int file_error = 0;

	const char* vers = SSLeay_version(SSLEAY_VERSION);

	bio_err = 0;

	LOG( SSL_LOG, NOTICE_LEVEL, 0, "using openssl : %s ", vers);

	if (0 == getConfigText("ssl_key_file")) {
		LOG( SSL_LOG, ERROR_LEVEL, 0,"%s", "SSL fehler kein ssl_key_file gesetzt");
		file_error = 1;
	}else{
		LOG( SSL_LOG, NOTICE_LEVEL, 0, "SSL ssl_key_file %s", getConfigText("ssl_key_file") );
	}

	if (0 == getConfigText("ssl_key_file_backup")) {
		//LOG( SSL_LOG, WARNING_LEVEL, 0, "SSL fehler kein ssl_key_file_backup gesetzt", "");
	}else{
		LOG( SSL_LOG, NOTICE_LEVEL, 0, "SSL ssl_key_file_backup %s", getConfigText("ssl_key_file_backup") );
	}

	if (0 == getConfigText("ssl_key_file_password")) {
		LOG( SSL_LOG, ERROR_LEVEL, 0, "%s","SSL fehler kein ssl_key_file_password gesetzt");
		file_error = 1;
	}

	if (0 == getConfigText("ssl_dh_file")) {
		LOG( SSL_LOG, ERROR_LEVEL, 0, "%s","SSL fehler kein ssl_dh_file gesetzt");
		file_error = 1;
	}else{
		LOG( SSL_LOG, NOTICE_LEVEL, 0, "SSL ssl_dh_file %s", getConfigText("ssl_dh_file") );
	}

	if (0 == getConfigText("ssl_ca_list_file")) {
		LOG( SSL_LOG, ERROR_LEVEL, 0, "%s","SSL fehler kein ssl_ca_list_file gesetzt");
		file_error = 1;
	}else{
		LOG( SSL_LOG, NOTICE_LEVEL, 0, "SSL ssl_ca_list_file %s", getConfigText("ssl_ca_list_file") );
	}

	if ( file_error == 1){
		LOG( SSL_LOG, ERROR_LEVEL, 0,"%s", "SSL Datei fehler");
		return -1;
	}

	OpenSSL_add_all_algorithms(); // load & register cryptos
	SSL_load_error_strings(); // load all error messages

	g_ctx = initialize_ctx(
							getConfigText("ssl_key_file"),
							getConfigText("ssl_key_file_backup"),
							getConfigText("ssl_ca_list_file"),
							getConfigText("ssl_key_file_password")
						);

	if ( g_ctx == NULL ){
		LOG( SSL_LOG, ERROR_LEVEL, 0, "%s","SSL fehler init ctx, SSL disabled");
		return -1;
	}
	if (load_dh_params(g_ctx, getConfigText("ssl_dh_file") ) < 0) {
		LOG( SSL_LOG, ERROR_LEVEL, 0,"%s", "SSL fehler dh_params");
		return -1;
	}

	//cache_mode = SSL_SESS_CACHE_OFF; // No session caching for client or server takes place.

	//sess_cache_mode = SSL_SESS_CACHE_SERVER|SSL_SESS_CACHE_NO_INTERNAL;
	//SSL_CTX_set_session_cache_mode(g_ctx, sess_cache_mode);


	sess_cache_mode = SSL_CTX_get_session_cache_mode(g_ctx);

	//printf("sess_cache_mode: 0x%X\n",sess_cache_mode);

	//SSL_CTX_sess_set_new_cb(ctx,    ssl_callback_NewSessionCacheEntry);
	//SSL_CTX_sess_set_get_cb(ctx,    ssl_callback_GetSessionCacheEntry);
	//SSL_CTX_sess_set_remove_cb(ctx, ssl_callback_DelSessionCacheEntry);


	SSL_CTX_set_mode(g_ctx, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
	SSL_CTX_set_read_ahead(g_ctx, 1);

	int ops = 0;
	if ( 1 == getConfigInt("ssl_disable_SSLv2") )   ops |= SSL_OP_NO_SSLv2;
	if ( 1 == getConfigInt("ssl_disable_SSLv3") )   ops |= SSL_OP_NO_SSLv3;
	if ( 1 == getConfigInt("ssl_disable_TLSv1.0") ) ops |= SSL_OP_NO_TLSv1;
	if ( 1 == getConfigInt("ssl_disable_TLSv1.1") ) ops |= SSL_OP_NO_TLSv1_1;


	// SSLv2, SSLv3 deaktivieren , testen :   nmap --script ssl-cert,ssl-enum-ciphers -p 443 192.168.11.94
	SSL_CTX_set_options(g_ctx, ops );


	return 0;
}

/*static int password_cb(char *buf, int num, UNUSED_PARA int rwflag, UNUSED_PARA void *userdata) {
	if (num < (int) strlen(pass) + 1) return (0);

	strncpy(buf, pass, num);
	return (strlen(pass));
}*/

int load_dh_params(SSL_CTX *ctx, char *file) {
    BIO *bio;
    
    if ((bio = BIO_new_file(file, "r")) == NULL) {
        LOG(CONNECTION_LOG, ERROR_LEVEL, 0, "%s", "Couldn't open DH file");
        return -1;
    }

#if OPENSSL_VERSION_NUMBER < 0x30000000L
    // Für OpenSSL 1.1.x: Veraltete Funktion verwenden
    DH *dh = PEM_read_bio_DHparams(bio, NULL, NULL, NULL);
    BIO_free(bio);

    if (dh == NULL) {
        LOG(CONNECTION_LOG, ERROR_LEVEL, 0, "%s", "Couldn't read DH parameters");
        return -1;
    }

    if (SSL_CTX_set_tmp_dh(ctx, dh) != 1) {
        LOG(CONNECTION_LOG, ERROR_LEVEL, 0, "%s", "Couldn't set DH parameters");
        DH_free(dh); // Ressourcen freigeben
        return -1;
    }

    DH_free(dh); // DH wird in OpenSSL 1.1.x kopiert, daher freigeben

#else
    // Für OpenSSL 3.0 und neuer: Die neue EVP_PKEY-basierte Methode
    EVP_PKEY *pkey = PEM_read_bio_Parameters(bio, NULL);
    BIO_free(bio);

    if (pkey == NULL) {
        LOG(CONNECTION_LOG, ERROR_LEVEL, 0, "%s", "Couldn't read DH parameters");
        return -1;
    }

    if (SSL_CTX_set0_tmp_dh_pkey(ctx, pkey) <= 0) {

        unsigned long err_code = ERR_get_error(); // Fehlercode abrufen
        char err_buf[256]; // Buffer für Fehlermeldung
        ERR_error_string_n(err_code, err_buf, sizeof(err_buf)); // Fehler in lesbaren Text umwandeln


        LOG(CONNECTION_LOG, ERROR_LEVEL, 0, "%s", "Couldn't set DH parameters");
        EVP_PKEY_free(pkey); // pkey freigeben, wenn das Setzen fehlschlägt
        return -1;
    }

    // pkey gehört jetzt zu ctx, also wird es nicht hier freigegeben
#endif

    return 1;
}


int VISIBLE_ATTR WebserverSSLTestKeyfile( char* keyfile ){
	SSL_METHOD *meth;
	SSL_CTX *ctx;

	int error = 0;
	meth = (SSL_METHOD*) SSLv23_server_method();
	ctx = SSL_CTX_new(meth);

	if (!(SSL_CTX_use_certificate_chain_file(ctx, keyfile))) {
		LOG(CONNECTION_LOG, ERROR_LEVEL, 0, "Can't read certificate: file %s", keyfile);
		error = 1;
	}

	if (!(SSL_CTX_use_PrivateKey_file(ctx, keyfile, SSL_FILETYPE_PEM))) {
		LOG(CONNECTION_LOG, ERROR_LEVEL, 0, "Can't read key: file %s", keyfile);
		error = 1;
	}

#ifdef ENABLE_DEVEL_WARNINGS
	#warning Memory Leak
#endif	

	return error;
}

SSL_CTX *initialize_ctx( char *keyfile, char* keyfile_backup, char* ca, char *password) {
	SSL_METHOD *meth;
	SSL_CTX *ctx;
	//char buffer[200];

	if (!bio_err) {
		SSL_library_init();
		SSL_load_error_strings();

		/* An error write context */
		bio_err = BIO_new_fp(stderr, BIO_NOCLOSE);
	}

	/* Create our context*/
	meth = (SSL_METHOD*) SSLv23_server_method();
	ctx = SSL_CTX_new(meth);

	/* Load our keys and certificates*/
	pass = password; // global char*

	if ( 0 == WebserverSSLTestKeyfile( keyfile ) ) {
		SSL_CTX_use_certificate_chain_file(ctx, keyfile);
		SSL_CTX_use_PrivateKey_file(ctx, keyfile, SSL_FILETYPE_PEM);

	}else{
		LOG(CONNECTION_LOG, ERROR_LEVEL, 0, "Can't read keyfile file %s trying backup", keyfile);

		if ( 0 == WebserverSSLTestKeyfile( keyfile_backup ) ) {
			SSL_CTX_use_certificate_chain_file(ctx, keyfile_backup);
			SSL_CTX_use_PrivateKey_file(ctx, keyfile_backup, SSL_FILETYPE_PEM);
		}else{
			LOG(CONNECTION_LOG, ERROR_LEVEL, 0, "Can't read keyfile backup file %s ", keyfile_backup);
			SSL_CTX_free( ctx );
			return NULL;
		}
	}

	if (!(SSL_CTX_load_verify_locations(ctx, ca, 0))) // Load the CAs we trust
	{
		LOG(CONNECTION_LOG, ERROR_LEVEL, 0, "Can't read CA list %s", ca);
		SSL_CTX_free( ctx );
		return NULL;
	}

	// TLS RSA AES_256_CBC SHA
//	#define CIPHER_LIST	"ALL:!aNULL:!eNULL:-HIGH"

// https://mozilla.github.io/server-side-tls/ssl-config-generator/

#define CIPHER_LIST	"HIGH:!aNULL:!eNULL:!3DES:@STRENGTH"
//#define CIPHER_LIST "ECDHE-ECDSA-CHACHA20-POLY1305:ECDHE-RSA-CHACHA20-POLY1305:ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES256-GCM-SHA384:ECDHE-RSA-AES256-GCM-SHA384:DHE-RSA-AES128-GCM-SHA256:DHE-RSA-AES256-GCM-SHA384:ECDHE-ECDSA-AES128-SHA256:ECDHE-RSA-AES128-SHA256:ECDHE-ECDSA-AES128-SHA:ECDHE-RSA-AES256-SHA384:ECDHE-RSA-AES128-SHA:ECDHE-ECDSA-AES256-SHA384:ECDHE-ECDSA-AES256-SHA:ECDHE-RSA-AES256-SHA:DHE-RSA-AES128-SHA256:DHE-RSA-AES128-SHA:DHE-RSA-AES256-SHA256:DHE-RSA-AES256-SHA:ECDHE-ECDSA-DES-CBC3-SHA:ECDHE-RSA-DES-CBC3-SHA:EDH-RSA-DES-CBC3-SHA:AES128-GCM-SHA256:AES256-GCM-SHA384:AES128-SHA256:AES256-SHA256:AES128-SHA:AES256-SHA:DES-CBC3-SHA:!DSS"
//#define  CIPHER_LIST	"HIGH:!SSLv2:!SSLv3:RC4+HIGH:!aNULL:!eNULL:!3DES:@STRENGTH"
/*#define  CIPHER_LIST	"AES256-SHA:AES128-SHA:"\
						"DHE-DSS-AES128-SHA:DHE-DSS-AES256-SHA:"\
						"DHE-RSA-AES128-SHA:DHE-RSA-AES256-SHA"
	*/
	// ADH-AES128-SHA <- broken laut nmap
	// ADH-AES256-SHA <- broken laut nmap
	// DHE_RSA:NULL-MD5:NULL-SHA"

	// TLS_DH_anon_WITH_AES_128_CBC_SHA
	//#define CIPHER_LIST	"TLS_RSA_WITH_AES_128_CBC_SHA"
	//#define CIPHER_LIST		"SSL_TXT_DES_192_EDE3_CBC_WITH_SHA:-HIGH"
	//#define CIPHER_LIST		"LOW:TLS_RSA_WITH_AES_128_CBC_SHA"

	if (!SSL_CTX_set_cipher_list(ctx, CIPHER_LIST)) {
		LOG(CONNECTION_LOG, ERROR_LEVEL, 0, "Can't set cipher list %s", CIPHER_LIST);
		SSL_CTX_free( ctx );
		return NULL;
	}

	//SSL_CTX_set_session_id_context(ctx, &sid_ctx, sizeof(sid_ctx));

#if (OPENSSL_VERSION_NUMBER < 0x00905100L)
	SSL_CTX_set_verify_depth(ctx, 1);
#endif

	return ctx;
}

#if 0
static void destroy_ctx(SSL_CTX *ctx) {
	SSL_CTX_free(ctx);
}
#endif

int WebserverSSLInit(socket_info* s) {
	if (s->ssl_context == 0){
		 s->ssl_context = (struct ssl_store_s*) WebserverMalloc ( sizeof ( struct ssl_store_s ) );
	}

	s->ssl_context->ssl = SSL_new(g_ctx);
	if (s->ssl_context->ssl == 0){
		LOG(CONNECTION_LOG, ERROR_LEVEL, s->socket,"%s", "WebserverSSLInit fehler");
	}

	s->ssl_pending_bytes = 0;
	s->ssl_context->sbio= BIO_new_socket(s->socket, BIO_NOCLOSE);
	SSL_set_bio(s->ssl_context->ssl, s->ssl_context->sbio, s->ssl_context->sbio);

	SSL_set_read_ahead(s->ssl_context->ssl, 0);

	//SSL_CTX_set_session_cache_mode(s->ssl_context->ssl, sess_cache_mode);

	return 0;
}

int WebserverSSLCloseSockets(socket_info *s) {
	ERR_clear_error();
	if (s->ssl_context != 0) {
		SSL_set_shutdown(s->ssl_context->ssl, SSL_SENT_SHUTDOWN | SSL_RECEIVED_SHUTDOWN);
		SSL_free(s->ssl_context->ssl);
		WebserverFree(s->ssl_context);
	}
	return 0;
}

void printSSLErrorQueue(socket_info* s) {
	unsigned long err_code;
	char buffer[256]; // SSL requires at least 256 bytes
	while ((err_code = ERR_get_error())) {
		ERR_error_string_n(err_code, buffer, sizeof( buffer ));
		LOG( CONNECTION_LOG, ERROR_LEVEL, s->socket, "%s", buffer);
	}
}

int WebserverSSLPending(socket_info* s) {

	if(s->use_ssl == 0){
		return 0;
	}

	if ( s->ssl_context == 0 ){
		return 0;
	}

	s->ssl_pending_bytes = SSL_pending( s->ssl_context->ssl );

	if( s->ssl_pending_bytes > 0 ){
		return 1;
	}

	return 0;
}

int WebserverSSLAccept(socket_info* s) {
	int r, r2;

	while (1) {
		ERR_clear_error();
		r = SSL_accept(s->ssl_context->ssl);
		if (r == 1) {
#if _WEBSERVER_CONNECTION_DEBUG_ > 1
			LOG ( CONNECTION_LOG,NOTICE_LEVEL,s->socket,"SSL accept ok","" );
#endif
			//PlatformSetNonBlocking(s->socket);
			s->run_ssl_accept = 0;
			printSSLErrorQueue(s);
			return SSL_ACCEPT_OK;
		}

		if (r <= 0) {
			r2 = SSL_get_error(s->ssl_context->ssl, r);
			switch (r2) {
			case SSL_ERROR_NONE:
				// The TLS/SSL I/O operation completed. This result code is returned if and only if ret > 0.
				break;

			case SSL_ERROR_WANT_READ:
				//printSSLErrorQueue(s);

				//
				// Hier muss nur nochmal gelesen werden
				//
#if _WEBSERVER_CONNECTION_DEBUG_ > 4
				LOG ( CONNECTION_LOG,ERROR_LEVEL,s->socket,"SSL_ERROR_WANT_READ","" );
#endif
				return CLIENT_NO_MORE_DATA;
				break;
			case SSL_ERROR_WANT_WRITE:
				// The operation did not complete; the same TLS/SSL I/O function should be called again later.
				// If, by then, the underlying BIO has data available for reading (if the result code is SSL_ERROR_WANT_READ)
				// or allows writing data (SSL_ERROR_WANT_WRITE), then some TLS/SSL protocol progress will take place,
				// i.e. at least part of an TLS/SSL record will be read or written.
				// Note that the retry may again lead to a SSL_ERROR_WANT_READ or SSL_ERROR_WANT_WRITE condition.
				// There is no fixed upper limit for the number of iterations that may be necessary until progress becomes visible at application protocol level.

				// For socket BIOs (e.g. when SSL_set_fd() was used), select() or poll() on the underlying socket can be used to find out
				// when the TLS/SSL I/O function should be retried.
				// Caveat: Any TLS/SSL I/O function can lead to either of SSL_ERROR_WANT_READ and SSL_ERROR_WANT_WRITE. In particular,
				// SSL_read() or SSL_peek() may want to write data and SSL_write() may want to read data.
				// This is mainly because TLS/SSL handshakes may occur at any time during the protocol (initiated by either the client or the server);
				// SSL_read(), SSL_peek(), and SSL_write() will handle any pending handshakes.
				printSSLErrorQueue(s);
//#if _WEBSERVER_CONNECTION_DEBUG_ >= 4
				LOG ( CONNECTION_LOG,ERROR_LEVEL,s->socket,"%s","SSL_ERROR_WANT_WRITE" );
//#endif
				return CLIENT_NO_MORE_DATA;
				break;

			case SSL_ERROR_WANT_CONNECT:
			case SSL_ERROR_WANT_ACCEPT:
				// The operation did not complete; the same TLS/SSL I/O function should be called again later.
				// The underlying BIO was not connected yet to the peer and the call would block in connect()/accept().
				// The SSL function should be called again when the connection is established.
				// These messages can only appear with a BIO_s_connect() or BIO_s_accept() BIO, respectively.
				// In order to find out, when the connection has been successfully established, on many platforms select() or poll()
				// for writing on the socket file descriptor can be used.
			case SSL_ERROR_ZERO_RETURN:
				// The TLS/SSL connection has been closed. If the protocol version is SSL 3.0 or TLS 1.0,
				// this result code is returned only if a closure alert has occurred in the protocol,
				// i.e. if the connection has been closed cleanly.
				// Note that in this case SSL_ERROR_ZERO_RETURN does not necessarily indicate that the underlying transport has been closed.
			case SSL_ERROR_WANT_X509_LOOKUP:
				// The operation did not complete because an application callback set by SSL_CTX_set_client_cert_cb() has asked to be called again.
				// The TLS/SSL I/O function should be called again later. Details depend on the application.
			case SSL_ERROR_SYSCALL:
				// Some I/O error occurred. The OpenSSL error queue may contain more information on the error.
				// If the error queue is empty (i.e. ERR_get_error() returns 0), ret can be used to find out more about the error:
				// If ret == 0, an EOF was observed that violates the protocol. If ret == -1, the underlying BIO reported an I/O error
				// (for socket I/O on Unix systems, consult errno for details).
				LOG( CONNECTION_LOG, ERROR_LEVEL, s->socket,"%s", "SSL_ERROR_SYSCALL");
				printSSLErrorQueue(s);
				return SSL_PROTOCOL_ERROR;
			case SSL_ERROR_SSL:{
					int r3 = ERR_get_error();

					// Suppress expected errors when clients reject self-signed certificates
					switch (r3) {
						case 336151574:  // 0x14094416 - OpenSSL 1.x: sslv3 alert certificate unknown
						case 336151576:  // 0x14094418 - OpenSSL 1.x: tlsv1 alert unknown ca
						case 167773206:  // 0x0A000416 - OpenSSL 3.x: sslv3 alert certificate unknown
						case 167773208:  // 0x0A000418 - OpenSSL 3.x: tlsv1 alert unknown ca
							return SSL_PROTOCOL_ERROR;
					}

					LOG( CONNECTION_LOG, ERROR_LEVEL, s->socket, "%s","SSL_ERROR_SSL");

					char buffer[256];
					ERR_error_string_n(r3, buffer, 256);
					LOG( CONNECTION_LOG, ERROR_LEVEL, s->socket, "%s", buffer);

					printSSLErrorQueue(s);
				}
				return SSL_PROTOCOL_ERROR;
				// A failure in the SSL library occurred, usually a protocol error. The OpenSSL error queue contains more information on the error.
			default:
				printSSLErrorQueue(s);
				LOG( CONNECTION_LOG, ERROR_LEVEL, s->socket, "Unhandled SSL Error ( %d )", r2);
				return SSL_PROTOCOL_ERROR;
			}
		}
	}
}

int WebserverSSLRecvNonBlocking(socket_info* s, unsigned char *buf, unsigned int len, UNUSED_PARA int flags) {
	int ret = 0;
	int l = 0;
	int r2;
	int diff;
	unsigned long err_code;

	if ( s->ssl_pending_bytes ){

		if ( ! WebserverSSLPending( s ) ){
			return 0;
		}

		if ( (unsigned int)s->ssl_pending_bytes <= len ){
			ret = SSL_read(s->ssl_context->ssl, buf, s->ssl_pending_bytes );
			return ret;
		}else{
			ret = SSL_read(s->ssl_context->ssl, buf, len );
			return ret;
		}

	}

	char buffer[130]; // SSL requires 120 bytes
	do {
		ERR_clear_error();
		diff = len - l;
		ret = SSL_read(s->ssl_context->ssl, &buf[l], diff);
		//printf("ssl read ret : %d - %d\n",ret, SSL_pending( s->ssl_context->ssl ));
		//s->ssl_context->sbio
		r2 = SSL_get_error(s->ssl_context->ssl, ret);
		switch (r2) {
		case SSL_ERROR_NONE:
			l += ret;
			break;

		case SSL_ERROR_WANT_READ:

#if _WEBSERVER_CONNECTION_DEBUG_ > 3
			LOG(CONNECTION_LOG,ERROR_LEVEL,s->socket,"SSL_ERROR_WANT_READ %d / %d / %d / %d ",l,len,ret,r2);
#endif
			if (l == 0) {
				return CLIENT_NO_MORE_DATA;
			} else {
				return l;
			}
			break;
		case SSL_ERROR_WANT_WRITE:

#if _WEBSERVER_CONNECTION_DEBUG_ > 3
			LOG(CONNECTION_LOG,ERROR_LEVEL,s->socket,"SSL_ERROR_WANT_WRITE %d / %d / %d / %d ",l,len,ret,r2);
#endif

			if (l == 0) {
				return CLIENT_NO_MORE_DATA;
			} else {
				return l;
			}
			break;

		case SSL_ERROR_ZERO_RETURN:
			err_code = ERR_get_error();
			ERR_error_string(err_code, buffer);
#ifdef _WEBSERVER_CONNECTION_DEBUG_
			LOG ( CONNECTION_LOG,ERROR_LEVEL,s->socket,"%s","SSL_ERROR_ZERO_RETURN" );
#endif
			return CLIENT_DISCONNECTED;

		case SSL_ERROR_SYSCALL:
			err_code = ERR_get_error();
			if ( err_code == 0 ){
				if ( ( ret == 0 ) && ( errno == 0 )){
					return CLIENT_DISCONNECTED;
				}
				#if _WEBSERVER_CONNECTION_DEBUG_ > 1
					LOG ( CONNECTION_LOG,ERROR_LEVEL,s->socket,"SSL_ERROR_SYSCALL ( %d / %d / %d / %m ) ",ret,r2, errno );
				#endif
			}else{
				ERR_error_string(err_code, buffer);
				#if _WEBSERVER_CONNECTION_DEBUG_ > 1
					LOG ( CONNECTION_LOG,ERROR_LEVEL,s->socket,"SSL_ERROR_SYSCALL ( %d / %d ) %s",ret,r2,buffer );
				#endif
			}
			return SSL_PROTOCOL_ERROR;

		case SSL_ERROR_WANT_X509_LOOKUP:
		case SSL_ERROR_WANT_CONNECT:
		case SSL_ERROR_WANT_ACCEPT:
			return SSL_PROTOCOL_ERROR;

		case SSL_ERROR_SSL:
			// A failure in the SSL library occurred, usually a protocol error.
			err_code = ERR_get_error();
			// Suppress harmless errors (client disconnect without close_notify, etc.)
#ifdef SSL_R_UNEXPECTED_EOF_WHILE_READING
			if (ERR_GET_REASON(err_code) == SSL_R_UNEXPECTED_EOF_WHILE_READING) {
				return CLIENT_DISCONNECTED;
			}
#endif
			ERR_error_string(err_code, buffer);
			LOG(CONNECTION_LOG, ERROR_LEVEL, s->socket, "SSL Error: %s", buffer);
			return SSL_PROTOCOL_ERROR;

		default:
			err_code = ERR_get_error();
			ERR_error_string(err_code, buffer);
			LOG(CONNECTION_LOG, ERROR_LEVEL, s->socket, "Unhandled SSL Error ( %d ) %s", r2, buffer);
			return SSL_PROTOCOL_ERROR;
		}
		if ((unsigned int)l == len) {
			return l;
		}
		//}while(SSL_pending(s->ssl)>0);
	} while (1);

	return SSL_PROTOCOL_ERROR;
}

SOCKET_SEND_STATUS WebserverSSLSendNonBlocking(socket_info* s, const unsigned char *buf, int len, UNUSED_PARA int flags, int* bytes_send) {
	int ret = 0;
	int l = 0;
	int r2;
	unsigned long err_code;
	//unsigned short want_reads = 0;
	//unsigned short want_writes = 0;

	char buffer[130]; // SSL requires 120 bytes
	do {
		ERR_clear_error();
		ret = SSL_write(s->ssl_context->ssl, &buf[l], len - l);
		r2 = SSL_get_error(s->ssl_context->ssl, ret);
		switch (r2) {
		case SSL_ERROR_NONE:
			l += ret;
			break;

		case SSL_ERROR_WANT_READ:

#if _WEBSERVER_CONNECTION_DEBUG_ > 3
			LOG( CONNECTION_LOG, ERROR_LEVEL, s->socket, "SSL_ERROR_WANT_READ", "");
#endif
			if (bytes_send != 0){
				*bytes_send = l;
			}
			return SOCKET_SEND_SEND_BUFFER_FULL;

		case SSL_ERROR_WANT_WRITE:

#if _WEBSERVER_CONNECTION_DEBUG_ > 3
			LOG(CONNECTION_LOG,ERROR_LEVEL,s->socket,"SSL_ERROR_WANT_WRITE","");
#endif
			if (bytes_send != 0){
				*bytes_send = l;
			}
			return SOCKET_SEND_SEND_BUFFER_FULL;

		case SSL_ERROR_SSL:
		case SSL_ERROR_WANT_X509_LOOKUP:
		case SSL_ERROR_SYSCALL:
			err_code = ERR_get_error();
			if ( err_code == 0 ){
				if ( ( ret == 0 ) && ( errno == 0 )){
					return CLIENT_DISCONNECTED;
				}
				#if _WEBSERVER_CONNECTION_DEBUG_ > 1
					LOG ( CONNECTION_LOG,ERROR_LEVEL,s->socket,"SSL_ERROR_SYSCALL ( %d / %d / %d / %m ) ",ret,r2, errno );
				#endif
			}else{
				ERR_error_string(err_code, buffer);
				#if _WEBSERVER_CONNECTION_DEBUG_ > 1
					LOG ( CONNECTION_LOG,ERROR_LEVEL,s->socket,"SSL_ERROR_SYSCALL ( %d / %d ) %s",ret,r2,buffer );
				#endif
			}
			return SSL_PROTOCOL_ERROR;

		case SSL_ERROR_ZERO_RETURN:
		case SSL_ERROR_WANT_CONNECT:
		case SSL_ERROR_WANT_ACCEPT:
		default:
			err_code = ERR_get_error();
			ERR_error_string(err_code, buffer);
			//ERR_error_string(r2,buffer);
			LOG( CONNECTION_LOG, ERROR_LEVEL, s->socket, "Unhandled SSL Error ( %d ) %s", r2, buffer);
			if (bytes_send != 0){
				*bytes_send = l;
			}
			return SOCKET_SEND_SSL_ERROR;
		}
		if (l == len) {
			if (bytes_send != 0){
				*bytes_send = l;
			}
			return SOCKET_SEND_NO_MORE_DATA;
		}
		//}while(SSL_pending(s->ssl)>0);
	} while (1);
	return SOCKET_SEND_SSL_ERROR;
}



#endif // WEBSERVER_USE_OPENSSL_CRYPTO
/*

 SSL_ERROR_NONE
 SSL_ERROR_WANT_READ
 SSL_ERROR_WANT_WRITE
 The operation did not complete; the same TLS/SSL I/O function should be called again later.
 If, by then, the underlying BIO has data available for reading (if the result code is SSL_ERROR_WANT_READ)
 or allows writing data (SSL_ERROR_WANT_WRITE), then some TLS/SSL protocol progress will take place,
 i.e. at least part of an TLS/SSL record will be read or written.
 Note that the retry may again lead to a SSL_ERROR_WANT_READ or SSL_ERROR_WANT_WRITE condition.
 There is no fixed upper limit for the number of iterations that may be necessary until progress becomes visible at application protocol level.

 For socket BIOs (e.g. when SSL_set_fd() was used), select() or poll() on the underlying socket can be used to find out
 when the TLS/SSL I/O function should be retried.
 Caveat: Any TLS/SSL I/O function can lead to either of SSL_ERROR_WANT_READ and SSL_ERROR_WANT_WRITE. In particular,
 SSL_read() or SSL_peek() may want to write data and SSL_write() may want to read data.
 This is mainly because TLS/SSL handshakes may occur at any time during the protocol (initiated by either the client or the server);
 SSL_read(), SSL_peek(), and SSL_write() will handle any pending handshakes.

 SSL_ERROR_ZERO_RETURN
 The TLS/SSL connection has been closed. If the protocol version is SSL 3.0 or TLS 1.0,
 this result code is returned only if a closure alert has occurred in the protocol,
 i.e. if the connection has been closed cleanly.
 Note that in this case SSL_ERROR_ZERO_RETURN does not necessarily indicate that the underlying transport has been closed.

 SSL_ERROR_WANT_CONNECT
 SSL_ERROR_WANT_ACCEPT
 The operation did not complete; the same TLS/SSL I/O function should be called again later.
 The underlying BIO was not connected yet to the peer and the call would block in connect()/accept().
 The SSL function should be called again when the connection is established.
 These messages can only appear with a BIO_s_connect() or BIO_s_accept() BIO, respectively.
 In order to find out, when the connection has been successfully established, on many platforms select() or poll()
 for writing on the socket file descriptor can be used.

 SSL_ERROR_WANT_X509_LOOKUP
 The operation did not complete because an application callback set by SSL_CTX_set_client_cert_cb() has asked to be called again.
 The TLS/SSL I/O function should be called again later. Details depend on the application.

 SSL_ERROR_SYSCALL
 Some I/O error occurred. The OpenSSL error queue may contain more information on the error.
 If the error queue is empty (i.e. ERR_get_error() returns 0), ret can be used to find out more about the error:
 If ret == 0, an EOF was observed that violates the protocol. If ret == -1, the underlying BIO reported an I/O error
 (for socket I/O on Unix systems, consult errno for details).

 SSL_ERROR_SSL
 A failure in the SSL library occurred, usually a protocol error. The OpenSSL error queue contains more information on the error.

 */

/*
 SSL_ERROR_NONE
 SSL_ERROR_WANT_READ
 SSL_ERROR_WANT_WRITE
 The operation did not complete; the same TLS/SSL I/O function should be called again later.
 If, by then, the underlying BIO has data available for reading (if the result code is SSL_ERROR_WANT_READ)
 or allows writing data (SSL_ERROR_WANT_WRITE), then some TLS/SSL protocol progress will take place,
 i.e. at least part of an TLS/SSL record will be read or written.
 Note that the retry may again lead to a SSL_ERROR_WANT_READ or SSL_ERROR_WANT_WRITE condition.
 There is no fixed upper limit for the number of iterations that may be necessary until progress becomes visible at application protocol level.

 For socket BIOs (e.g. when SSL_set_fd() was used), select() or poll() on the underlying socket can be used to find out
 when the TLS/SSL I/O function should be retried.
 Caveat: Any TLS/SSL I/O function can lead to either of SSL_ERROR_WANT_READ and SSL_ERROR_WANT_WRITE. In particular,
 SSL_read() or SSL_peek() may want to write data and SSL_write() may want to read data.
 This is mainly because TLS/SSL handshakes may occur at any time during the protocol (initiated by either the client or the server);
 SSL_read(), SSL_peek(), and SSL_write() will handle any pending handshakes.

 SSL_ERROR_ZERO_RETURN
 The TLS/SSL connection has been closed. If the protocol version is SSL 3.0 or TLS 1.0,
 this result code is returned only if a closure alert has occurred in the protocol,
 i.e. if the connection has been closed cleanly.
 Note that in this case SSL_ERROR_ZERO_RETURN does not necessarily indicate that the underlying transport has been closed.

 SSL_ERROR_WANT_CONNECT
 SSL_ERROR_WANT_ACCEPT
 The operation did not complete; the same TLS/SSL I/O function should be called again later.
 The underlying BIO was not connected yet to the peer and the call would block in connect()/accept().
 The SSL function should be called again when the connection is established.
 These messages can only appear with a BIO_s_connect() or BIO_s_accept() BIO, respectively.
 In order to find out, when the connection has been successfully established, on many platforms select() or poll()
 for writing on the socket file descriptor can be used.

 SSL_ERROR_WANT_X509_LOOKUP
 The operation did not complete because an application callback set by SSL_CTX_set_client_cert_cb() has asked to be called again.
 The TLS/SSL I/O function should be called again later. Details depend on the application.

 SSL_ERROR_SYSCALL
 Some I/O error occurred. The OpenSSL error queue may contain more information on the error.
 If the error queue is empty (i.e. ERR_get_error() returns 0), ret can be used to find out more about the error:
 If ret == 0, an EOF was observed that violates the protocol. If ret == -1, the underlying BIO reported an I/O error
 (for socket I/O on Unix systems, consult errno for details).

 SSL_ERROR_SSL
 A failure in the SSL library occurred, usually a protocol error. The OpenSSL error queue contains more information on the error.

 */
