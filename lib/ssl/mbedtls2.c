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

#ifdef WEBSERVER_USE_MBEDTLS2_CRYPTO

#include "webserver.h"

#include "mbedtls/version.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/pk.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/debug.h"
#include "mbedtls/error.h"

#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <unistd.h>

#ifdef DMALLOC
#include <dmalloc/dmalloc.h>
#endif

#if (MBEDTLS_VERSION_NUMBER >= 0x03000000)
	#error "mbedtls2.c requires mbed TLS 2.x - use mbedtls3.c for version 3.x or later"
#endif

/* Global context for server certificate and key */
static mbedtls_x509_crt server_cert;
static mbedtls_pk_context server_key;
static mbedtls_entropy_context entropy;
static mbedtls_ctr_drbg_context ctr_drbg;
static mbedtls_ssl_config ssl_conf;
static int ssl_initialized = 0;

/* Per-connection context */
struct ssl_store_s {
	mbedtls_ssl_context ssl;
	int socket_fd;
	int handshake_done;
};

/* Custom send callback for non-blocking sockets */
static int ssl_send_callback(void *ctx, const unsigned char *buf, size_t len) {
	struct ssl_store_s *ssl_ctx = (struct ssl_store_s *)ctx;
	int ret;

	ret = (int)send(ssl_ctx->socket_fd, buf, len, 0);
	if (ret < 0) {
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			return MBEDTLS_ERR_SSL_WANT_WRITE;
		}
		if (errno == EPIPE || errno == ECONNRESET) {
			return MBEDTLS_ERR_NET_CONN_RESET;
		}
		return MBEDTLS_ERR_NET_SEND_FAILED;
	}
	return ret;
}

/* Custom recv callback for non-blocking sockets */
static int ssl_recv_callback(void *ctx, unsigned char *buf, size_t len) {
	struct ssl_store_s *ssl_ctx = (struct ssl_store_s *)ctx;
	int ret;

	ret = (int)recv(ssl_ctx->socket_fd, buf, len, 0);
	if (ret < 0) {
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			return MBEDTLS_ERR_SSL_WANT_READ;
		}
		if (errno == EPIPE || errno == ECONNRESET) {
			return MBEDTLS_ERR_NET_CONN_RESET;
		}
		return MBEDTLS_ERR_NET_RECV_FAILED;
	}
	return ret;
}

static void mbedtls_debug_callback(void *ctx, int level, const char *file, int line, const char *str) {
	(void)ctx;
	(void)level;
	LOG(SSL_LOG, NOTICE_LEVEL, 0, "mbedTLS [%s:%d]: %s", file, line, str);
}

static void print_mbedtls_error(const char *func, int ret) {
	char error_buf[100];
	mbedtls_strerror(ret, error_buf, sizeof(error_buf));
	LOG(SSL_LOG, ERROR_LEVEL, 0, "%s returned -0x%04x: %s", func, -ret, error_buf);
}

int initOpenSSL(void) {
	int ret;
	const char *keyfile;
	const char *cafile;

	LOG(SSL_LOG, NOTICE_LEVEL, 0, "using mbedTLS: %s", MBEDTLS_VERSION_STRING);

	/* Initialize structures */
	mbedtls_x509_crt_init(&server_cert);
	mbedtls_pk_init(&server_key);
	mbedtls_entropy_init(&entropy);
	mbedtls_ctr_drbg_init(&ctr_drbg);
	mbedtls_ssl_config_init(&ssl_conf);

	/* Seed the random number generator */
	ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
	                            (const unsigned char *)"libcwebui", 9);
	if (ret != 0) {
		print_mbedtls_error("mbedtls_ctr_drbg_seed", ret);
		return -1;
	}

	/* Load server certificate */
	keyfile = getConfigText("ssl_key_file");
	if (keyfile == NULL) {
		LOG(SSL_LOG, ERROR_LEVEL, 0, "ssl_key_file not configured");
		return -1;
	}

	LOG(SSL_LOG, NOTICE_LEVEL, 0, "SSL ssl_key_file %s", keyfile);

	ret = mbedtls_x509_crt_parse_file(&server_cert, keyfile);
	if (ret != 0) {
		print_mbedtls_error("mbedtls_x509_crt_parse_file (cert)", ret);
		return -1;
	}

	/* Load server private key - mbedTLS 2.x API (no RNG parameter) */
	ret = mbedtls_pk_parse_keyfile(&server_key, keyfile, NULL);
	if (ret != 0) {
		print_mbedtls_error("mbedtls_pk_parse_keyfile", ret);
		return -1;
	}

	/* Load CA certificate if configured */
	cafile = getConfigText("ssl_ca_list_file");
	if (cafile != NULL) {
		LOG(SSL_LOG, NOTICE_LEVEL, 0, "SSL ssl_ca_list_file %s", cafile);
		ret = mbedtls_x509_crt_parse_file(&server_cert, cafile);
		if (ret != 0) {
			print_mbedtls_error("mbedtls_x509_crt_parse_file (ca)", ret);
			/* Continue without CA - not fatal */
		}
	}

	/* Setup SSL config */
	ret = mbedtls_ssl_config_defaults(&ssl_conf,
	                                  MBEDTLS_SSL_IS_SERVER,
	                                  MBEDTLS_SSL_TRANSPORT_STREAM,
	                                  MBEDTLS_SSL_PRESET_DEFAULT);
	if (ret != 0) {
		print_mbedtls_error("mbedtls_ssl_config_defaults", ret);
		return -1;
	}

	mbedtls_ssl_conf_rng(&ssl_conf, mbedtls_ctr_drbg_random, &ctr_drbg);
	mbedtls_ssl_conf_dbg(&ssl_conf, mbedtls_debug_callback, NULL);
#ifdef _WEBSERVER_SSL_DEBUG_
	mbedtls_debug_set_threshold(4);  /* Enable debug output */
#endif

	/* Set certificate and key */
	ret = mbedtls_ssl_conf_own_cert(&ssl_conf, &server_cert, &server_key);
	if (ret != 0) {
		print_mbedtls_error("mbedtls_ssl_conf_own_cert", ret);
		return -1;
	}

	ssl_initialized = 1;
	return 0;
}

int VISIBLE_ATTR WebserverSSLTestKeyfile(char *keyfile) {
	mbedtls_x509_crt test_cert;
	mbedtls_pk_context test_key;
	int ret;
	int error = 0;

	mbedtls_x509_crt_init(&test_cert);
	mbedtls_pk_init(&test_key);

	ret = mbedtls_x509_crt_parse_file(&test_cert, keyfile);
	if (ret != 0) {
		LOG(SSL_LOG, ERROR_LEVEL, 0, "Can't read certificate: file %s", keyfile);
		error = 1;
	}

	if (!error) {
		/* mbedTLS 2.x - no RNG parameter needed */
		ret = mbedtls_pk_parse_keyfile(&test_key, keyfile, NULL);
		if (ret != 0) {
			LOG(SSL_LOG, ERROR_LEVEL, 0, "Can't read key: file %s", keyfile);
			error = 1;
		}
	}

	mbedtls_pk_free(&test_key);
	mbedtls_x509_crt_free(&test_cert);

	return error;
}

int WebserverSSLInit(socket_info *s) {
	struct ssl_store_s *ctx;
	int ret;

	if (!ssl_initialized) {
		LOG(SSL_LOG, ERROR_LEVEL, s->socket, "SSL not initialized");
		return -1;
	}

	ctx = (struct ssl_store_s *)WebserverMalloc(sizeof(struct ssl_store_s));
	if (ctx == NULL) {
		return -1;
	}

	memset(ctx, 0, sizeof(struct ssl_store_s));
	mbedtls_ssl_init(&ctx->ssl);

	/* Setup SSL context */
	ret = mbedtls_ssl_setup(&ctx->ssl, &ssl_conf);
	if (ret != 0) {
		print_mbedtls_error("mbedtls_ssl_setup", ret);
		WebserverFree(ctx);
		return -1;
	}

	/* Set socket and custom I/O callbacks */
	ctx->socket_fd = s->socket;
	mbedtls_ssl_set_bio(&ctx->ssl, ctx, ssl_send_callback, ssl_recv_callback, NULL);

	s->ssl_context = ctx;
	ctx->handshake_done = 0;

	return 0;
}

int WebserverSSLCloseSockets(socket_info *s) {
	struct ssl_store_s *ctx = s->ssl_context;

	if (ctx != NULL) {
		mbedtls_ssl_close_notify(&ctx->ssl);
		mbedtls_ssl_free(&ctx->ssl);
		WebserverFree(ctx);
		s->ssl_context = NULL;
	}
	return 0;
}

static void printSSLErrorQueue(socket_info *s) {
	(void)s;
	/* mbedTLS doesn't have an error queue like OpenSSL */
}

int WebserverSSLPending(socket_info *s) {
	struct ssl_store_s *ctx = s->ssl_context;
	if (ctx == NULL) return 0;

	size_t pending = mbedtls_ssl_get_bytes_avail(&ctx->ssl);
	return (pending > 0) ? 1 : 0;
}

int WebserverSSLAccept(socket_info *s) {
	struct ssl_store_s *ctx = s->ssl_context;
	int ret;

	if (ctx == NULL) {
		return SSL_PROTOCOL_ERROR;
	}

	if (ctx->handshake_done) {
		return SSL_ACCEPT_OK;
	}

	ret = mbedtls_ssl_handshake(&ctx->ssl);

	if (ret == 0) {
		ctx->handshake_done = 1;
		s->run_ssl_accept = 0;
#ifdef _WEBSERVER_CONNECTION_DEBUG_
		LOG(SSL_LOG, NOTICE_LEVEL, s->socket, "SSL handshake complete");
#endif
		return SSL_ACCEPT_OK;
	}

	if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
		return CLIENT_NO_MORE_DATA;
	}

	/* Suppress expected errors when clients reject self-signed certificates */
	if (ret == MBEDTLS_ERR_SSL_FATAL_ALERT_MESSAGE) {
		return SSL_PROTOCOL_ERROR;
	}

	print_mbedtls_error("mbedtls_ssl_handshake", ret);
	return SSL_PROTOCOL_ERROR;
}

int WebserverSSLRecvNonBlocking(socket_info *s, unsigned char *buf, unsigned int len, UNUSED_PARA int flags) {
	struct ssl_store_s *ctx = s->ssl_context;
	int ret;

	(void)flags;

	if (ctx == NULL) {
		return SSL_PROTOCOL_ERROR;
	}

	ret = mbedtls_ssl_read(&ctx->ssl, buf, len);

	if (ret > 0) {
		return ret;
	}

	if (ret == 0) {
		return CLIENT_DISCONNECTED;
	}

	if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
		return CLIENT_NO_MORE_DATA;
	}

	if (ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
		return CLIENT_DISCONNECTED;
	}

#ifdef _WEBSERVER_CONNECTION_DEBUG_
	print_mbedtls_error("mbedtls_ssl_read", ret);
#endif
	return SSL_PROTOCOL_ERROR;
}

SOCKET_SEND_STATUS WebserverSSLSendNonBlocking(socket_info *s, const unsigned char *buf, int len, UNUSED_PARA int flags, int *bytes_send) {
	struct ssl_store_s *ctx = s->ssl_context;
	int ret;

	(void)flags;

	if (ctx == NULL) {
		if (bytes_send) *bytes_send = 0;
		return SOCKET_SEND_SSL_ERROR;
	}

	ret = mbedtls_ssl_write(&ctx->ssl, buf, len);

	if (ret > 0) {
		if (bytes_send) *bytes_send = ret;
		if (ret == len) {
			return SOCKET_SEND_NO_MORE_DATA;
		}
		return SOCKET_SEND_SEND_BUFFER_FULL;
	}

	if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
		if (bytes_send) *bytes_send = 0;
		return SOCKET_SEND_SEND_BUFFER_FULL;
	}

#ifdef _WEBSERVER_CONNECTION_DEBUG_
	print_mbedtls_error("mbedtls_ssl_write", ret);
#endif
	if (bytes_send) *bytes_send = 0;
	return SOCKET_SEND_SSL_ERROR;
}

#endif /* WEBSERVER_USE_MBEDTLS2_CRYPTO */
