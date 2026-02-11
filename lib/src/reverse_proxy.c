/*

SPDX-License-Identifier: MPL-2.0

 libCWebUI
 Copyright (C) 2007 - 2019  Ramin Seyed-Moussavi

 Projekt URL : https://github.com/lordrasmus/libcwebui

 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at https://mozilla.org/MPL/2.0/. 

*/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>

#include "webserver.h"
#include "intern/reverse_proxy.h"

/*
 * Streaming ist implementiert - Daten werden sofort an den Client weitergeleitet
 * während sie vom Backend empfangen werden. Der Buffer wird nur für Zwischenspeicherung
 * verwendet wenn der Client-Socket blockiert (STREAMING_WAIT_WRITE).
 */
#define PROXY_BUFFER_SIZE       (64 * 1024)
#define PROXY_MAX_HEADER_SIZE   (8 * 1024)

/* Debug Level System für Reverse Proxy
 * 0 oder undefiniert: Keine Ausgaben
 * 1: Nur Fehler
 * 2: Fehler + wichtige Zustandsänderungen (connect, done)
 * 3: + Request/Response Details
 * 4: + Alle Zustandsänderungen
 * 5: + Detaillierte Byte-Level Infos
 * 6: + HTTP Header Dumps (Request und Response)
 */
#ifndef _WEBSERVER_PROXY_DEBUG_
#define _WEBSERVER_PROXY_DEBUG_ 0
#endif

/*
 * Static Variables
 */
static reverse_proxy_registration_t* g_proxy_registrations = NULL;

/*
 * Forward Declarations
 */
static void proxy_process_state(reverse_proxy_connection_t* proxy);
static void proxy_cleanup_connection(reverse_proxy_connection_t* proxy);
static int proxy_connect_backend(reverse_proxy_connection_t* proxy);
static int proxy_parse_response_header(reverse_proxy_connection_t* proxy);
static int proxy_find_header_end(const unsigned char* buffer, uint32_t len);
static int proxy_parse_content_length(const unsigned char* header, uint32_t header_len);
static int proxy_check_chunked_encoding(const unsigned char* header, uint32_t header_len);
static int proxy_check_websocket_upgrade(const unsigned char* header, uint32_t header_len);
static int proxy_get_status_code(const unsigned char* header);
static int proxy_inject_forwarded_headers(reverse_proxy_connection_t* proxy);
static int proxy_inject_connection_close(reverse_proxy_connection_t* proxy);
static int proxy_extract_host_header(const unsigned char* header, uint32_t header_len, char* host_buf, int host_buf_size);

#if _WEBSERVER_PROXY_DEBUG_ >= 2
/*
 * Timing Helper - returns milliseconds between two timespecs
 */
static double proxy_time_diff_ms(struct timespec* start, struct timespec* end) {
    double diff = (end->tv_sec - start->tv_sec) * 1000.0;
    diff += (end->tv_nsec - start->tv_nsec) / 1000000.0;
    return diff;
}
#endif

#if _WEBSERVER_PROXY_DEBUG_ >= 6
/*
 * Debug Helper - dump HTTP header line by line
 */
static void proxy_dump_header(const char* label, const unsigned char* buffer, uint32_t header_end) {
    uint32_t line_start = 0;
    uint32_t i;
    char line_buf[512];

    LOG(PROXY_LOG, NOTICE_LEVEL, 0, "=== %s ===", label);

    for (i = 0; i < header_end; i++) {
        if (buffer[i] == '\n') {
            uint32_t line_len = i - line_start;
            if (line_len > 0 && buffer[i-1] == '\r') {
                line_len--;
            }
            if (line_len > sizeof(line_buf) - 1) {
                line_len = sizeof(line_buf) - 1;
            }
            memcpy(line_buf, &buffer[line_start], line_len);
            line_buf[line_len] = '\0';
            LOG(PROXY_LOG, NOTICE_LEVEL, 0, "  %s", line_buf);
            line_start = i + 1;
        }
    }
    LOG(PROXY_LOG, NOTICE_LEVEL, 0, "=== END %s ===", label);
}
#endif

/*
 * Send 502 Bad Gateway error response directly on socket
 */
static void proxy_send_error_response(reverse_proxy_connection_t* proxy) {
    char body[512];
    char response[768];
    int body_len;
    int response_len;
    const char* backend_path = (proxy != NULL && proxy->backend_path != NULL) ? proxy->backend_path : "unknown";

    body_len = snprintf(body, sizeof(body),
        "<html><head><title>502 Bad Gateway</title></head>"
        "<body><h1>502 Bad Gateway</h1>"
        "<p>Backend service unavailable: %s</p></body></html>",
        backend_path);

    response_len = snprintf(response, sizeof(response),
        "HTTP/1.1 502 Bad Gateway\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s",
        body_len, body);

    if (proxy != NULL && proxy->client_sock != NULL) {
        int fd = proxy->client_sock->socket;
        if (fd >= 0) {
            ssize_t sent = send(fd, response, response_len, 0);
            if (sent < 0) {
                LOG(PROXY_LOG, ERROR_LEVEL, fd, "Failed to send error response: %s", strerror(errno));
            }
        }
    }
}

/*
 * Registration Functions
 */

void reverse_proxy_register_uds_internal(const char* method, const char* url_prefix, const char* backend_path) {
    reverse_proxy_registration_t* reg;
    
    if (url_prefix == NULL || backend_path == NULL) {
        LOG(PROXY_LOG, ERROR_LEVEL, 0, "%s", "Invalid parameters for registration");
        return;
    }

    reg = WebserverMalloc(sizeof(reverse_proxy_registration_t));
    if (reg == NULL) {
        LOG(PROXY_LOG, ERROR_LEVEL, 0, "%s", "Failed to allocate registration");
        return;
    }
    
    memset(reg, 0, sizeof(reverse_proxy_registration_t));
    
    if (method != NULL) {
        reg->method = strdup(method);
    } else {
        reg->method = strdup("*");
    }
    
    reg->url_prefix = strdup(url_prefix);
    reg->url_prefix_len = strlen(url_prefix);
    reg->backend_path = strdup(backend_path);
    reg->type = PROXY_TYPE_UDS;
    reg->forward_websocket = 1;  /* Default: WebSockets weiterleiten */
    reg->next = NULL;
    
    /* Ans Ende der Liste anhängen */
    if (g_proxy_registrations == NULL) {
        g_proxy_registrations = reg;
    } else {
        reverse_proxy_registration_t* p = g_proxy_registrations;
        while (p->next != NULL) {
            p = p->next;
        }
        p->next = reg;
    }
    
#if _WEBSERVER_PROXY_DEBUG_ >= 2
    LOG(PROXY_LOG, NOTICE_LEVEL, 0, "Registered %s %s -> %s",
              reg->method, reg->url_prefix, reg->backend_path);
#endif
}

/*
 * Find matching proxy registration
 */
static reverse_proxy_registration_t* proxy_find_registration(const char* method, const char* url) {
    reverse_proxy_registration_t* reg = g_proxy_registrations;
    
    while (reg != NULL) {
        /* Check URL prefix */
        if (strncmp(url, reg->url_prefix, reg->url_prefix_len) == 0) {
            /* Check method (wenn nicht "*") */
            if (strcmp(reg->method, "*") == 0 || 
                strcasecmp(reg->method, method) == 0) {
                return reg;
            }
        }
        reg = reg->next;
    }
    
    return NULL;
}

/*
 * Extract method and URL from first line of HTTP request
 */
static int proxy_parse_request_line(const char* buffer, int len, 
                                    char* method, int method_size,
                                    char* url, int url_size) {
    int i = 0;
    int pos = 0;
    
    /* Parse method */
    while (i < len && buffer[i] != ' ' && pos < method_size - 1) {
        method[pos++] = buffer[i++];
    }
    method[pos] = '\0';
    
    if (i >= len || buffer[i] != ' ') {
        return -1;
    }
    i++;  /* Skip space */
    
    /* Parse URL */
    pos = 0;
    while (i < len && buffer[i] != ' ' && buffer[i] != '?' && pos < url_size - 1) {
        url[pos++] = buffer[i++];
    }
    url[pos] = '\0';
    
    return 0;
}

/*
 * Create new proxy connection
 */
static reverse_proxy_connection_t* proxy_create_connection(socket_info* client_sock, 
                                                           reverse_proxy_registration_t* reg) {
    reverse_proxy_connection_t* proxy;
    
    proxy = WebserverMalloc(sizeof(reverse_proxy_connection_t));
    if (proxy == NULL) {
        return NULL;
    }
    
    memset(proxy, 0, sizeof(reverse_proxy_connection_t));

#if _WEBSERVER_PROXY_DEBUG_ >= 2
    /* Timing starten */
    clock_gettime(CLOCK_MONOTONIC, &proxy->time_start);
#endif

    proxy->state = PROXY_STATE_RECV_CLIENT_HEADER;
    proxy->client_sock = client_sock;
    proxy->backend_fd = -1;
    proxy->backend_path = strdup(reg->backend_path);
    
    /* Request Buffer allokieren */
    proxy->request_buffer = WebserverMalloc(PROXY_BUFFER_SIZE);
    if (proxy->request_buffer == NULL) {
        WebserverFree(proxy);
        return NULL;
    }
    proxy->request_buffer_size = PROXY_BUFFER_SIZE;
    proxy->request_buffer_pos = 0;
    
    /* Response Buffer allokieren */
    proxy->response_buffer = WebserverMalloc(PROXY_BUFFER_SIZE);
    if (proxy->response_buffer == NULL) {
        WebserverFree(proxy->request_buffer);
        WebserverFree(proxy);
        return NULL;
    }
    proxy->response_buffer_size = PROXY_BUFFER_SIZE;
    proxy->response_buffer_pos = 0;
    
    return proxy;
}

/*
 * Main entry point - called from header_parser.c
 */
int checkReverseProxy(socket_info* s, char* buffer, int length) {
    char method[16];
    char url[512];
    reverse_proxy_registration_t* reg;
    reverse_proxy_connection_t* proxy;
    
    /* Wurde bereits geprüft? */
    if (s->reverse_proxy_checked == 1) {
        return 1;  /* Kein Proxy */
    }
    
    /* Request Line parsen */
    if (proxy_parse_request_line(buffer, length, method, sizeof(method), 
                                  url, sizeof(url)) < 0) {
        return 1;  /* Parse Error, normales Handling */
    }
    
#if _WEBSERVER_PROXY_DEBUG_ >= 7
    LOG(PROXY_LOG, NOTICE_LEVEL, 0, "checkReverseProxy: %s %s", method, url);
#endif

    /* Passende Registration finden */
    reg = proxy_find_registration(method, url);
    if (reg == NULL) {
        s->reverse_proxy_checked = 1;
        return 1;  /* Keine Registration gefunden */
    }

#if _WEBSERVER_PROXY_DEBUG_ >= 3
    LOG(PROXY_LOG, NOTICE_LEVEL, 0, "Found proxy: %s -> %s", url, reg->backend_path);
#endif

    /* Proxy Connection erstellen */
    proxy = proxy_create_connection(s, reg);
    if (proxy == NULL) {
        LOG(PROXY_LOG, ERROR_LEVEL, 0, "%s", "Failed to create proxy connection");
        s->reverse_proxy_error = 1;
        return 1;
    }

    /* URL für Logging speichern */
    snprintf(proxy->request_url, sizeof(proxy->request_url), "%s", url);

    /* Bisherige Daten in Request Buffer kopieren */
    if (length > 0 && (uint32_t)length < proxy->request_buffer_size) {
        memcpy(proxy->request_buffer, buffer, length);
        proxy->request_buffer_pos = length;
    }

    /* Client Socket für Proxy konfigurieren */
    s->extern_handle = reverse_proxy_client_handler;
    s->extern_handle_data_ptr = proxy;
    s->reverse_proxy_checked = 1;

    /* Sofort den vorhandenen Request verarbeiten */
    proxy_process_state(proxy);

    /* Falls noch nicht fertig, Event für weiteres Lesen registrieren */
    if (proxy->state == PROXY_STATE_RECV_CLIENT_HEADER ||
        proxy->state == PROXY_STATE_RECV_CLIENT_BODY) {
        addEventSocketReadPersist(s);
    }

    return 0;  /* Proxy übernimmt */
}

/*
 * Client Event Handler
 */
void reverse_proxy_client_handler(int fd, void* ptr) {
    reverse_proxy_connection_t* proxy = (reverse_proxy_connection_t*)ptr;
    int bytes_read;
    
    (void)fd;
    
    if (proxy == NULL) {
        LOG(PROXY_LOG, ERROR_LEVEL, 0, "%s", "proxy is NULL in client handler");
        return;
    }

    switch (proxy->state) {
        case PROXY_STATE_RECV_CLIENT_HEADER:
        case PROXY_STATE_RECV_CLIENT_BODY:
            /* Weitere Daten vom Client lesen (mit SSL-Unterstützung für Client-Socket) */
            if (proxy->request_buffer_pos < proxy->request_buffer_size) {
                bytes_read = WebserverRecv(proxy->client_sock,
                                  &proxy->request_buffer[proxy->request_buffer_pos],
                                  proxy->request_buffer_size - proxy->request_buffer_pos,
                                  0);

                if (bytes_read <= 0) {
#if _WEBSERVER_PROXY_DEBUG_ >= 3
                    if (bytes_read == 0) {
                        LOG(PROXY_LOG, NOTICE_LEVEL, 0, "%s", "Client disconnected");
                    } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
                        LOG(PROXY_LOG, NOTICE_LEVEL, 0, "Client recv error: %s", strerror(errno));
                    }
#endif
                    if (bytes_read == 0 || (errno != EAGAIN && errno != EWOULDBLOCK)) {
                        proxy->state = PROXY_STATE_ERROR;
                        proxy_process_state(proxy);
                        return;
                    }
                    return;  /* EAGAIN - später nochmal versuchen */
                }

                /* Defensive check: recv should never return more than requested */
                /* Also prevents overflow: if bytes_read fits in remaining space, addition is safe */
                if (proxy->request_buffer_pos > proxy->request_buffer_size ||
                    (uint32_t)bytes_read > proxy->request_buffer_size - proxy->request_buffer_pos) {
                    LOG(PROXY_LOG, ERROR_LEVEL, 0, "recv returned %d bytes, but only %u space available",
                        bytes_read, proxy->request_buffer_size - proxy->request_buffer_pos);
                    proxy->state = PROXY_STATE_ERROR;
                    proxy_process_state(proxy);
                    return;
                }

                proxy->request_buffer_pos += (uint32_t)bytes_read;
#if _WEBSERVER_PROXY_DEBUG_ >= 5
                LOG(PROXY_LOG, NOTICE_LEVEL, 0, "Received %d bytes from client, total=%d",
                          bytes_read, proxy->request_buffer_pos);
#endif
            }
            break;

        case PROXY_STATE_STREAMING_WAIT_WRITE:
            /* Client-Socket ist wieder beschreibbar - Write-Event entfernen und Streaming fortsetzen */
#if _WEBSERVER_PROXY_DEBUG_ >= 4
            LOG(PROXY_LOG, NOTICE_LEVEL, 0, "%s", "STREAMING_WAIT_WRITE - client writable again");
#endif
            delEventSocketWritePersist(proxy->client_sock);
            proxy->state = PROXY_STATE_STREAMING;
            proxy_process_state(proxy);
            return;

        case PROXY_STATE_WEBSOCKET_TUNNEL:
            /* WebSocket Tunnel: Daten vom Client zum Backend */
            {
                unsigned char tunnel_buf[4096];
                /* Client-Socket: WebserverRecv für SSL-Unterstützung */
                bytes_read = WebserverRecv(proxy->client_sock, tunnel_buf, sizeof(tunnel_buf), 0);

                if (bytes_read <= 0) {
                    if (bytes_read == 0 || (errno != EAGAIN && errno != EWOULDBLOCK)) {
#if _WEBSERVER_PROXY_DEBUG_ >= 3
                        LOG(PROXY_LOG, NOTICE_LEVEL, 0, "%s", "WebSocket tunnel: client closed");
#endif
                        proxy->state = PROXY_STATE_DONE;
                        proxy_process_state(proxy);
                        return;
                    }
                    return;
                }

                /* Direkt ans Backend weiterleiten (UDS - kein SSL nötig) */
                if (proxy->backend_fd >= 0) {
                    int sent = send(proxy->backend_fd, tunnel_buf, bytes_read, 0);
                    if (sent < 0) {
                        LOG(PROXY_LOG, ERROR_LEVEL, 0, "%s", "WebSocket tunnel: backend send error");
                        proxy->state = PROXY_STATE_ERROR;
                        proxy_process_state(proxy);
                        return;
                    }
#if _WEBSERVER_PROXY_DEBUG_ >= 5
                    LOG(PROXY_LOG, NOTICE_LEVEL, 0, "WebSocket tunnel: forwarded %d bytes to backend", sent);
#endif
                }
            }
            return;

        default:
            break;
    }
    
    proxy_process_state(proxy);
}

/*
 * Backend Event Handler
 */
void reverse_proxy_backend_handler(int fd, void* ptr) {
    reverse_proxy_connection_t* proxy = (reverse_proxy_connection_t*)ptr;
    int bytes_read;
    
    (void)fd;
    
    if (proxy == NULL) {
        LOG(PROXY_LOG, ERROR_LEVEL, 0, "%s", "proxy is NULL in backend handler");
        return;
    }

#if _WEBSERVER_PROXY_DEBUG_ >= 5
    LOG(PROXY_LOG, NOTICE_LEVEL, proxy->backend_fd, "backend_handler called, state=%d", proxy->state);
#endif

    switch (proxy->state) {
        case PROXY_STATE_RECV_BACKEND_HEADER:
            /* Daten vom Backend lesen - warten auf kompletten Header */
            if (proxy->response_buffer_pos < proxy->response_buffer_size) {
                bytes_read = recv(proxy->backend_fd,
                                  &proxy->response_buffer[proxy->response_buffer_pos],
                                  proxy->response_buffer_size - proxy->response_buffer_pos,
                                  0);
#if _WEBSERVER_PROXY_DEBUG_ >= 5
                LOG(PROXY_LOG, NOTICE_LEVEL, proxy->backend_fd, "backend recv (header): %d bytes", bytes_read);
#endif
                if (bytes_read <= 0) {
                    if (bytes_read == 0) {
#if _WEBSERVER_PROXY_DEBUG_ >= 3
                        LOG(PROXY_LOG, NOTICE_LEVEL, proxy->backend_fd, "%s", "Backend closed during header");
#endif
                        proxy->state = PROXY_STATE_ERROR;
                    } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
                        LOG(PROXY_LOG, ERROR_LEVEL, proxy->backend_fd, "Backend recv error: %s", strerror(errno));
                        proxy->state = PROXY_STATE_ERROR;
                    } else {
                        return;  /* EAGAIN - später nochmal */
                    }
                } else {
#if _WEBSERVER_PROXY_DEBUG_ >= 2
                    /* Timing: Erste Response-Bytes */
                    if (proxy->response_buffer_pos == 0) {
                        clock_gettime(CLOCK_MONOTONIC, &proxy->time_first_response);
                    }
#endif
                    /* Defensive check: recv should never return more than requested */
                    /* Also prevents overflow: if bytes_read fits in remaining space, addition is safe */
                    if (proxy->response_buffer_pos > proxy->response_buffer_size ||
                        (uint32_t)bytes_read > proxy->response_buffer_size - proxy->response_buffer_pos) {
                        LOG(PROXY_LOG, ERROR_LEVEL, proxy->backend_fd, "recv returned %d bytes, but only %u space available",
                            bytes_read, proxy->response_buffer_size - proxy->response_buffer_pos);
                        proxy->state = PROXY_STATE_ERROR;
                        proxy_process_state(proxy);
                        return;
                    }
                    proxy->response_buffer_pos += (uint32_t)bytes_read;
#if _WEBSERVER_PROXY_DEBUG_ >= 5
                    LOG(PROXY_LOG, NOTICE_LEVEL, proxy->backend_fd, "Received %d bytes from backend, total=%d",
                              bytes_read, proxy->response_buffer_pos);
#endif
                }
            }
            break;

        case PROXY_STATE_STREAMING:
            /* Streaming: Daten empfangen und sofort weiterleiten */
            {
                /* Noch ungesendete Daten im Buffer? Erst senden! */
                if (proxy->response_buffer_pos > proxy->send_pos) {
#if _WEBSERVER_PROXY_DEBUG_ >= 5
                    LOG(PROXY_LOG, NOTICE_LEVEL, 0, "%s", "STREAMING: buffer not empty, sending first");
#endif
                    proxy_process_state(proxy);
                    return;
                }

                /* Bei CONTENT_LENGTH prüfen ob schon alles empfangen */
                if (proxy->response_body_mode == BODY_MODE_CONTENT_LENGTH) {
                    if (proxy->response_body_sent >= proxy->response_content_length) {
#if _WEBSERVER_PROXY_DEBUG_ >= 4
                        LOG(PROXY_LOG, NOTICE_LEVEL, 0, "%s", "STREAMING: all data sent, done");
#endif
                        proxy->state = PROXY_STATE_DONE;
                        proxy_process_state(proxy);
                        return;
                    }
                }

                /* Buffer ist leer, neu vom Backend lesen */
                proxy->response_buffer_pos = 0;
                proxy->send_pos = 0;

                bytes_read = recv(proxy->backend_fd,
                                  proxy->response_buffer,
                                  proxy->response_buffer_size, 0);
#if _WEBSERVER_PROXY_DEBUG_ >= 5
                LOG(PROXY_LOG, NOTICE_LEVEL, proxy->backend_fd, "backend recv (streaming): %d bytes", bytes_read);
#endif
                if (bytes_read <= 0) {
                    if (bytes_read == 0) {
#if _WEBSERVER_PROXY_DEBUG_ >= 4
                        LOG(PROXY_LOG, NOTICE_LEVEL, proxy->backend_fd, "%s", "Backend closed during streaming");
#endif
                        /* Prüfen ob alles empfangen wurde */
                        if (proxy->response_body_mode == BODY_MODE_CLOSE ||
                            proxy->response_body_sent >= proxy->response_content_length) {
                            proxy->state = PROXY_STATE_DONE;
                        } else {
                            LOG(PROXY_LOG, WARNING_LEVEL, proxy->backend_fd, "Backend closed prematurely, sent=%lu, expected=%lu",
                                      (unsigned long)proxy->response_body_sent,
                                      (unsigned long)proxy->response_content_length);
                            proxy->state = PROXY_STATE_ERROR;
                        }
                        proxy_process_state(proxy);
                        return;
                    } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
                        LOG(PROXY_LOG, ERROR_LEVEL, proxy->backend_fd, "Backend recv error: %s", strerror(errno));
                        proxy->state = PROXY_STATE_ERROR;
                        proxy_process_state(proxy);
                        return;
                    }
                    return;  /* EAGAIN - später nochmal */
                }

                proxy->response_buffer_pos = bytes_read;
                /* Sofort weiterleiten */
                proxy_process_state(proxy);
                return;
            }
            break;
            
        case PROXY_STATE_WEBSOCKET_TUNNEL:
            /* WebSocket Tunnel: Daten vom Backend zum Client */
            {
                unsigned char tunnel_buf[4096];
#if _WEBSERVER_PROXY_DEBUG_ >= 5
                LOG(PROXY_LOG, NOTICE_LEVEL, proxy->backend_fd, "%s", "WebSocket tunnel: backend handler called");
#endif
                /* Backend: direktes recv (UDS - kein SSL) */
                bytes_read = recv(proxy->backend_fd, tunnel_buf, sizeof(tunnel_buf), 0);

                if (bytes_read <= 0) {
                    if (bytes_read == 0 || (errno != EAGAIN && errno != EWOULDBLOCK)) {
#if _WEBSERVER_PROXY_DEBUG_ >= 3
                        LOG(PROXY_LOG, NOTICE_LEVEL, proxy->backend_fd, "%s", "WebSocket tunnel: backend closed");
#endif
                        proxy->state = PROXY_STATE_DONE;
                        proxy_process_state(proxy);
                        return;
                    }
                    return;
                }

                /* Client-Socket: WebserverSend für SSL-Unterstützung */
                int sent = 0;
                SOCKET_SEND_STATUS status = WebserverSend(proxy->client_sock,
                                                          tunnel_buf, bytes_read, 0, &sent);
                if (status == SOCKET_SEND_CLIENT_DISCONNECTED) {
                    LOG(PROXY_LOG, ERROR_LEVEL, proxy->backend_fd, "%s", "WebSocket tunnel: client disconnected");
                    proxy->state = PROXY_STATE_ERROR;
                    proxy_process_state(proxy);
                    return;
                }
#if _WEBSERVER_PROXY_DEBUG_ >= 5
                LOG(PROXY_LOG, NOTICE_LEVEL, proxy->backend_fd, "WebSocket tunnel: forwarded %d bytes to client", sent);
#endif
            }
            return;

        default:
            break;
    }

    proxy_process_state(proxy);
}

/*
 * State Machine Processing
 */
static void proxy_process_state(reverse_proxy_connection_t* proxy) {
    int ret;

    switch (proxy->state) {
        case PROXY_STATE_RECV_CLIENT_HEADER:
            /* Prüfen ob Header komplett */
            ret = proxy_find_header_end(proxy->request_buffer, proxy->request_buffer_pos);
            if (ret < 0) {
                /* Header noch nicht komplett, weiter lesen */
                return;
            }
            
            proxy->request_header_end = ret;
#if _WEBSERVER_PROXY_DEBUG_ >= 4
            LOG(PROXY_LOG, NOTICE_LEVEL, 0, "Client header complete at pos %d", ret);
#endif
            /* Content-Length oder Chunked? */
            {
                int parsed_content_length = proxy_parse_content_length(
                    proxy->request_buffer, proxy->request_header_end);

                if (parsed_content_length > 0) {
                    proxy->request_body_mode = BODY_MODE_CONTENT_LENGTH;
                    proxy->request_content_length = (uint64_t)parsed_content_length;
                    proxy->request_body_received = proxy->request_buffer_pos - proxy->request_header_end;
#if _WEBSERVER_PROXY_DEBUG_ >= 4
                    LOG(PROXY_LOG, NOTICE_LEVEL, 0, "Request Content-Length: %lu, received: %lu",
                              (unsigned long)proxy->request_content_length,
                              (unsigned long)proxy->request_body_received);
#endif
                    if (proxy->request_body_received < proxy->request_content_length) {
                        proxy->state = PROXY_STATE_RECV_CLIENT_BODY;
                        return;
                    }
                } else if (proxy_check_chunked_encoding(proxy->request_buffer,
                                                         proxy->request_header_end)) {
                    proxy->request_body_mode = BODY_MODE_CHUNKED;
                    proxy->request_content_length = 0;
                    /* TODO: Chunked Request Body handling */
#if _WEBSERVER_PROXY_DEBUG_ >= 3
                    LOG(PROXY_LOG, NOTICE_LEVEL, 0, "%s", "Request uses chunked encoding (not fully implemented)");
#endif
                } else {
                    /* Kein Body oder Content-Length: 0 */
                    proxy->request_body_mode = BODY_MODE_NONE;
                    proxy->request_content_length = 0;
                }
            }

            proxy->state = PROXY_STATE_CONNECT_BACKEND;
            proxy_process_state(proxy);
            break;

        case PROXY_STATE_RECV_CLIENT_BODY:
            /* Prüfen ob Body komplett */
            proxy->request_body_received = proxy->request_buffer_pos - proxy->request_header_end;
#if _WEBSERVER_PROXY_DEBUG_ >= 5
            LOG(PROXY_LOG, NOTICE_LEVEL, 0, "Body received: %lu of %lu",
                      (unsigned long)proxy->request_body_received,
                      (unsigned long)proxy->request_content_length);
#endif
            if (proxy->request_body_received >= proxy->request_content_length) {
                proxy->state = PROXY_STATE_CONNECT_BACKEND;
                proxy_process_state(proxy);
            }
            break;

        case PROXY_STATE_CONNECT_BACKEND:
#if _WEBSERVER_PROXY_DEBUG_ >= 3
            LOG(PROXY_LOG, NOTICE_LEVEL, 0, "Connecting to backend: %s", proxy->backend_path);
#endif
            ret = proxy_connect_backend(proxy);
            if (ret < 0) {
                proxy->state = PROXY_STATE_ERROR;
                proxy_process_state(proxy);
                return;
            }
#if _WEBSERVER_PROXY_DEBUG_ >= 2
            /* Timing: Backend verbunden */
            clock_gettime(CLOCK_MONOTONIC, &proxy->time_backend_connected);
#endif
#if _WEBSERVER_PROXY_DEBUG_ >= 3
            LOG(PROXY_LOG, NOTICE_LEVEL, 0, "Backend connected, fd=%d", proxy->backend_fd);
#endif
            /* Inject X-Forwarded-* headers before sending to backend */
            if (proxy_inject_forwarded_headers(proxy) < 0) {
                LOG(PROXY_LOG, ERROR_LEVEL, 0, "%s", "Failed to inject forwarded headers");
                proxy->state = PROXY_STATE_ERROR;
                proxy_process_state(proxy);
                return;
            }

#if _WEBSERVER_PROXY_DEBUG_ >= 6
            proxy_dump_header("REQUEST TO BACKEND", proxy->request_buffer, proxy->request_header_end);
#endif

            proxy->state = PROXY_STATE_SEND_TO_BACKEND;
            proxy_process_state(proxy);
            break;
            
        case PROXY_STATE_SEND_TO_BACKEND:
            /* Request an Backend senden */
            {
                int to_send = proxy->request_buffer_pos - proxy->send_pos;
                int sent = send(proxy->backend_fd,
                               &proxy->request_buffer[proxy->send_pos],
                               to_send, 0);

                if (sent < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        return;  /* Später nochmal */
                    }
                    LOG(PROXY_LOG, ERROR_LEVEL, 0, "Backend send error: %s", strerror(errno));
                    proxy->state = PROXY_STATE_ERROR;
                    proxy_process_state(proxy);
                    return;
                }

                proxy->send_pos += sent;
#if _WEBSERVER_PROXY_DEBUG_ >= 5
                LOG(PROXY_LOG, NOTICE_LEVEL, 0, "Sent %d bytes to backend, total=%d/%d",
                          sent, proxy->send_pos, proxy->request_buffer_pos);
#endif
                
                if (proxy->send_pos >= proxy->request_buffer_pos) {
                    /* Alles gesendet */
#if _WEBSERVER_PROXY_DEBUG_ >= 2
                    /* Timing: Request vollständig gesendet */
                    clock_gettime(CLOCK_MONOTONIC, &proxy->time_request_sent);
#endif

                    proxy->send_pos = 0;
                    proxy->state = PROXY_STATE_RECV_BACKEND_HEADER;
                    
                    /* Backend Socket Event registrieren */
                    proxy->backend_sock->extern_handle = reverse_proxy_backend_handler;
                    proxy->backend_sock->extern_handle_data_ptr = proxy;
                    addEventSocketReadPersist(proxy->backend_sock);
                    
                    /* Client Socket Event deaktivieren (erstmal) */
                    delEventSocketReadPersist(proxy->client_sock);
                }
            }
            break;
            
        case PROXY_STATE_RECV_BACKEND_HEADER:
            /* Prüfen ob Response Header komplett */
            ret = proxy_find_header_end(proxy->response_buffer, proxy->response_buffer_pos);
            if (ret < 0) {
                return;  /* Header noch nicht komplett */
            }

            proxy->response_header_end = ret;
            proxy->response_header_complete = 1;
#if _WEBSERVER_PROXY_DEBUG_ >= 4
            LOG(PROXY_LOG, NOTICE_LEVEL, proxy->backend_fd, "Backend response header complete at pos %d", ret);
#endif
            /* Response Header parsen */
            proxy_parse_response_header(proxy);

#if _WEBSERVER_PROXY_DEBUG_ >= 6
            proxy_dump_header("RESPONSE FROM BACKEND", proxy->response_buffer, proxy->response_header_end);
#endif

            /* WebSocket Upgrade? -> Tunnel Mode */
            if (proxy->response_is_websocket_upgrade) {
#if _WEBSERVER_PROXY_DEBUG_ >= 3
                LOG(PROXY_LOG, NOTICE_LEVEL, proxy->backend_fd, "%s", "WebSocket upgrade detected");
#endif
                /* Header senden, dann Tunnel - KEIN Connection: close bei WebSockets! */
                proxy->state = PROXY_STATE_STREAMING;
                proxy_process_state(proxy);
                return;
            }

            /* Connection: close injizieren für sauberes Verbindungsende (nicht bei WebSockets) */
            proxy_inject_connection_close(proxy);

            /* Body Mode bestimmen */
            {
                int parsed_content_length = proxy_parse_content_length(
                    proxy->response_buffer, proxy->response_header_end);

                /* Body Mode bestimmen
                 * HTTP 204 (No Content) und 304 (Not Modified) haben nie einen Body
                 * Content-Length: 0 bedeutet auch kein Body
                 * parsed_content_length >= 0 bedeutet Content-Length Header vorhanden
                 * parsed_content_length == -1 bedeutet kein Content-Length Header */
                if (proxy->response_status_code == 204 || proxy->response_status_code == 304) {
                    proxy->response_body_mode = BODY_MODE_CONTENT_LENGTH;
                    proxy->response_content_length = 0;
                } else if (parsed_content_length >= 0) {
                    /* Content-Length Header vorhanden (auch wenn 0) */
                    proxy->response_body_mode = BODY_MODE_CONTENT_LENGTH;
                    proxy->response_content_length = (uint64_t)parsed_content_length;
                } else if (proxy_check_chunked_encoding(proxy->response_buffer,
                                                         proxy->response_header_end)) {
                    proxy->response_body_mode = BODY_MODE_CHUNKED;
                    proxy->response_content_length = 0;
#if _WEBSERVER_PROXY_DEBUG_ >= 3
                    LOG(PROXY_LOG, NOTICE_LEVEL, proxy->backend_fd, "%s", "Response uses chunked encoding");
#endif
                } else {
                    /* Kein Content-Length, kein Chunked -> read until close */
                    proxy->response_body_mode = BODY_MODE_CLOSE;
                    proxy->response_content_length = 0;
                }
            }

            proxy->response_body_received = proxy->response_buffer_pos - proxy->response_header_end;
#if _WEBSERVER_PROXY_DEBUG_ >= 4
            LOG(PROXY_LOG, NOTICE_LEVEL, proxy->backend_fd, "Response Content-Length: %lu, body in buffer: %lu",
                      (unsigned long)proxy->response_content_length,
                      (unsigned long)proxy->response_body_received);
#endif
            /* Wenn bei CONTENT_LENGTH alle Daten bereits empfangen, Backend-Events deregistrieren */
            if (proxy->response_body_mode == BODY_MODE_CONTENT_LENGTH &&
                proxy->response_body_received >= proxy->response_content_length) {
#if _WEBSERVER_PROXY_DEBUG_ >= 4
                LOG(PROXY_LOG, NOTICE_LEVEL, proxy->backend_fd, "%s", "All body data already received");
#endif
                delEventSocketReadPersist(proxy->backend_sock);
            }

            /* Streaming starten - Header und vorhandene Body-Daten sofort senden */
            proxy->state = PROXY_STATE_STREAMING;
            proxy->send_pos = 0;
            proxy->response_header_sent = 0;
            proxy->response_body_sent = 0;
            proxy_process_state(proxy);
            break;

        case PROXY_STATE_STREAMING:
            /* Streaming: Daten sofort an Client weiterleiten */
            {
                int to_send;
                int sent = 0;
                SOCKET_SEND_STATUS status;

                /* Noch Header zu senden? */
                if (!proxy->response_header_sent) {
                    while (proxy->send_pos < proxy->response_header_end) {
                        to_send = proxy->response_header_end - proxy->send_pos;
                        status = WebserverSend(proxy->client_sock,
                                   &proxy->response_buffer[proxy->send_pos],
                                   to_send, 0, &sent);
#if _WEBSERVER_PROXY_DEBUG_ >= 5
                        LOG(PROXY_LOG, NOTICE_LEVEL, proxy->backend_fd, "STREAMING header: sent %d/%d bytes, status=%d", sent, to_send, status);
#endif
                        if (status == SOCKET_SEND_SEND_BUFFER_FULL) {
                            proxy->state = PROXY_STATE_STREAMING_WAIT_WRITE;
                            addEventSocketWritePersist(proxy->client_sock);
                            return;
                        }
                        if (status == SOCKET_SEND_CLIENT_DISCONNECTED) {
                            proxy->state = PROXY_STATE_ERROR;
                            proxy_process_state(proxy);
                            return;
                        }

                        proxy->send_pos += sent;
                        if (sent == 0) {
                            /* Keine Daten gesendet, später nochmal versuchen */
                            proxy->state = PROXY_STATE_STREAMING_WAIT_WRITE;
                            addEventSocketWritePersist(proxy->client_sock);
                            return;
                        }
                    }
                    proxy->response_header_sent = 1;
#if _WEBSERVER_PROXY_DEBUG_ >= 4
                    LOG(PROXY_LOG, NOTICE_LEVEL, proxy->backend_fd, "%s", "STREAMING: header sent, starting body");
#endif
                    /* Bei WebSocket-Upgrade sofort in Tunnel-Mode wechseln */
                    if (proxy->response_is_websocket_upgrade) {
#if _WEBSERVER_PROXY_DEBUG_ >= 3
                        LOG(PROXY_LOG, NOTICE_LEVEL, proxy->backend_fd, "%s", "Entering WebSocket tunnel mode");
#endif
                        proxy->state = PROXY_STATE_WEBSOCKET_TUNNEL;
                        /* Events müssen erst entfernt werden bevor sie neu hinzugefügt werden (libevent requirement) */
                        delEventSocketAll(proxy->client_sock);
                        delEventSocketAll(proxy->backend_sock);
                        addEventSocketReadPersist(proxy->client_sock);
                        addEventSocketReadPersist(proxy->backend_sock);
                        return;
                    }
                }

                /* Body-Daten aus Buffer senden - in Schleife bis Buffer leer oder BUFFER_FULL */
                while (proxy->send_pos < proxy->response_buffer_pos) {
                    to_send = proxy->response_buffer_pos - proxy->send_pos;
                    status = WebserverSend(proxy->client_sock,
                               &proxy->response_buffer[proxy->send_pos],
                               to_send, 0, &sent);

                    if (status == SOCKET_SEND_SEND_BUFFER_FULL) {
                        proxy->state = PROXY_STATE_STREAMING_WAIT_WRITE;
                        addEventSocketWritePersist(proxy->client_sock);
                        return;
                    }
                    if (status == SOCKET_SEND_CLIENT_DISCONNECTED) {
                        proxy->state = PROXY_STATE_ERROR;
                        proxy_process_state(proxy);
                        return;
                    }

                    if (sent == 0) {
                        /* Keine Daten gesendet, später nochmal versuchen */
                        proxy->state = PROXY_STATE_STREAMING_WAIT_WRITE;
                        addEventSocketWritePersist(proxy->client_sock);
                        return;
                    }

                    proxy->send_pos += sent;
                    proxy->response_body_sent += sent;
#if _WEBSERVER_PROXY_DEBUG_ >= 5
                    LOG(PROXY_LOG, NOTICE_LEVEL, proxy->backend_fd, "STREAMING body: sent %d bytes, total: %lu",
                              sent, (unsigned long)proxy->response_body_sent);
#endif
                }

                /* Buffer ist leer - zurücksetzen für nächsten Backend-recv */
                proxy->response_buffer_pos = 0;
                proxy->send_pos = 0;

                /* Prüfen ob Response komplett */
                if (proxy->response_body_mode == BODY_MODE_CONTENT_LENGTH) {
                    if (proxy->response_body_sent >= proxy->response_content_length) {
#if _WEBSERVER_PROXY_DEBUG_ >= 3
                        LOG(PROXY_LOG, NOTICE_LEVEL, proxy->backend_fd, "STREAMING: all body sent (%lu bytes)",
                                  (unsigned long)proxy->response_body_sent);
#endif
                        if (proxy->response_is_websocket_upgrade) {
#if _WEBSERVER_PROXY_DEBUG_ >= 3
                            LOG(PROXY_LOG, NOTICE_LEVEL, proxy->backend_fd, "%s", "Entering WebSocket tunnel mode");
#endif
                            proxy->state = PROXY_STATE_WEBSOCKET_TUNNEL;
                            addEventSocketReadPersist(proxy->client_sock);
                            addEventSocketReadPersist(proxy->backend_sock);
                            return;
                        }

                        proxy->state = PROXY_STATE_DONE;
                        proxy_process_state(proxy);
                        return;
                    }
                }
                /* Für BODY_MODE_CLOSE warten wir bis Backend schließt */
            }
            break;

        case PROXY_STATE_STREAMING_WAIT_WRITE:
            /* Warte auf Client Write Event - wird in client_handler behandelt */
            break;

        case PROXY_STATE_ERROR:
#if _WEBSERVER_PROXY_DEBUG_ >= 3
            LOG(PROXY_LOG, NOTICE_LEVEL, 0, "%s", "Proxy error state, cleaning up");
#endif
            {
                socket_info* client = proxy->client_sock;

                /* 502 Error senden wenn noch keine Response gesendet wurde */
                if (proxy->response_header_sent == 0) {
                    LOG(PROXY_LOG, NOTICE_LEVEL, 0, "Sending 502 Bad Gateway for %s", proxy->request_url);
                    proxy_send_error_response(proxy);
                }

                client->reverse_proxy_error = 1;
                /* extern_handle_data_ptr auf NULL setzen BEVOR cleanup,
                 * um Rekursion bei WebserverConnectionManagerCloseRequest zu vermeiden */
                client->extern_handle_data_ptr = NULL;
                client->extern_handle = NULL;
                proxy_cleanup_connection(proxy);
                /* Client Socket schließen über ConnectionManager */
                WebserverConnectionManagerCloseRequest(client);
            }
            break;

        case PROXY_STATE_DONE:
#if _WEBSERVER_PROXY_DEBUG_ >= 2
            /* Timing: Fertig */
            clock_gettime(CLOCK_MONOTONIC, &proxy->time_done);
            {
                double t_connect = proxy_time_diff_ms(&proxy->time_start, &proxy->time_backend_connected);
                double t_send = proxy_time_diff_ms(&proxy->time_backend_connected, &proxy->time_request_sent);
                double t_wait = proxy_time_diff_ms(&proxy->time_request_sent, &proxy->time_first_response);
                double t_transfer = proxy_time_diff_ms(&proxy->time_first_response, &proxy->time_done);
                double t_total = proxy_time_diff_ms(&proxy->time_start, &proxy->time_done);

                LOG(PROXY_LOG, NOTICE_LEVEL, proxy->backend_fd,
                    "TIMING %s: connect=%.1fms send=%.1fms wait=%.1fms transfer=%.1fms TOTAL=%.1fms",
                    proxy->request_url, t_connect, t_send, t_wait, t_transfer, t_total);
            }
#endif
#if _WEBSERVER_PROXY_DEBUG_ >= 2
            LOG(PROXY_LOG, NOTICE_LEVEL, proxy->backend_fd, "%s", "Proxy done, cleaning up");
#endif
            {
                socket_info* client = proxy->client_sock;
                /* extern_handle_data_ptr auf NULL setzen BEVOR cleanup */
                client->extern_handle_data_ptr = NULL;
                client->extern_handle = NULL;
                proxy_cleanup_connection(proxy);
                /* Client Socket schließen über ConnectionManager */
                WebserverConnectionManagerCloseRequest(client);
            }
            break;
            
        default:
            LOG(PROXY_LOG, ERROR_LEVEL, 0, "Unknown state: %d", proxy->state);
            break;
    }
}

/*
 * Connect to backend UDS
 */
static int proxy_connect_backend(reverse_proxy_connection_t* proxy) {
    struct sockaddr_un addr;
    int sock_fd;
    int flags;

    sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        LOG(PROXY_LOG, ERROR_LEVEL, 0, "Failed to create UDS socket: %s", strerror(errno));
        return -1;
    }

    /* Non-blocking setzen */
    flags = fcntl(sock_fd, F_GETFL, 0);
    if (flags < 0 || fcntl(sock_fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        LOG(PROXY_LOG, ERROR_LEVEL, 0, "Failed to set non-blocking on UDS socket: %s", strerror(errno));
        close(sock_fd);
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, proxy->backend_path, sizeof(addr.sun_path) - 1);

    if (connect(sock_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        if (errno != EINPROGRESS) {
            LOG(PROXY_LOG, NOTICE_LEVEL, 0, "Failed to connect to backend %s: %s",
                      proxy->backend_path, strerror(errno));
            close(sock_fd);
            return -1;
        }
    }

    proxy->backend_fd = sock_fd;

    /* Backend socket_info erstellen */
    proxy->backend_sock = WebserverMallocSocketInfo();
    if (proxy->backend_sock == NULL) {
        close(sock_fd);
        proxy->backend_fd = -1;
        return -1;
    }

    proxy->backend_sock->socket = sock_fd;
    proxy->backend_sock->active = 1;
    proxy->backend_sock->client = 1;

    /* Socket Container hinzufügen für Event-Handling */
    addSocketContainer(proxy->backend_sock);

#if _WEBSERVER_PROXY_DEBUG_ >= 3
    LOG(PROXY_LOG, NOTICE_LEVEL, 0, "Connected to backend %s (fd=%d)", proxy->backend_path, sock_fd);
#endif

    return 0;
}

/*
 * Parse response header for status code and other info
 */
static int proxy_parse_response_header(reverse_proxy_connection_t* proxy) {
    proxy->response_status_code = proxy_get_status_code(proxy->response_buffer);
    proxy->response_is_websocket_upgrade = proxy_check_websocket_upgrade(
        proxy->response_buffer, proxy->response_header_end);

#if _WEBSERVER_PROXY_DEBUG_ >= 4
    LOG(PROXY_LOG, NOTICE_LEVEL, 0, "Response status: %d, websocket_upgrade: %d",
              proxy->response_status_code, proxy->response_is_websocket_upgrade);
#endif

    return 0;
}

/*
 * Find end of HTTP header (\r\n\r\n)
 */
static int proxy_find_header_end(const unsigned char* buffer, uint32_t len) {
    uint32_t i;

    if (len < 4) {
        return -1;
    }

    for (i = 0; i < len - 3; i++) {
        if (buffer[i] == '\r' && buffer[i+1] == '\n' &&
            buffer[i+2] == '\r' && buffer[i+3] == '\n') {
            return i + 4;
        }
    }

    return -1;
}

/*
 * Parse Content-Length header
 */
static int proxy_parse_content_length(const unsigned char* header, uint32_t header_len) {
    const unsigned char* p = header;
    const unsigned char* end = header + header_len;
    const char* cl_str = "Content-Length:";
    int cl_len = strlen(cl_str);

    while (p < end - cl_len) {
        if (strncasecmp((const char*)p, cl_str, cl_len) == 0) {
            p += cl_len;
            while (p < end && (*p == ' ' || *p == '\t')) {
                p++;
            }
            return atoi((const char*)p);
        }
        /* Zur nächsten Zeile */
        while (p < end && *p != '\n') {
            p++;
        }
        if (p < end) {
            p++;
        }
    }

    return -1;  /* Kein Content-Length Header gefunden */
}

/*
 * Check for Transfer-Encoding: chunked
 */
static int proxy_check_chunked_encoding(const unsigned char* header, uint32_t header_len) {
    const unsigned char* p = header;
    const unsigned char* end = header + header_len;
    const char* te_str = "Transfer-Encoding:";
    int te_len = strlen(te_str);

    while (p < end - te_len) {
        if (strncasecmp((const char*)p, te_str, te_len) == 0) {
            p += te_len;
            while (p < end && *p != '\r' && *p != '\n') {
                if (strncasecmp((const char*)p, "chunked", 7) == 0) {
                    return 1;
                }
                p++;
            }
        }
        /* Zur nächsten Zeile */
        while (p < end && *p != '\n') {
            p++;
        }
        if (p < end) {
            p++;
        }
    }

    return 0;
}

/*
 * Check for WebSocket upgrade response (101 Switching Protocols)
 */
static int proxy_check_websocket_upgrade(const unsigned char* header, uint32_t header_len) {
    const unsigned char* p;
    const unsigned char* end;
    int status = proxy_get_status_code(header);

    if (status != 101) {
        return 0;
    }

    /* Check for Upgrade: websocket header */
    p = header;
    end = header + header_len;

    while (p < end - 8) {
        if (strncasecmp((const char*)p, "Upgrade:", 8) == 0) {
            p += 8;
            while (p < end && *p != '\r' && *p != '\n') {
                if (strncasecmp((const char*)p, "websocket", 9) == 0) {
                    return 1;
                }
                p++;
            }
        }
        while (p < end && *p != '\n') {
            p++;
        }
        if (p < end) {
            p++;
        }
    }

    return 0;
}

/*
 * Get HTTP status code from response
 */
static int proxy_get_status_code(const unsigned char* header) {
    /* Format: HTTP/1.x SSS ... */
    const unsigned char* p = header;

    /* Skip "HTTP/1.x " */
    while (*p && *p != ' ') {
        p++;
    }
    if (*p == ' ') {
        p++;
    }

    return atoi((const char*)p);
}

/*
 * Extract Host header from HTTP request
 */
static int proxy_extract_host_header(const unsigned char* header, uint32_t header_len,
                                      char* host_buf, int host_buf_size) {
    const unsigned char* p = header;
    const unsigned char* end = header + header_len;
    const char* host_str = "Host:";
    int host_str_len = 5;
    int pos = 0;

    while (p < end - host_str_len) {
        if (strncasecmp((const char*)p, host_str, host_str_len) == 0) {
            p += host_str_len;
            /* Skip whitespace */
            while (p < end && (*p == ' ' || *p == '\t')) {
                p++;
            }
            /* Copy until end of line */
            while (p < end && *p != '\r' && *p != '\n' && pos < host_buf_size - 1) {
                host_buf[pos++] = *p++;
            }
            host_buf[pos] = '\0';
            return pos;
        }
        /* Move to next line */
        while (p < end && *p != '\n') {
            p++;
        }
        if (p < end) {
            p++;
        }
    }

    host_buf[0] = '\0';
    return 0;
}

/*
 * Inject X-Forwarded-* headers into request before sending to backend
 * This modifies the request_buffer to add headers before the body
 */
static int proxy_inject_forwarded_headers(reverse_proxy_connection_t* proxy) {
    char host_buf[256];
    char forwarded_headers[512];
    int forwarded_len;
    unsigned char* new_buffer;
    uint32_t new_size;
    const char* proto = "http";

    /* Extract Host header from original request */
    proxy_extract_host_header(proxy->request_buffer, proxy->request_header_end,
                               host_buf, sizeof(host_buf));

    /* Determine protocol */
#ifdef WEBSERVER_USE_SSL
    if (proxy->client_sock->use_ssl) {
        proto = "https";
    }
#endif

    /* Build X-Forwarded-* headers */
    forwarded_len = snprintf(forwarded_headers, sizeof(forwarded_headers),
        "X-Forwarded-For: %s\r\n"
        "X-Forwarded-Host: %s\r\n"
        "X-Forwarded-Proto: %s\r\n",
        proxy->client_sock->client_ip_str,
        host_buf[0] ? host_buf : "unknown",
        proto);

    if (forwarded_len <= 0 || forwarded_len >= (int)sizeof(forwarded_headers)) {
        LOG(PROXY_LOG, ERROR_LEVEL, 0, "%s", "Failed to build forwarded headers");
        return -1;
    }

#if _WEBSERVER_PROXY_DEBUG_ >= 4
    LOG(PROXY_LOG, NOTICE_LEVEL, 0, "Injecting X-Forwarded-For=%s, Proto=%s",
              proxy->client_sock->client_ip_str, proto);
#endif

    /* Calculate new buffer size */
    new_size = proxy->request_buffer_pos + forwarded_len;
    if (new_size > proxy->request_buffer_size) {
        /* Allocate larger buffer */
        new_buffer = WebserverMalloc(new_size + 1024);
        if (new_buffer == NULL) {
            LOG(PROXY_LOG, ERROR_LEVEL, 0, "%s", "Failed to allocate new request buffer");
            return -1;
        }
    } else {
        /* Reuse existing buffer with memmove */
        new_buffer = proxy->request_buffer;
    }

    /* Insert headers before the \r\n\r\n (header_end - 2 is where we insert) */
    /* Format: [original_header_without_final_crlf][new_headers][\r\n][body] */

    if (new_buffer != proxy->request_buffer) {
        /* Copy header (without final \r\n) */
        memcpy(new_buffer, proxy->request_buffer, proxy->request_header_end - 2);
    }

    /* Move body data to make room for new headers */
    uint32_t body_start = proxy->request_header_end;
    uint32_t body_len = proxy->request_buffer_pos - body_start;

    if (new_buffer == proxy->request_buffer) {
        /* Move body data to the right */
        memmove(&new_buffer[proxy->request_header_end - 2 + forwarded_len + 2],
                &proxy->request_buffer[body_start],
                body_len);
    } else {
        /* Copy body to new buffer */
        memcpy(&new_buffer[proxy->request_header_end - 2 + forwarded_len + 2],
               &proxy->request_buffer[body_start],
               body_len);
    }

    /* Insert new headers */
    memcpy(&new_buffer[proxy->request_header_end - 2], forwarded_headers, forwarded_len);

    /* Add final \r\n after new headers */
    new_buffer[proxy->request_header_end - 2 + forwarded_len] = '\r';
    new_buffer[proxy->request_header_end - 2 + forwarded_len + 1] = '\n';

    /* Update proxy state */
    if (new_buffer != proxy->request_buffer) {
        WebserverFree(proxy->request_buffer);
        proxy->request_buffer = new_buffer;
        proxy->request_buffer_size = new_size + 1024;
    }

    proxy->request_buffer_pos += forwarded_len;
    proxy->request_header_end += forwarded_len;

    return 0;
}

/*
 * Inject Connection: close header into response to force connection close after response
 */
static int proxy_inject_connection_close(reverse_proxy_connection_t* proxy) {
    const char* conn_header = "Connection: close\r\n";
    int conn_len = strlen(conn_header);
    unsigned char* new_buffer;
    uint32_t new_size;

    /* Calculate new buffer size */
    new_size = proxy->response_buffer_pos + conn_len;
    if (new_size > proxy->response_buffer_size) {
        /* Allocate larger buffer */
        new_buffer = WebserverMalloc(new_size + 1024);
        if (new_buffer == NULL) {
            LOG(PROXY_LOG, ERROR_LEVEL, 0, "%s", "Failed to allocate new response buffer");
            return -1;
        }
        proxy->response_buffer_size = new_size + 1024;
    } else {
        new_buffer = proxy->response_buffer;
    }

    /* Insert header before the \r\n\r\n (header_end - 2 is where we insert) */
    uint32_t body_start = proxy->response_header_end;
    uint32_t body_len = proxy->response_buffer_pos - body_start;

    if (new_buffer != proxy->response_buffer) {
        /* Copy header (without final \r\n) */
        memcpy(new_buffer, proxy->response_buffer, proxy->response_header_end - 2);
    }

    /* Move body data to make room for new header */
    if (new_buffer == proxy->response_buffer) {
        memmove(&new_buffer[proxy->response_header_end - 2 + conn_len + 2],
                &proxy->response_buffer[body_start],
                body_len);
    } else {
        memcpy(&new_buffer[proxy->response_header_end - 2 + conn_len + 2],
               &proxy->response_buffer[body_start],
               body_len);
    }

    /* Insert Connection: close header */
    memcpy(&new_buffer[proxy->response_header_end - 2], conn_header, conn_len);

    /* Add final \r\n after new header */
    new_buffer[proxy->response_header_end - 2 + conn_len] = '\r';
    new_buffer[proxy->response_header_end - 2 + conn_len + 1] = '\n';

    /* Update proxy state */
    if (new_buffer != proxy->response_buffer) {
        WebserverFree(proxy->response_buffer);
        proxy->response_buffer = new_buffer;
    }

    proxy->response_buffer_pos += conn_len;
    proxy->response_header_end += conn_len;

#if _WEBSERVER_PROXY_DEBUG_ >= 5
    LOG(PROXY_LOG, NOTICE_LEVEL, 0, "%s", "Injected Connection: close header");
#endif
    return 0;
}

/*
 * Cleanup proxy connection
 */
static void proxy_cleanup_connection(reverse_proxy_connection_t* proxy) {
    if (proxy == NULL) {
        return;
    }

#if _WEBSERVER_PROXY_DEBUG_ >= 4
    LOG(PROXY_LOG, NOTICE_LEVEL, proxy->backend_fd, "%s", "Cleaning up proxy connection");
#endif
    
    /* Backend Socket schließen */
    if (proxy->backend_sock != NULL) {
        delEventSocketAll(proxy->backend_sock);
        deleteSocket(proxy->backend_sock);
        if (proxy->backend_fd >= 0) {
            close(proxy->backend_fd);
            proxy->backend_fd = -1;
        }
        WebserverFreeSocketInfo(proxy->backend_sock);
        proxy->backend_sock = NULL;
    } else if (proxy->backend_fd >= 0) {
        close(proxy->backend_fd);
        proxy->backend_fd = -1;
    }
    
    /* Client Socket wieder normalisieren (wird von WebserverConnectionManagerCloseRequest geschlossen) */
    if (proxy->client_sock != NULL) {
        proxy->client_sock->extern_handle = NULL;
        proxy->client_sock->extern_handle_data_ptr = NULL;
        /* Client Socket wird vom Aufrufer geschlossen */
    }
    
    /* Buffer freigeben */
    if (proxy->request_buffer != NULL) {
        WebserverFree(proxy->request_buffer);
    }
    if (proxy->response_buffer != NULL) {
        WebserverFree(proxy->response_buffer);
    }
    if (proxy->backend_path != NULL) {
        free(proxy->backend_path);
    }
    
    WebserverFree(proxy);
}

/*
 * Error Response generieren
 */
void reverse_proxy_gen_error_msg(http_request* p) {
    if (p->socket->header->isHttp1_1) {
        printHeaderChunk(p->socket, "HTTP/1.1 ");
    } else {
        printHeaderChunk(p->socket, "HTTP/1.0 ");
    }
    
    printHeaderChunk(p->socket, "502 Bad Gateway\r\n");
    printHeaderChunk(p->socket, "Server: libCWebUI\r\n");
    printHeaderChunk(p->socket, "Content-Type: text/html\r\n");
    printHeaderChunk(p->socket, "Content-Length: 50\r\n");
    printHeaderChunk(p->socket, "Connection: close\r\n");
    printHeaderChunk(p->socket, "\r\n");
    printHeaderChunk(p->socket, "<html><body><h1>502 Bad Gateway</h1></body></html>");
}

/*
 * Cleanup bei Socket Close
 */
void reverse_proxy_cleanup(socket_info* s) {
    if (s == NULL || s->extern_handle_data_ptr == NULL) {
        return;
    }
    
    reverse_proxy_connection_t* proxy = (reverse_proxy_connection_t*)s->extern_handle_data_ptr;
    proxy_cleanup_connection(proxy);
    s->extern_handle_data_ptr = NULL;
}
