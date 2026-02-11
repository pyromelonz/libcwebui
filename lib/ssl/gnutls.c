/*

SPDX-License-Identifier: MPL-2.0

 libCWebUI
 Copyright (C) 2007 - 2019  Ramin Seyed-Moussavi

 Projekt URL : https://github.com/lordrasmus/libcwebui

 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at https://mozilla.org/MPL/2.0/. 

*/

#include "WebserverConfig.h"

#ifdef WEBSERVER_USE_GNUTLS_CRYPTO

#include <gnutls/gnutls.h>
#include <gnutls/crypto.h>
#include <gnutls/extra.h>
#include <gnutls/openpgp.h>
#include <gnutls/x509.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "webserver.h"



#if (OPENSSL_VERSION_NUMBER < 0x00905100L)
#warning "GnuTLS Version auch prüfen"
#endif

#define KEYFILE "server.pem"
#define PASSWORD "password"
#define CA_LIST "root.pem"
#define DHFILE "dh1024.pem"

//#define SA struct sockaddr
//#define SOCKET_ERR(err,s) if(err==-1) {perror(s);return(1);}
//#define MAX_BUF 1024
//#define PORT 5556               /* listen to 5556 port */
#define DH_BITS 1024

struct ssl_store_s {
	gnutls_session_t session;
};



static gnutls_anon_server_credentials_t anoncred;
static gnutls_dh_params_t dh_params;

static const int protocol_priority[] = { GNUTLS_TLS1_1, GNUTLS_TLS1_0, GNUTLS_SSL3, 0 };

int fp = 0;

void gnutls_debug_log_all(int level, const char *str) {
//	write(fp,str,strlen(str));
	printf("%s", str);
}

gnutls_priority_t pri_cache;
gnutls_x509_crt_t crts[100];

int initOpenSSL(void) {
	const char *err;
	int ret, fd,cert_max=10;
	unsigned char buffer[2000];
	//fp = open("/tmp/gnutls.log",O_WRONLY );
	gnutls_global_set_log_level(9);
	gnutls_global_set_log_function(gnutls_debug_log_all);
	_gnutls_log(fp, "gnutls: %s\n", gnutls_check_version(NULL));

	gnutls_global_init();
	//gnutls_anon_allocate_server_credentials(&anoncred);
	//generate_dh_params();
	//gnutls_anon_set_server_dh_params(anoncred, dh_params);

	//gnutls_priority_init(&pri_cache ,"SECURE:-VERS-SSL3.0:+COMP-DEFLATE",&err);
	gnutls_priority_init(&pri_cache, "EXPORT:%COMPAT", &err);

	ret = gnutls_dh_params_init(&dh_params);
	if (ret < 0) {
		return printf("GnuTLS: Failed to initialize: (%d) %s", ret, gnutls_strerror(ret));
	}

	fd = open(DHFILE, O_RDONLY);
	ret = read(fd, buffer, 2000);
	close(fd);

	gnutls_datum_t dh_data;
	dh_data.size = ret;
	dh_data.data = buffer;

	ret = gnutls_dh_params_import_pkcs3(dh_params, &dh_data, GNUTLS_X509_FMT_PEM);
	if (ret < 0) {
		return printf("GnuTLS: Failed to Import DH params '%s': (%d) %s", DHFILE, ret, gnutls_strerror(ret));
	}


	fd = open("gnutls/certs/server.crt", O_RDONLY);
	ret = read(fd, buffer, 2000);
	close(fd);

	ret = gnutls_x509_crt_list_import(crts, &cert_max, &dh_data, GNUTLS_X509_FMT_PEM, 0);
	if (ret < 0) {
		return printf("GnuTLS: Failed to Import Certificate '%s': (%d) %s", "root.pem", ret, gnutls_strerror(ret));
	}

	return 0;
}

int WebserverSSLInit(socket_info* s) {

	if (s->ssl_context == 0) s->ssl_context = (struct ssl_store_s*) WebserverMalloc(sizeof(struct ssl_store_s), 0);

	gnutls_init(&s->ssl_context->session, GNUTLS_SERVER);
	gnutls_protocol_set_priority(s->ssl_context->session, protocol_priority);
	//gnutls_priority_set_direct(s->ssl_context->session, "NORMAL:+ANON-ECDH:+ANON-DH", NULL);
	gnutls_credentials_set(s->ssl_context->session, GNUTLS_CRD_ANON, anoncred);
	gnutls_dh_set_prime_bits(s->ssl_context->session, DH_BITS);

	return 0;
}

int WebserverSSLCloseSockets(socket_info *s) {
	//ERR_clear_error();
	if (s->ssl_context != 0) {
		//SSL_set_shutdown(s->ssl_context->ssl, SSL_SENT_SHUTDOWN | SSL_RECEIVED_SHUTDOWN);
		//SSL_free(s->ssl_context->ssl);
		gnutls_bye(s->ssl_context->session, GNUTLS_SHUT_WR);
		//	close(s->socket);
		gnutls_deinit(s->ssl_context->session);
		WebserverFree(s->ssl_context);
	}
	return 0;
}

//ssize_t mgs_transport_read(gnutls_transport_ptr_t t, void*, size_t len){
ssize_t mgs_transport_read(gnutls_transport_ptr_t ptr, void *buffer, size_t len) {
	socket_info* s = (socket_info*) ptr;
	return recv(s->socket, buffer, len, 0);
}

int WebserverSSLAccept(socket_info* s) {
	int ret;

	gnutls_transport_set_pull_function(s->ssl_context->session, mgs_transport_read);
	//gnutls_transport_set_push_function(s->ssl_context->session, mgs_transport_write);

	gnutls_transport_set_ptr(s->ssl_context->session, (gnutls_transport_ptr_t) s);

	ret = gnutls_handshake(s->ssl_context->session);
	if (ret < 0) {
		close(s->socket);
		gnutls_deinit(s->ssl_context->session);
		WebserverFree(s->ssl_context);
		fprintf(stderr, "*** Handshake has failed (%s)\n\n", gnutls_strerror(ret));
		s->use_ssl = 0;
		return NO_SSL_CONNECTION_ERROR;

	}
	return SSL_ACCEPT_OK;
}

int WebserverSLLRecvNonBlocking(socket_info* s, unsigned char *buf, int len, int flags) {
	int ret = gnutls_record_recv(s->ssl_context->session, buf, len);

	if (ret == 0) {
		printf("\n- Peer has closed the GnuTLS connection\n");
		return SSL_PROTOCOL_ERROR;
	} else if (ret < 0) {
		fprintf(stderr, "\n*** Received corrupted data(%d). Closing the connection.\n\n", ret);
		return SSL_PROTOCOL_ERROR;
	} else if (ret > 0) {
		return ret;

		//return CLIENT_NO_MORE_DATA;
	}
	return SSL_PROTOCOL_ERROR;
}

SOCKET_SEND_STATUS WebserverSLLSendNonBlocking(socket_info* s, unsigned char *buf, int len, int flags, int* bytes_send) {
	gnutls_record_send(s->ssl_context->session, buf, len);
	*bytes_send = len;
	return SOCKET_SEND_NO_MORE_DATA;
	//return SOCKET_SEND_SSL_ERROR;
}



#endif // WEBSERVER_USE_OPENSSL_CRYPTO
#ifdef OPENSSL_CODE

void printSSLErrorQueue(socket_info* s) {
	unsigned long err_code;
	char buffer[130]; // SSL requires 120 bytes
	while ((err_code = ERR_get_error())) {
		ERR_error_string(err_code, buffer);
		LOG(CONNECTION_LOG, ERROR_LEVEL, s->socket, "%s", buffer);
	}
}

#endif

#ifdef OPENSSL_CODE
static int password_cb(char *buf, int num, int rwflag, void *userdata) {
	if (num < (int) strlen(pass) + 1) return (0);

	strncpy(buf, pass, num);
	return (strlen(pass));
}

int load_dh_params(SSL_CTX *ctx, char *file) {
	DH *ret = 0;
	BIO *bio;

	if ((bio = BIO_new_file(file, "r")) == NULL) {
		LOG(CONNECTION_LOG, ERROR_LEVEL, 0, "Couldn't open DH file", "");
		return -1;
	}

	ret = PEM_read_bio_DHparams(bio, NULL, NULL, NULL);
	BIO_free(bio);
	if (SSL_CTX_set_tmp_dh(ctx, ret) < 0) {
		LOG(CONNECTION_LOG, ERROR_LEVEL, 0, "Couldn't set DH parameters", "");
		return -1;
	}

	return 1;
}

SSL_CTX *initialize_ctx(char* path, char *keyfile, char* ca, char *password) {
#ifdef OPENSSL_CODE
	SSL_METHOD *meth;
	SSL_CTX *ctx;
	char buffer[200];

	if (!bio_err) {
		/* Global system initialization*/
		SSL_library_init();
		SSL_load_error_strings();

		/* An error write context */
		bio_err = BIO_new_fp(stderr, BIO_NOCLOSE);
	}

	/* Set up a SIGPIPE handler */
	//signal(SIGPIPE,sigpipe_handle);
	/* Create our context*/
	meth = (SSL_METHOD*) SSLv23_server_method();
	ctx = SSL_CTX_new(meth);

	/* Load our keys and certificates*/
	snprintf(buffer, 200, "%s%s", path, keyfile);
	if (!(SSL_CTX_use_certificate_chain_file(ctx, buffer) )) {
		LOG(CONNECTION_LOG, ERROR_LEVEL, 0, "Can't read certificate file %s", buffer);
		return NULL;
	}

	pass = password;
	SSL_CTX_set_default_passwd_cb(ctx, password_cb);
	if (!(SSL_CTX_use_PrivateKey_file(ctx, buffer, SSL_FILETYPE_PEM) )) {
		LOG(CONNECTION_LOG, ERROR_LEVEL, 0, "Can't read key file %s", buffer);
		return NULL;
	}

	snprintf(buffer, 200, "%s%s", path, ca);
	if (!(SSL_CTX_load_verify_locations(ctx, buffer, 0) )) // Load the CAs we trust
	{
		LOG(CONNECTION_LOG, ERROR_LEVEL, 0, "Can't read CA list %s", buffer);
		return NULL;
	}

	// TLS RSA AES_256_CBC SHA
//	#define CIPHER_LIST	"ALL:!aNULL:!eNULL:-HIGH"

#define  CIPHER_LIST	"AES256-SHA:AES128-SHA:DHE-DSS-AES128-SHA:DHE-DSS-AES256-SHA:DHE-RSA-AES128-SHA:DHE-RSA-AES256-SHA:ADH-AES128-SHA:ADH-AES256-SHA" //	 DHE_RSA:NULL-MD5:NULL-SHA"
	//#define CIPHER_LIST	"TLS_RSA_WITH_AES_128_CBC_SHA"
	//#define CIPHER_LIST		"SSL_TXT_DES_192_EDE3_CBC_WITH_SHA:-HIGH"
	//#define CIPHER_LIST		"LOW:TLS_RSA_WITH_AES_128_CBC_SHA"

	if (!SSL_CTX_set_cipher_list(ctx, CIPHER_LIST)) {
		LOG(CONNECTION_LOG, ERROR_LEVEL, 0, "Can't set cipher list %s", CIPHER_LIST);
		return NULL;
	}

	//SSL_CTX_set_session_id_context(ctx, &sid_ctx, sizeof(sid_ctx));

#if (OPENSSL_VERSION_NUMBER < 0x00905100L)
	SSL_CTX_set_verify_depth(ctx, 1);
#endif

	return ctx;
#endif
	return 0;
}

void destroy_ctx(SSL_CTX *ctx) {
	SSL_CTX_free(ctx);
}

#endif
