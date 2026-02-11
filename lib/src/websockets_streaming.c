/*

SPDX-License-Identifier: MPL-2.0

 libCWebUI
 Copyright (C) 2007 - 2024  Ramin Seyed-Moussavi

 Projekt URL : https://github.com/lordrasmus/libcwebui


 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at https://mozilla.org/MPL/2.0/.

*/

#include "webserver.h"

#ifdef WEBSERVER_USE_WEBSOCKETS

#ifndef WEBSOCKET_STREAM_CHUNK_SIZE
    #define WEBSOCKET_STREAM_CHUNK_SIZE (64 * 1024)
#endif

#define WEBSOCKET_STREAM_MAX_URL_LENGTH 1024

/* Handler registry - simple linked list
 * Entries persist for the lifetime of the application.
 * Per-stream state is in socket_info, not here.
 */
static list_t stream_handler_list;
static int streaming_initialized = 0;

/*
 * Initialize streaming subsystem
 */
void initWebsocketStreaming(void) {
    if (streaming_initialized) return;
    ws_list_init(&stream_handler_list);
    streaming_initialized = 1;
}

/*
 * Cleanup streaming subsystem (called at shutdown)
 */
void cleanupWebsocketStreaming(void) {
    if (!streaming_initialized) return;

    websocket_stream_handler_entry* entry;
    ws_list_iterator_start(&stream_handler_list);
    while ((entry = (websocket_stream_handler_entry*)ws_list_iterator_next(&stream_handler_list))) {
        if (entry->url) WebserverFree(entry->url);
        WebserverFree(entry);
    }
    ws_list_iterator_stop(&stream_handler_list);
    ws_list_clear(&stream_handler_list);
    streaming_initialized = 0;
}

/*
 * Register a streaming handler for a URL
 * Public API
 */
void RegisterWebsocketStreamHandler(
    const char* url,
    ws_stream_start_handler on_start,
    ws_stream_chunk_handler on_chunk,
    ws_stream_end_handler on_end
) {
    if (!streaming_initialized) {
        initWebsocketStreaming();
    }

    /* Validate parameters with specific error messages */
    if (!url) {
        LOG(WEBSOCKET_LOG, ERROR_LEVEL, 0, "%s", "RegisterWebsocketStreamHandler: url is NULL");
        return;
    }
    if (!on_start) {
        LOG(WEBSOCKET_LOG, ERROR_LEVEL, 0, "RegisterWebsocketStreamHandler: on_start callback is NULL for url '%s'", url);
        return;
    }
    if (!on_chunk) {
        LOG(WEBSOCKET_LOG, ERROR_LEVEL, 0, "RegisterWebsocketStreamHandler: on_chunk callback is NULL for url '%s'", url);
        return;
    }
    if (!on_end) {
        LOG(WEBSOCKET_LOG, ERROR_LEVEL, 0, "RegisterWebsocketStreamHandler: on_end callback is NULL for url '%s'", url);
        return;
    }

    size_t url_len = strlen(url);
    if (url_len == 0) {
        LOG(WEBSOCKET_LOG, ERROR_LEVEL, 0, "%s", "RegisterWebsocketStreamHandler: url is empty");
        return;
    }
    if (url_len >= WEBSOCKET_STREAM_MAX_URL_LENGTH) {
        LOG(WEBSOCKET_LOG, ERROR_LEVEL, 0, "RegisterWebsocketStreamHandler: url too long (%zu >= %d)", url_len, WEBSOCKET_STREAM_MAX_URL_LENGTH);
        return;
    }

    websocket_stream_handler_entry* entry = WebserverMalloc(sizeof(websocket_stream_handler_entry));
    if (!entry) {
        LOG(WEBSOCKET_LOG, ERROR_LEVEL, 0, "RegisterWebsocketStreamHandler: failed to allocate handler entry for url '%s'", url);
        return;
    }

    entry->url = WebserverMalloc(url_len + 1);
    if (!entry->url) {
        LOG(WEBSOCKET_LOG, ERROR_LEVEL, 0, "RegisterWebsocketStreamHandler: failed to allocate url string for '%s'", url);
        WebserverFree(entry);
        return;
    }
    memcpy(entry->url, url, url_len);
    entry->url[url_len] = '\0';

    entry->on_start = on_start;
    entry->on_chunk = on_chunk;
    entry->on_end = on_end;

    ws_list_append(&stream_handler_list, entry, 0);

#ifdef _WEBSERVER_WEBSOCKET_STREAMING_DEBUG_
    LOG(WEBSOCKET_LOG, INFO_LEVEL, 0, "Registered streaming handler for URL: %s", url);
#endif
}

/*
 * Find streaming handler for URL
 * Internal
 */
websocket_stream_handler_entry* findWebsocketStreamHandler(const char* url) {
    if (!streaming_initialized || !url) return NULL;

    websocket_stream_handler_entry* entry;
    ws_list_iterator_start(&stream_handler_list);
    while ((entry = (websocket_stream_handler_entry*)ws_list_iterator_next(&stream_handler_list))) {
        if (strcmp(entry->url, url) == 0) {
            ws_list_iterator_stop(&stream_handler_list);
            return entry;
        }
    }
    ws_list_iterator_stop(&stream_handler_list);
    return NULL;
}

/*
 * Free stream context (per-stream state, not the handler entry)
 * Internal
 */
void freeWebsocketStreamContext(websocket_stream_context* ctx) {
    if (!ctx) return;
    if (ctx->guid) WebserverFree(ctx->guid);
    if (ctx->url) WebserverFree(ctx->url);
    WebserverFree(ctx);
}

/*
 * Cleanup streaming state from socket (helper)
 */
static void cleanupSocketStreamState(socket_info *sock) {
    sock->websocket_streaming_active = 0;
    sock->websocket_stream_fragmented = 0;
    sock->websocket_stream_ctx = NULL;
    sock->websocket_stream_handler = NULL;  /* Just a pointer, handler stays in registry */
    sock->websocket_stream_remaining = 0;
    sock->websocket_stream_mask_offset = 0;
}

/*
 * Public helper functions
 */
void WebsocketStreamSetUserData(websocket_stream_context* ctx, void* data) {
    if (ctx) ctx->user_data = data;
}

void* WebsocketStreamGetUserData(websocket_stream_context* ctx) {
    return ctx ? ctx->user_data : NULL;
}

void WebsocketStreamAbort(websocket_stream_context* ctx) {
    if (ctx) ctx->aborted = 1;
}

/*
 * Start a new streaming session
 * Called from recFrameV8 when header is complete and stream handler exists
 * Internal
 *
 * For non-fragmented streams (fragmented=0):
 *   - total_length is known from the single frame header
 *   - remaining tracks bytes until frame is complete
 *   - continueWebsocketStream reads raw bytes from socket
 *
 * For fragmented streams (fragmented=1):
 *   - total_length is unknown (set to 0), updated as fragments arrive
 *   - remaining is NOT used (each fragment has its own length)
 *   - Chunks are passed from recFrameV8 as each fragment arrives
 */
int startWebsocketStream(socket_info *sock, uint64_t frame_length,
                         unsigned char opcode, unsigned char* mask,
                         websocket_stream_handler_entry* handler,
                         char fragmented) {

    if (!sock || !handler) {
        LOG(WEBSOCKET_LOG, ERROR_LEVEL, 0, "startWebsocketStream: invalid parameters (sock=%p, handler=%p)",
            (void*)sock, (void*)handler);
        return -1;
    }

    /* Create context */
    websocket_stream_context* ctx = WebserverMalloc(sizeof(websocket_stream_context));
    if (!ctx) {
        LOG(WEBSOCKET_LOG, ERROR_LEVEL, sock->socket, "%s", "startWebsocketStream: failed to allocate stream context");
        return -1;
    }

    memset(ctx, 0, sizeof(websocket_stream_context));

    /* Copy GUID */
    size_t guid_len = strlen(sock->websocket_guid);
    ctx->guid = WebserverMalloc(guid_len + 1);
    if (!ctx->guid) {
        LOG(WEBSOCKET_LOG, ERROR_LEVEL, sock->socket, "%s", "startWebsocketStream: failed to allocate guid");
        WebserverFree(ctx);
        return -1;
    }
    memcpy(ctx->guid, sock->websocket_guid, guid_len);
    ctx->guid[guid_len] = '\0';

    /* Copy URL */
    size_t url_len = strlen(sock->header->url);
    ctx->url = WebserverMalloc(url_len + 1);
    if (!ctx->url) {
        LOG(WEBSOCKET_LOG, ERROR_LEVEL, sock->socket, "%s", "startWebsocketStream: failed to allocate url");
        WebserverFree(ctx->guid);
        WebserverFree(ctx);
        return -1;
    }
    memcpy(ctx->url, sock->header->url, url_len);
    ctx->url[url_len] = '\0';

    /* For fragmented streams, total_length is unknown */
    ctx->total_length = fragmented ? 0 : frame_length;
    ctx->received_length = 0;
    ctx->is_binary = (opcode == 0x02);  /* WSF_BINARY */
    ctx->aborted = 0;
    ctx->user_data = NULL;

    /* Save state in socket_info */
    sock->websocket_streaming_active = 1;
    sock->websocket_stream_fragmented = fragmented;
    sock->websocket_stream_remaining = fragmented ? 0 : frame_length;
    sock->websocket_stream_ctx = ctx;
    sock->websocket_stream_handler = handler;

    /* Save mask for continuous unmasking (only used for non-fragmented) */
    if (mask && !fragmented) {
        memcpy(sock->websocket_stream_mask, mask, 4);
    } else {
        memset(sock->websocket_stream_mask, 0, 4);
    }
    sock->websocket_stream_mask_offset = 0;

    /* Notify handler */
    handler->on_start(ctx);

#ifdef _WEBSERVER_WEBSOCKET_STREAMING_DEBUG_
    if (fragmented) {
        LOG(WEBSOCKET_LOG, DEBUG_LEVEL, sock->socket, "Started fragmented streaming for url '%s'", ctx->url);
    } else {
        LOG(WEBSOCKET_LOG, DEBUG_LEVEL, sock->socket, "Started streaming for url '%s', total length: %llu",
            ctx->url, (unsigned long long)frame_length);
    }
#endif

    return 0;
}

/*
 * Continue receiving stream data
 * Called from epoll event handler when streaming_active is set
 * Internal
 */
int continueWebsocketStream(socket_info *sock) {
    unsigned char chunk_buffer[WEBSOCKET_STREAM_CHUNK_SIZE];
    websocket_stream_context* ctx;
    websocket_stream_handler_entry* handler;

    if (!sock) {
        LOG(WEBSOCKET_LOG, ERROR_LEVEL, 0, "%s", "continueWebsocketStream: sock is NULL");
        return -1;
    }

    ctx = sock->websocket_stream_ctx;
    handler = sock->websocket_stream_handler;

    if (!ctx || !handler) {
        LOG(WEBSOCKET_LOG, ERROR_LEVEL, sock->socket, "continueWebsocketStream: invalid state (ctx=%p, handler=%p)",
            (void*)ctx, (void*)handler);
        cleanupSocketStreamState(sock);
        return -1;
    }

    /* Calculate how much to read */
    uint32_t to_read = sock->websocket_stream_remaining;
    if (to_read > WEBSOCKET_STREAM_CHUNK_SIZE) {
        to_read = WEBSOCKET_STREAM_CHUNK_SIZE;
    }

    /* Read what's available (non-blocking) */
    ssize_t ret = WebserverRecv(sock, chunk_buffer, to_read, 0);

    if (ret < 0) {
        /* EAGAIN/EWOULDBLOCK - no data available, return to epoll */
        #ifdef _WIN32
        if (WSAGetLastError() == WSAEWOULDBLOCK) return 0;
        #else
        if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
        #endif

        /* Real error */
        LOG(WEBSOCKET_LOG, ERROR_LEVEL, sock->socket, "continueWebsocketStream: recv error: %s", strerror(errno));
        handler->on_end(ctx, 0);
        freeWebsocketStreamContext(ctx);
        cleanupSocketStreamState(sock);
        return -1;
    }

    if (ret == 0) {
        /* Connection closed */
        LOG(WEBSOCKET_LOG, WARNING_LEVEL, sock->socket, "%s", "continueWebsocketStream: connection closed during stream");
        handler->on_end(ctx, 0);
        freeWebsocketStreamContext(ctx);
        cleanupSocketStreamState(sock);
        return -1;
    }

    /* Unmask data */
    for (ssize_t i = 0; i < ret; i++) {
        chunk_buffer[i] ^= sock->websocket_stream_mask[
            (sock->websocket_stream_mask_offset + i) % 4
        ];
    }
    sock->websocket_stream_mask_offset =
        (sock->websocket_stream_mask_offset + ret) % 4;

    /* Pass to handler */
    handler->on_chunk(ctx, chunk_buffer, (uint32_t)ret);

    ctx->received_length += ret;
    sock->websocket_stream_remaining -= ret;

    /* Stream complete? */
    if (sock->websocket_stream_remaining == 0 || ctx->aborted) {
        int was_aborted = ctx->aborted;
        uint64_t remaining_at_abort = sock->websocket_stream_remaining;

#ifdef _WEBSERVER_WEBSOCKET_STREAMING_DEBUG_
        if (was_aborted) {
            LOG(WEBSOCKET_LOG, INFO_LEVEL, sock->socket, "Stream aborted by handler for url '%s'", ctx->url);
        } else {
            LOG(WEBSOCKET_LOG, DEBUG_LEVEL, sock->socket, "Stream complete for url '%s', received %llu bytes",
                ctx->url, (unsigned long long)ctx->received_length);
        }
#endif
        handler->on_end(ctx, was_aborted ? 0 : 1);
        freeWebsocketStreamContext(ctx);
        cleanupSocketStreamState(sock);

        /* If aborted with remaining data, we must close the socket.
         * Otherwise the remaining payload bytes would be interpreted as new frames. */
        if (was_aborted && remaining_at_abort > 0) {
            LOG(WEBSOCKET_LOG, WARNING_LEVEL, sock->socket,
                "Closing socket due to stream abort with %llu bytes remaining",
                (unsigned long long)remaining_at_abort);
            sock->closeSocket = 1;
            return -1;
        }
    }

    return 0;
}

#endif /* WEBSERVER_USE_WEBSOCKETS */
