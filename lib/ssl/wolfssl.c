/*

SPDX-License-Identifier: MPL-2.0

 libCWebUI
 Copyright (C) 2007 - 2024  Ramin Seyed-Moussavi

 Projekt URL : https://github.com/lordrasmus/libcwebui


 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at https://mozilla.org/MPL/2.0/.

*/

#include "WebserverConfig.h"

#ifdef WEBSERVER_USE_WOLFSSL_CRYPTO

#include "webserver.h"

#include <wolfssl/options.h>
#include <wolfssl/ssl.h>
#include <wolfssl/error-ssl.h>

#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <unistd.h>

#ifdef DMALLOC
#include <dmalloc/dmalloc.h>
#endif

/* Global context for server */
static WOLFSSL_CTX *wolf_ctx = NULL;
static int ssl_initialized = 0;

/* Per-connection context */
struct ssl_store_s {
	WOLFSSL *ssl;
	int socket_fd;
	int handshake_done;
};

static void print_wolfssl_error(const char *func, int ret) {
	char error_buf[WOLFSSL_MAX_ERROR_SZ];
	wolfSSL_ERR_error_string((unsigned long)ret, error_buf);
	LOG(SSL_LOG, ERROR_LEVEL, 0, "%s returned %d: %s", func, ret, error_buf);
}

int initOpenSSL(void) {
	int ret;
	const char *keyfile;
	const char *cafile;
	const char *dhfile;

	LOG(SSL_LOG, NOTICE_LEVEL, 0, "using wolfSSL: %s", wolfSSL_lib_version());

	/* Initialize wolfSSL library */
	ret = wolfSSL_Init();
	if (ret != WOLFSSL_SUCCESS) {
		LOG(SSL_LOG, ERROR_LEVEL, 0, "wolfSSL_Init() failed");
		return -1;
	}

	/* Create context for TLS server */
	wolf_ctx = wolfSSL_CTX_new(wolfTLSv1_2_server_method());
	if (wolf_ctx == NULL) {
		LOG(SSL_LOG, ERROR_LEVEL, 0, "wolfSSL_CTX_new() failed");
		return -1;
	}

	/* Load server certificate */
	keyfile = getConfigText("ssl_key_file");
	if (keyfile == NULL) {
		LOG(SSL_LOG, ERROR_LEVEL, 0, "ssl_key_file not configured");
		return -1;
	}

	LOG(SSL_LOG, NOTICE_LEVEL, 0, "SSL ssl_key_file %s", keyfile);

	/* Load certificate chain */
	ret = wolfSSL_CTX_use_certificate_chain_file(wolf_ctx, keyfile);
	if (ret != WOLFSSL_SUCCESS) {
		print_wolfssl_error("wolfSSL_CTX_use_certificate_chain_file", ret);
		return -1;
	}

	/* Load private key */
	ret = wolfSSL_CTX_use_PrivateKey_file(wolf_ctx, keyfile, SSL_FILETYPE_PEM);
	if (ret != WOLFSSL_SUCCESS) {
		print_wolfssl_error("wolfSSL_CTX_use_PrivateKey_file", ret);
		return -1;
	}

	/* Verify private key matches certificate */
	ret = wolfSSL_CTX_check_private_key(wolf_ctx);
	if (ret != WOLFSSL_SUCCESS) {
		LOG(SSL_LOG, ERROR_LEVEL, 0, "Private key does not match certificate");
		return -1;
	}

	/* Load CA certificate if configured */
	cafile = getConfigText("ssl_root_file");
	if (cafile != NULL) {
		LOG(SSL_LOG, NOTICE_LEVEL, 0, "SSL ssl_root_file %s", cafile);
		ret = wolfSSL_CTX_load_verify_locations(wolf_ctx, cafile, NULL);
		if (ret != WOLFSSL_SUCCESS) {
			print_wolfssl_error("wolfSSL_CTX_load_verify_locations", ret);
			/* Continue without CA verification */
		}
	}

	/* Load DH parameters if configured */
	dhfile = getConfigText("ssl_dh_file");
	if (dhfile != NULL) {
		LOG(SSL_LOG, NOTICE_LEVEL, 0, "SSL ssl_dh_file %s", dhfile);
		ret = wolfSSL_CTX_SetTmpDH_file(wolf_ctx, dhfile, SSL_FILETYPE_PEM);
		if (ret != WOLFSSL_SUCCESS) {
			print_wolfssl_error("wolfSSL_CTX_SetTmpDH_file", ret);
			/* Continue without custom DH parameters */
		}
	}

	/* Set cipher list - prefer modern ciphers */
	wolfSSL_CTX_set_cipher_list(wolf_ctx,
		"ECDHE-RSA-AES256-GCM-SHA384:"
		"ECDHE-RSA-AES128-GCM-SHA256:"
		"DHE-RSA-AES256-GCM-SHA384:"
		"DHE-RSA-AES128-GCM-SHA256:"
		"AES256-GCM-SHA384:"
		"AES128-GCM-SHA256");

	ssl_initialized = 1;
	LOG(SSL_LOG, NOTICE_LEVEL, 0, "wolfSSL initialized successfully");
	return 0;
}

int VISIBLE_ATTR WebserverSSLTestKeyfile(char *keyfile) {
	WOLFSSL_CTX *test_ctx;
	int ret;

	if (keyfile == NULL) {
		return -1;
	}

	test_ctx = wolfSSL_CTX_new(wolfTLSv1_2_server_method());
	if (test_ctx == NULL) {
		return -1;
	}

	ret = wolfSSL_CTX_use_certificate_chain_file(test_ctx, keyfile);
	if (ret != WOLFSSL_SUCCESS) {
		wolfSSL_CTX_free(test_ctx);
		return -1;
	}

	ret = wolfSSL_CTX_use_PrivateKey_file(test_ctx, keyfile, SSL_FILETYPE_PEM);
	if (ret != WOLFSSL_SUCCESS) {
		wolfSSL_CTX_free(test_ctx);
		return -1;
	}

	ret = wolfSSL_CTX_check_private_key(test_ctx);
	wolfSSL_CTX_free(test_ctx);

	return (ret == WOLFSSL_SUCCESS) ? 0 : -1;
}

int WebserverSSLInit(socket_info *s) {
	struct ssl_store_s *ctx;

	if (!ssl_initialized || wolf_ctx == NULL) {
		LOG(SSL_LOG, ERROR_LEVEL, s->socket, "SSL not initialized");
		return -1;
	}

	ctx = (struct ssl_store_s *)WebserverMalloc(sizeof(struct ssl_store_s));
	if (ctx == NULL) {
		LOG(SSL_LOG, ERROR_LEVEL, s->socket, "Failed to allocate SSL context");
		return -1;
	}

	memset(ctx, 0, sizeof(struct ssl_store_s));
	ctx->socket_fd = s->socket;
	ctx->handshake_done = 0;

	ctx->ssl = wolfSSL_new(wolf_ctx);
	if (ctx->ssl == NULL) {
		LOG(SSL_LOG, ERROR_LEVEL, s->socket, "wolfSSL_new() failed");
		WebserverFree(ctx);
		return -1;
	}

	/* Set socket for SSL connection */
	wolfSSL_set_fd(ctx->ssl, s->socket);

	/* Set non-blocking mode */
	wolfSSL_set_using_nonblock(ctx->ssl, 1);

	s->ssl_context = ctx;
	return 0;
}

int WebserverSSLCloseSockets(socket_info *s) {
	struct ssl_store_s *ctx = s->ssl_context;

	if (ctx == NULL) {
		return 0;
	}

	if (ctx->ssl != NULL) {
		/* Try to send close_notify, but don't block */
		wolfSSL_shutdown(ctx->ssl);
		wolfSSL_free(ctx->ssl);
		ctx->ssl = NULL;
	}

	WebserverFree(ctx);
	s->ssl_context = NULL;
	return 0;
}

int WebserverSSLPending(socket_info *s) {
	struct ssl_store_s *ctx = s->ssl_context;

	if (ctx == NULL || ctx->ssl == NULL) {
		return 0;
	}

	return wolfSSL_pending(ctx->ssl);
}

int WebserverSSLAccept(socket_info *s) {
	struct ssl_store_s *ctx = s->ssl_context;
	int ret;
	int error;

	if (ctx == NULL) {
		return SSL_PROTOCOL_ERROR;
	}

	if (ctx->handshake_done) {
		return SSL_ACCEPT_OK;
	}

	ret = wolfSSL_accept(ctx->ssl);
	if (ret == WOLFSSL_SUCCESS) {
		ctx->handshake_done = 1;
		s->run_ssl_accept = 0;
#if _WEBSERVER_CONNECTION_DEBUG_ > 1
		LOG(SSL_LOG, DEBUG_LEVEL, s->socket, "SSL handshake complete: %s",
			wolfSSL_get_cipher_name(ctx->ssl));
#endif
		return SSL_ACCEPT_OK;
	}

	error = wolfSSL_get_error(ctx->ssl, ret);

	if (error == WOLFSSL_ERROR_WANT_READ || error == WOLFSSL_ERROR_WANT_WRITE) {
		return CLIENT_NO_MORE_DATA;
	}

	/* Suppress expected errors when clients reject self-signed certificates */

	/* -308 = SOCKET_ERROR_E: client closed connection during handshake */
	if (error == -308) {
		return SSL_PROTOCOL_ERROR;
	}

	/* Check TLS alert history for certificate rejection alerts */
	{
		WOLFSSL_ALERT_HISTORY alert_history;
		if (wolfSSL_get_alert_history(ctx->ssl, &alert_history) == WOLFSSL_SUCCESS) {
			int last_alert = alert_history.last_rx.code;
			switch (last_alert) {
				case 46:  /* certificate_unknown */
				case 48:  /* unknown_ca */
					return SSL_PROTOCOL_ERROR;
			}
		}
	}

	print_wolfssl_error("wolfSSL_accept", error);
	return SSL_PROTOCOL_ERROR;
}

int WebserverSSLRecvNonBlocking(socket_info *s, unsigned char *buf, unsigned int len, UNUSED_PARA int flags) {
	struct ssl_store_s *ctx = s->ssl_context;
	int ret;
	int error;

	(void)flags;

	if (ctx == NULL) {
		return SSL_PROTOCOL_ERROR;
	}

	ret = wolfSSL_read(ctx->ssl, buf, (int)len);

	if (ret > 0) {
		return ret;
	}

	error = wolfSSL_get_error(ctx->ssl, ret);

	if (error == WOLFSSL_ERROR_WANT_READ || error == WOLFSSL_ERROR_WANT_WRITE) {
		return CLIENT_NO_MORE_DATA;
	}

	if (error == WOLFSSL_ERROR_ZERO_RETURN) {
		return CLIENT_DISCONNECTED;
	}

	if (error == WOLFSSL_ERROR_SYSCALL) {
		if (errno == 0 || errno == ECONNRESET || errno == EPIPE) {
			return CLIENT_DISCONNECTED;
		}
	}

#ifdef _WEBSERVER_CONNECTION_DEBUG_
	print_wolfssl_error("wolfSSL_read", error);
#endif
	return SSL_PROTOCOL_ERROR;
}

SOCKET_SEND_STATUS WebserverSSLSendNonBlocking(socket_info *s, const unsigned char *buf, int len, UNUSED_PARA int flags, int *bytes_send) {
	struct ssl_store_s *ctx = s->ssl_context;
	int ret;
	int error;

	(void)flags;

	if (ctx == NULL) {
		if (bytes_send) *bytes_send = 0;
		return SOCKET_SEND_SSL_ERROR;
	}

	ret = wolfSSL_write(ctx->ssl, buf, len);

	if (ret > 0) {
		if (bytes_send) *bytes_send = ret;
		if (ret == len) {
			return SOCKET_SEND_NO_MORE_DATA;
		}
		return SOCKET_SEND_SEND_BUFFER_FULL;
	}

	error = wolfSSL_get_error(ctx->ssl, ret);

	if (error == WOLFSSL_ERROR_WANT_READ || error == WOLFSSL_ERROR_WANT_WRITE) {
		if (bytes_send) *bytes_send = 0;
		return SOCKET_SEND_SEND_BUFFER_FULL;
	}

#ifdef _WEBSERVER_CONNECTION_DEBUG_
	print_wolfssl_error("wolfSSL_write", error);
#endif
	if (bytes_send) *bytes_send = 0;
	return SOCKET_SEND_SSL_ERROR;
}

#endif /* WEBSERVER_USE_WOLFSSL_CRYPTO */
