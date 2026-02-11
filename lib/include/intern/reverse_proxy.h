/*

SPDX-License-Identifier: MPL-2.0

 libCWebUI
 Copyright (C) 2007 - 2019  Ramin Seyed-Moussavi

 Projekt URL : https://github.com/lordrasmus/libcwebui

 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at https://mozilla.org/MPL/2.0/. 

*/

#ifndef _WEBSERVER_REVERSE_PROXY_H_
#define _WEBSERVER_REVERSE_PROXY_H_

#include <time.h>
#include "intern/dataTypes.h"

/* Proxy States */
typedef enum {
    PROXY_STATE_IDLE = 0,
    
    /* HTTP Request Handling */
    PROXY_STATE_RECV_CLIENT_HEADER,      /* Empfange HTTP Header vom Client */
    PROXY_STATE_RECV_CLIENT_BODY,        /* Empfange HTTP Body vom Client */
    PROXY_STATE_CONNECT_BACKEND,         /* Verbinde zum Backend */
    PROXY_STATE_SEND_TO_BACKEND,         /* Sende Request an Backend */
    PROXY_STATE_RECV_BACKEND_HEADER,     /* Empfange Response Header vom Backend */
    PROXY_STATE_STREAMING,               /* Streaming: Backend -> Client (sofort weiterleiten) */
    PROXY_STATE_STREAMING_WAIT_WRITE,    /* Warte auf Client Write-Event */

    /* WebSocket Tunnel */
    PROXY_STATE_WEBSOCKET_TUNNEL,        /* Bidirektionales Forwarding */
    
    PROXY_STATE_ERROR,
    PROXY_STATE_DONE
} proxy_state_t;

/* Body Transfer Modes */
typedef enum {
    BODY_MODE_NONE = 0,          /* Kein Body */
    BODY_MODE_CONTENT_LENGTH,    /* Content-Length Header */
    BODY_MODE_CHUNKED,           /* Transfer-Encoding: chunked */
    BODY_MODE_CLOSE              /* Bis Connection Close lesen */
} body_transfer_mode_t;

/* Proxy Connection Info */
typedef struct reverse_proxy_connection {
    /* State Machine */
    proxy_state_t state;
    
    /* Client Socket (der originale Webserver Socket) */
    socket_info* client_sock;
    
    /* Backend Socket (UDS Verbindung) */
    socket_info* backend_sock;
    int backend_fd;
    
    /* Backend Info */
    char* backend_path;          /* UDS Path */
    
    /* Request Buffer (vom Client empfangen) */
    unsigned char* request_buffer;
    uint32_t request_buffer_size;
    uint32_t request_buffer_pos;
    uint32_t request_header_end;  /* Position wo Header endet (\r\n\r\n) */
    
    /* Request Body Info */
    body_transfer_mode_t request_body_mode;
    uint64_t request_content_length;
    uint64_t request_body_received;
    
    /* Response Buffer (vom Backend empfangen) */
    unsigned char* response_buffer;
    uint32_t response_buffer_size;
    uint32_t response_buffer_pos;
    uint32_t response_header_end;
    int response_header_complete;
    
    /* Response Info */
    int response_status_code;
    body_transfer_mode_t response_body_mode;
    uint64_t response_content_length;
    uint64_t response_body_received;
    int response_is_websocket_upgrade;
    
    /* Send Tracking */
    uint32_t send_pos;           /* Aktuelle Position beim Senden */

    /* Streaming State */
    int response_header_sent;    /* Header wurde an Client gesendet */
    uint64_t response_body_sent; /* Bytes des Body an Client gesendet */

    /* Chunked Encoding State */
    int chunk_state;             /* 0=size, 1=data, 2=trailer */
    uint64_t current_chunk_size;
    uint64_t current_chunk_received;

#if _WEBSERVER_PROXY_DEBUG_ >= 2
    /* Timing Info */
    struct timespec time_start;           /* Proxy Start */
    struct timespec time_backend_connected; /* Backend verbunden */
    struct timespec time_request_sent;    /* Request vollständig gesendet */
    struct timespec time_first_response;  /* Erste Response-Bytes empfangen */
    struct timespec time_done;            /* Proxy fertig */
#endif
    char request_url[512];                /* URL für Logging */

} reverse_proxy_connection_t;

/* Registration Structure */
typedef enum {
    PROXY_TYPE_UDS = 0
} proxy_type_t;

typedef struct reverse_proxy_registration {
    char* method;                /* GET, POST, etc. oder "*" für alle */
    char* url_prefix;            /* URL Prefix zum Matchen */
    int url_prefix_len;
    
    proxy_type_t type;
    char* backend_path;          /* UDS Socket Path */
    
    int forward_websocket;       /* WebSocket Upgrade weiterleiten? */
    
    struct reverse_proxy_registration* next;
} reverse_proxy_registration_t;


/*
 * API Functions
 */

/* Registriere einen Reverse-Proxy Endpunkt (interne Funktion)
 * method: HTTP Methode ("GET", "POST", "*" für alle)
 * url_prefix: URL Prefix zum Matchen (z.B. "/api/")
 * backend_path: Pfad zum Unix Domain Socket
 */
void reverse_proxy_register_uds_internal(const char* method, const char* url_prefix, const char* backend_path);

/* Prüfe ob URL für Reverse-Proxy registriert ist
 * Gibt 0 zurück wenn Proxy-Handling übernommen wird, 1 wenn nicht
 */
int checkReverseProxy(socket_info* s, char* buffer, int length);

/* Generiere Fehler-Response */
void reverse_proxy_gen_error_msg(http_request* p);

/* Cleanup bei Verbindungsende */
void reverse_proxy_cleanup(socket_info* s);


/*
 * Internal Functions (für system_sockets.c)
 */

/* Event Handler für Client Socket im Proxy-Modus */
void reverse_proxy_client_handler(int fd, void* ptr);

/* Event Handler für Backend Socket */
void reverse_proxy_backend_handler(int fd, void* ptr);

#endif /* _WEBSERVER_REVERSE_PROXY_H_ */
