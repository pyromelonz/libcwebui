/*

SPDX-License-Identifier: MPL-2.0

 libCWebUI
 Copyright (C) 2007 - 2019  Ramin Seyed-Moussavi

 Projekt URL : https://github.com/lordrasmus/libcwebui

 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at https://mozilla.org/MPL/2.0/. 

*/



#include "webserver.h"

#ifdef WEBSERVER_USE_WEBSOCKETS


#ifdef DMALLOC
#include <dmalloc/dmalloc.h>
#endif

#ifndef WEBSOCKET_MAX_INBUFFER_SIZE
	#define WEBSOCKET_MAX_INBUFFER_SIZE 65000
	#warning #define WEBSOCKET_MAX_INBUFFER_SIZE x missing, assuming 65000 byte
#endif

#ifdef WEBSOCKET_INBUFFER_SIZE
	#warning WEBSOCKET_INBUFFER_SIZE depricated
#endif

typedef enum {
	WSF_CONTINUE = 0x0,
	WSF_TEXT = 0x1,
	WSF_BINARY = 0x2,
	WSF_RESERVED1 = 0x3,
	WSF_RESERVED2 = 0x4,
	WSF_RESERVED3 = 0x5,
	WSF_RESERVED4 = 0x6,
	WSF_RESERVED5 = 0x7,
	WSF_CLOSE = 0x8,
	WSF_PING = 0x9,
	WSF_PONG = 0xA,
	WSF_RESERVED6 = 0xB,
	WSF_RESERVED7 = 0xC,
	WSF_RESERVED8 = 0xD,
	WSF_RESERVED9 = 0xE,
	WSF_RESERVED10 = 0xF
} Opcodes;

typedef struct {
	char fin;
	/*char rsv1:1;
	 char rsv2:1;
	 char rsv3:1;*/
	Opcodes opcode;
	char mask_bit;
	unsigned char playload_length;
	WEBSOCK_LEN_T real_length;
	unsigned char *mask; 
} websocket_frame;


int sendWebsocketFrame(const Opcodes op_code, const char* guid,const  char* in, const WEBSOCK_LEN_T length);

int sendWebsocketTextFrame(const char* guid,const  char* in, const WEBSOCK_LEN_T length) {
	char* msg;
	if ( 0 != is_utf8( (unsigned char*) in, length, &msg) ){
		printf("sendWebsocketTextFrame: invalid UTF-8 sting : %s\n",in);
		return -1;
	}
	return sendWebsocketFrame(WSF_TEXT, guid, in, length );
}

int sendWebsocketBinaryFrame(const char* guid,const unsigned  char* in, const WEBSOCK_LEN_T length) {
	return sendWebsocketFrame(WSF_BINARY, guid,(const  char* ) in, length );
}

int sendWebsocketFrame(const Opcodes op_code, const char* guid,const  char* in, const WEBSOCK_LEN_T length) {
	/*
		Daten im Text Frame nicht 0 terminieren
	*/
	websocket_queue_msg* msg;
	if (length < 126) {
		msg = create_websocket_output_queue_msg(WEBSOCKET_SIGNAL_MSG, guid, length + 2);
		msg->msg[0] = (unsigned char)(0x80 + op_code);
		msg->msg[1] = (unsigned char)(length);
		memcpy(&msg->msg[2], in, length);
		insert_websocket_output_queue(msg);
		return 0;
	} else {
		if ( ( length >= 126) && ( length < 65536) ){
			msg = create_websocket_output_queue_msg(WEBSOCKET_SIGNAL_MSG, guid, length + 4);
			msg->msg[0] = (unsigned char)(0x80 + op_code);
			msg->msg[1] = 126;
			msg->msg[2] = (unsigned char)(((unsigned int)length) >> 8);
			msg->msg[3] = (unsigned char)(((unsigned int)length) >> 0);
			memcpy(&msg->msg[4], in, length);
			insert_websocket_output_queue(msg);
			return 0;
		}
		msg = create_websocket_output_queue_msg( WEBSOCKET_SIGNAL_MSG, guid, length + 10);
		msg->msg[0] = (unsigned char)(0x80 + op_code);
		msg->msg[1] = 127;
		#ifdef SHORT_WS_FRAMES_T
			msg->msg[2] = 0;
			msg->msg[3] = 0;
			msg->msg[4] = 0;
			msg->msg[5] = 0;
		#else
			msg->msg[2] = ((WEBSOCK_LEN_T)length) >> 56;
			msg->msg[3] = ((WEBSOCK_LEN_T)length) >> 48;
			msg->msg[4] = ((WEBSOCK_LEN_T)length) >> 40;
			msg->msg[5] = ((WEBSOCK_LEN_T)length) >> 32;
		#endif
		msg->msg[6] = (unsigned char)((length) >> 24);
		msg->msg[7] = (unsigned char)((length) >> 16);
		msg->msg[8] = (unsigned char)((length) >> 8);
		msg->msg[9] = (unsigned char)((length) >> 0);
		memcpy(&msg->msg[10], in, length);
		insert_websocket_output_queue(msg);
		return 0;
	}
	return 0;
}

static void insert_websocket_output_chunk(socket_info *sock, const unsigned char* in, const WEBSOCK_LEN_T length){
	int ret;
	ret = ws_list_empty(&sock->websocket_chunk_list);

	sendWebsocketChunk(sock, in, length);

	PlatformUnlockMutex(&sock->socket_mutex);

	/* Write Event nur hinzufuegen wenn noch keine frames in der liste waren */
	if (ret == 1) {
		delEventSocketAll(sock);
		addEventSocketReadWritePersist(sock);
	}
}

/*
 Schreibt Daten aus der output queue in die Socket chunk liste
*/
int sendWebsocketFrameReal(const char* guid, const unsigned char* in, const WEBSOCK_LEN_T length) {

	/* getSocketByGUID locked den socket mutex */
	socket_info *sock = getSocketByGUID(guid);
	if (sock == 0){
		return -1;
	}

	if (sock->closeSocket == 1) {
		PlatformUnlockMutex(&sock->socket_mutex);
		return -1;
	}

	if (sock->header->SecWebSocketVersion > 6) {
		insert_websocket_output_chunk(sock,in,length);
	}else{
		printf("Error : Websocket Version %d < 6 \n",sock->header->SecWebSocketVersion);
		PlatformUnlockMutex(&sock->socket_mutex);
	}

	return 0;

}

static void sendCloseFrameWithStatus(const char* guid, uint16_t status_code) {
	unsigned char buffer[2];
	buffer[0] = (status_code >> 8) & 0xFF;
	buffer[1] = status_code & 0xFF;
	sendWebsocketFrame(WSF_CLOSE, guid, (const char*) buffer, 2);
}

void sendCloseFrame2(const char* guid) {
	/* status 1000 - normal close */
	sendCloseFrameWithStatus(guid, 1000);
}

void sendCloseFrame(socket_info *sock) {
	sendCloseFrame2(sock->websocket_guid);}


static int recFrameV8(socket_info *sock) {
	int last_frame_start;
	unsigned int offset = 0;
	unsigned int offset2 = 0;
	unsigned int i;
	unsigned int ui = 0;
	int ret;
	unsigned int diff = 0;
	unsigned int extra_bytes;
	int max_read;
	websocket_queue_msg* msg;
	websocket_frame wsf;
	unsigned char *tmp;

	/* Check if we're continuing an active stream */
	if (sock->websocket_streaming_active) {
		return continueWebsocketStream(sock);
	}

	memset( &wsf, 0 , sizeof( websocket_frame ) );

	while (1) {
		max_read = WebserverMallocedSize(sock->websocket_buffer) - sock->websocket_buffer_offset;
		ret = WebserverRecv(sock, &sock->websocket_buffer[sock->websocket_buffer_offset], max_read, 0);
		if (ret <= 0){
			return ret;
		}

		sock->websocket_buffer_offset += ret;

		while (1) {

			extra_bytes = 0;
			last_frame_start = offset;
			if ( sock->websocket_buffer_offset > offset ){
				diff = sock->websocket_buffer_offset - offset;
			}else{
				printf("sock->websocket_buffer_offset ( %d ) <=  offset ( %d ) \n", sock->websocket_buffer_offset, offset);
			}

			if (diff < 2){
				goto recopy_buffer;
			}

			/* RFC 6455: Reserved Bits (RSV1-3) must be 0 */
			if ( ( sock->websocket_buffer[offset] & 0x70 ) != 0 ){
				LOG(WEBSOCKET_LOG, ERROR_LEVEL, sock->socket,
					"%s", "Protocol error: reserved bits are non-zero");
				goto close_socket_protocol_error;
			}

			wsf.fin = 0;
			if (sock->websocket_buffer[offset] & 0x80){
				wsf.fin = 1;
			}
			wsf.opcode = (Opcodes) (sock->websocket_buffer[offset++] & 0x0F);

			if (sock->websocket_buffer[offset] & 0x80){
				wsf.mask_bit = 1;
				/* 4 Byte Mask */
				extra_bytes += 4;
			} else {
				/* RFC 6455 Section 5.1: Client frames MUST be masked.
				 * "A server MUST close the connection upon receiving a
				 * frame that is not masked." */
				LOG(WEBSOCKET_LOG, ERROR_LEVEL, sock->socket,
					"%s", "Protocol error: received unmasked frame from client");
				goto close_socket_protocol_error;
			}
			wsf.playload_length = sock->websocket_buffer[offset++] & 0x7F;


			if (wsf.playload_length == 126) {
				/* 2 byte extended payload */
				offset += 2;
				extra_bytes += 2;
			}
			if (wsf.playload_length == 127) {
				/* 8 byte extended payload */
				offset += 8;
				extra_bytes += 8;
			}

			if (diff < (2 + extra_bytes )){
				goto recopy_buffer;
			}

			wsf.real_length = wsf.playload_length;

			if (wsf.playload_length == 126) {
				wsf.real_length  = ((uint32_t)sock->websocket_buffer[offset - 1]) << 0;
				wsf.real_length += ((uint32_t)sock->websocket_buffer[offset - 2]) << 8;
			}

			if (wsf.playload_length == 127) {
				if ( ( sock->websocket_buffer[offset - 8] & 0x80 ) == 0x80 ) {
					printf("recFrameV8: the most significant bit MUST be 0\n");
					goto close_socket_error;
				}

				wsf.real_length  = ((uint32_t)sock->websocket_buffer[offset - 1]) << 0;
				wsf.real_length += ((uint32_t)sock->websocket_buffer[offset - 2]) << 8;
				wsf.real_length += ((uint32_t)sock->websocket_buffer[offset - 3]) << 16;
				wsf.real_length += ((uint32_t)sock->websocket_buffer[offset - 4]) << 24;
				#ifndef SHORT_WS_FRAMES_T
				wsf.real_length += ((uint64_t)sock->websocket_buffer[offset - 5]) << 32;
				wsf.real_length += ((uint64_t)sock->websocket_buffer[offset - 6]) << 40;
				wsf.real_length += ((uint64_t)sock->websocket_buffer[offset - 7]) << 48;
				wsf.real_length += ((uint64_t)sock->websocket_buffer[offset - 8]) << 56;
				#else
				if (
					( sock->websocket_buffer[offset - 5] != 0 ) ||
					( sock->websocket_buffer[offset - 6] != 0 ) ||
					( sock->websocket_buffer[offset - 7] != 0 ) ||
					( sock->websocket_buffer[offset - 8] != 0 )
					){
					printf("Error SHORT_WS_FRAMES_T defined Max Frame size 4G\n");
					goto close_socket_error;
				}

				#endif
			}


			/* Extract mask if present (need it for both streaming and normal mode) */
			if (wsf.mask_bit == 1) {
				wsf.mask = &sock->websocket_buffer[offset];
				offset += 4;
			}

			/*
			 * Check for streaming handler BEFORE size limit check.
			 * For streaming we only need the header complete, not the entire payload.
			 * This allows frames larger than WEBSOCKET_MAX_INBUFFER_SIZE.
			 */
			if ((wsf.opcode == WSF_TEXT || wsf.opcode == WSF_BINARY) && wsf.fin == 1) {
				websocket_stream_handler_entry* stream_handler = findWebsocketStreamHandler(sock->header->url);
				if (stream_handler) {
					/* Start streaming mode (non-fragmented) */
					unsigned int header_length = offset;  /* Everything before payload */
					uint32_t total_bytes_after_header = sock->websocket_buffer_offset - header_length;
					uint32_t frame_end_pos = header_length + wsf.real_length;

					if (startWebsocketStream(sock, wsf.real_length, wsf.opcode, wsf.mask, stream_handler, 0) != 0) {
						goto close_socket_error;
					}

					/* Process any payload data already in buffer */
					if (total_bytes_after_header > 0) {
						uint32_t payload_in_buffer = total_bytes_after_header;
						unsigned char* payload_start = sock->websocket_buffer + header_length;

						/* IMPORTANT: Cap at actual frame length to avoid consuming next frame's data */
						if (payload_in_buffer > wsf.real_length) {
							payload_in_buffer = wsf.real_length;
						}

						/* Unmask the initial payload */
						if (wsf.mask_bit == 1) {
							for (uint32_t j = 0; j < payload_in_buffer; j++) {
								payload_start[j] ^= wsf.mask[j % 4];
							}
							sock->websocket_stream_mask_offset = payload_in_buffer % 4;
						}

						/* Pass to handler */
						stream_handler->on_chunk(sock->websocket_stream_ctx, payload_start, payload_in_buffer);
						sock->websocket_stream_ctx->received_length += payload_in_buffer;
						sock->websocket_stream_remaining -= payload_in_buffer;

						/* Check if complete or aborted */
						if (sock->websocket_stream_remaining == 0 || sock->websocket_stream_ctx->aborted) {
							int was_aborted = sock->websocket_stream_ctx->aborted;
							uint64_t remaining_at_abort = sock->websocket_stream_remaining;

							stream_handler->on_end(sock->websocket_stream_ctx, was_aborted ? 0 : 1);
							freeWebsocketStreamContext(sock->websocket_stream_ctx);
							sock->websocket_streaming_active = 0;
							sock->websocket_stream_ctx = NULL;
							sock->websocket_stream_handler = NULL;

							/* If aborted with remaining data, close socket to prevent desync */
							if (was_aborted && remaining_at_abort > 0) {
								LOG(WEBSOCKET_LOG, WARNING_LEVEL, sock->socket,
									"Closing socket due to stream abort with %llu bytes remaining",
									(unsigned long long)remaining_at_abort);
								goto close_socket_error;
							}

							/* Preserve bytes from next frame(s) if coalesced read occurred */
							if (sock->websocket_buffer_offset > frame_end_pos) {
								uint32_t remaining = sock->websocket_buffer_offset - frame_end_pos;
								memmove(sock->websocket_buffer, sock->websocket_buffer + frame_end_pos, remaining);
								sock->websocket_buffer_offset = remaining;
								/* Continue parsing remaining data */
								offset = 0;
								continue;
							}
						}
					}

					/* Clear buffer, return to epoll for more data */
					sock->websocket_buffer_offset = 0;
					return 0;
				}
			}

			/*
			 * Normal (non-streaming) mode: need entire frame in buffer.
			 * Rewind offset to before mask for the size check.
			 */
			if (wsf.mask_bit == 1) {
				offset -= 4;  /* Will re-extract mask after size check */
			}

			if (diff < (2 + extra_bytes + wsf.real_length)){
				if ( ( last_frame_start == 0 ) && ( wsf.real_length > WebserverMallocedSize(sock->websocket_buffer) ) ){

					if ( wsf.real_length > WEBSOCKET_MAX_INBUFFER_SIZE  ){
						printf("Websocket Frame ( %d ) > Max Input Buffer ( %d ) \n", wsf.real_length, WEBSOCKET_MAX_INBUFFER_SIZE);
						goto close_socket_error;
					}

					sock->websocket_buffer = WebserverRealloc( sock->websocket_buffer, 2 + extra_bytes + wsf.real_length + 10 );
					offset = 0;
					goto read_more_data;
				}
				goto recopy_buffer;
			}

			/* Re-extract mask after size check passed */
			if (wsf.mask_bit == 1) {
				wsf.mask = &sock->websocket_buffer[offset];
				offset += 4;
			}

			switch (wsf.opcode) {
			case WSF_TEXT:
			case WSF_BINARY:
				/* RFC 6455: Cannot start new message while fragmented message is in progress.
				 * Check both streaming and normal fragment sequences. */
				if ((sock->websocket_streaming_active && sock->websocket_stream_fragmented) ||
				    sock->websocket_fragment_start_opcode != 0) {
					LOG(WEBSOCKET_LOG, ERROR_LEVEL, sock->socket,
						"Protocol error: received new data frame while fragment sequence active (opcode=%d)",
						wsf.opcode);
					goto close_socket_protocol_error;
				}

				if ( wsf.fin == 1 ){
					if ( wsf.opcode == WSF_TEXT ){
						/* Defensive overflow check for +1 (null terminator) */
						if (wsf.real_length >= WEBSOCKET_MAX_INBUFFER_SIZE ||
						    wsf.real_length > UINT32_MAX - 1) {
							LOG(WEBSOCKET_LOG, ERROR_LEVEL, sock->socket,
								"Text frame too large: %llu >= %d", (unsigned long long)wsf.real_length, WEBSOCKET_MAX_INBUFFER_SIZE);
							goto close_socket_error;
						}
						msg = create_websocket_input_queue_msg(WEBSOCKET_SIGNAL_MSG, sock->websocket_guid, sock->header->url, wsf.real_length + 1 );
					}else{
						msg = create_websocket_input_queue_msg(WEBSOCKET_SIGNAL_MSG, sock->websocket_guid, sock->header->url, wsf.real_length );
					}
					msg->binary = (wsf.opcode == WSF_BINARY) ? 1 : 0;

					for (ui = 0; ui < wsf.real_length; ui++){
						if (wsf.mask_bit == 1) {
							msg->msg[ui] = sock->websocket_buffer[offset + ui] ^ wsf.mask[ui % 4];
						}else{
							msg->msg[ui] = sock->websocket_buffer[offset + ui];
						}
					}
					offset += ui;

					if ( wsf.opcode == WSF_TEXT ){
						msg->msg[wsf.real_length] = '\0';
						/* RFC 6455 Section 5.6: Text frames must contain valid UTF-8 */
						char* utf8_error = NULL;
						if (is_utf8(msg->msg, wsf.real_length, &utf8_error) != 0) {
							LOG(WEBSOCKET_LOG, ERROR_LEVEL, sock->socket,
								"Invalid UTF-8 in text frame: %s", utf8_error ? utf8_error : "unknown error");
							WebserverFree(msg->guid);
							WebserverFree(msg->msg);
							WebserverFree(msg->url);
							WebserverFree(msg);
							goto close_socket_invalid_utf8;
						}
					}

					insert_websocket_input_queue(msg);
				}else{
					/* fin=0: First fragment of a fragmented message */
					websocket_stream_handler_entry* frag_stream_handler = findWebsocketStreamHandler(sock->header->url);
					if (frag_stream_handler) {
						/* Start fragmented streaming mode */
						if (startWebsocketStream(sock, 0, wsf.opcode, NULL, frag_stream_handler, 1) != 0) {
							goto close_socket_error;
						}

						/* Unmask and pass first fragment as chunk */
						tmp = WebserverMalloc(wsf.real_length);
						if (!tmp) {
							goto close_socket_error;
						}
						for (ui = 0; ui < wsf.real_length; ui++) {
							if (wsf.mask_bit == 1) {
								tmp[ui] = sock->websocket_buffer[offset + ui] ^ wsf.mask[ui % 4];
							} else {
								tmp[ui] = sock->websocket_buffer[offset + ui];
							}
						}
						offset += ui;

						frag_stream_handler->on_chunk(sock->websocket_stream_ctx, tmp, wsf.real_length);
						sock->websocket_stream_ctx->received_length += wsf.real_length;
						WebserverFree(tmp);

						/* Check if handler wants to abort */
						if (sock->websocket_stream_ctx->aborted) {
							frag_stream_handler->on_end(sock->websocket_stream_ctx, 0);
							freeWebsocketStreamContext(sock->websocket_stream_ctx);
							sock->websocket_streaming_active = 0;
							sock->websocket_stream_fragmented = 0;
							sock->websocket_stream_ctx = NULL;
							sock->websocket_stream_handler = NULL;
						}
					} else {
						/* Normal fragment buffering (no streaming handler) */
						tmp = WebserverMalloc( wsf.real_length ) ;

						ws_list_append(&sock->websocket_fragments, tmp,0);
						for (ui = 0; ui < wsf.real_length; ui++){
							if (wsf.mask_bit == 1) {
								tmp[ui] = sock->websocket_buffer[offset + ui] ^ wsf.mask[ui % 4];
							}else{
								tmp[ui] = sock->websocket_buffer[offset + ui];
							}
						}
						offset += ui;

						sock->websocket_fragment_start_opcode = wsf.opcode;
						sock->websocket_fragments_length = wsf.real_length;
					}
				}
				break;

			case WSF_CONTINUE:
				/* RFC 6455: CONTINUE frame must follow a Text/Binary frame with fin=0.
				 * Check if we're in a valid fragment sequence. */
				if (!(sock->websocket_streaming_active && sock->websocket_stream_fragmented) &&
				    sock->websocket_fragment_start_opcode == 0) {
					/* Orphan CONTINUE frame - no fragment sequence was started */
					LOG(WEBSOCKET_LOG, ERROR_LEVEL, sock->socket,
						"%s", "Protocol error: received CONTINUE frame without start frame");
					goto close_socket_protocol_error;
				}

				/* Check if we're in fragmented streaming mode */
				if (sock->websocket_streaming_active && sock->websocket_stream_fragmented) {
					websocket_stream_handler_entry* handler = sock->websocket_stream_handler;
					websocket_stream_context* ctx = sock->websocket_stream_ctx;

					if (!handler || !ctx) {
						/* Invalid state - cleanup and close */
						if (ctx) {
							freeWebsocketStreamContext(ctx);
						}
						sock->websocket_streaming_active = 0;
						sock->websocket_stream_fragmented = 0;
						sock->websocket_stream_ctx = NULL;
						sock->websocket_stream_handler = NULL;
						goto close_socket_error;
					}

					/* Unmask and pass chunk to handler */
					tmp = WebserverMalloc(wsf.real_length);
					if (!tmp) {
						handler->on_end(ctx, 0);
						freeWebsocketStreamContext(ctx);
						sock->websocket_streaming_active = 0;
						sock->websocket_stream_fragmented = 0;
						sock->websocket_stream_ctx = NULL;
						sock->websocket_stream_handler = NULL;
						goto close_socket_error;
					}

					for (ui = 0; ui < wsf.real_length; ui++) {
						if (wsf.mask_bit == 1) {
							tmp[ui] = sock->websocket_buffer[offset + ui] ^ wsf.mask[ui % 4];
						} else {
							tmp[ui] = sock->websocket_buffer[offset + ui];
						}
					}
					offset += ui;

					handler->on_chunk(ctx, tmp, wsf.real_length);
					ctx->received_length += wsf.real_length;
					WebserverFree(tmp);

					/* Check if handler wants to abort or if this is the final fragment */
					if (ctx->aborted || wsf.fin == 1) {
						/* Update total_length now that we know it */
						if (wsf.fin == 1) {
							ctx->total_length = ctx->received_length;
						}
						handler->on_end(ctx, ctx->aborted ? 0 : 1);
						freeWebsocketStreamContext(ctx);
						sock->websocket_streaming_active = 0;
						sock->websocket_stream_fragmented = 0;
						sock->websocket_stream_ctx = NULL;
						sock->websocket_stream_handler = NULL;
					}
				} else {
					/* Normal fragment buffering (no streaming) */
					tmp = WebserverMalloc( wsf.real_length ) ;

					ws_list_append(&sock->websocket_fragments, tmp,0);
					for (ui = 0; ui < wsf.real_length; ui++){
						if (wsf.mask_bit == 1) {
							tmp[ui] = sock->websocket_buffer[offset + ui] ^ wsf.mask[ui % 4];
						}else{
							tmp[ui] = sock->websocket_buffer[offset + ui];
						}
					}
					offset += ui;
					sock->websocket_fragments_length += ui;

					if ( wsf.fin == 1 ){
						if ( sock->websocket_fragment_start_opcode == WSF_TEXT ){
							/* Defensive overflow check for +1 (null terminator) */
							if (sock->websocket_fragments_length >= WEBSOCKET_MAX_INBUFFER_SIZE) {
								LOG(WEBSOCKET_LOG, ERROR_LEVEL, sock->socket,
									"Fragmented text frame too large: %u >= %d", sock->websocket_fragments_length, WEBSOCKET_MAX_INBUFFER_SIZE);
								goto close_socket_error;
							}
							msg = create_websocket_input_queue_msg(WEBSOCKET_SIGNAL_MSG, sock->websocket_guid, sock->header->url, sock->websocket_fragments_length + 1 );
						}else{
							msg = create_websocket_input_queue_msg(WEBSOCKET_SIGNAL_MSG, sock->websocket_guid, sock->header->url, sock->websocket_fragments_length );
						}
						msg->binary = (sock->websocket_fragment_start_opcode == WSF_BINARY) ? 1 : 0;

						offset2=0;
						ws_list_iterator_start(&sock->websocket_fragments);
						while ( ( tmp = (unsigned char*)ws_list_iterator_next(&sock->websocket_fragments) )) {
							memcpy( &msg->msg[offset2] , tmp , WebserverMallocedSize(tmp) );
							offset2 += WebserverMallocedSize(tmp);
							WebserverFree(tmp);
						}
						ws_list_iterator_stop(&sock->websocket_fragments);
						ws_list_clear(&sock->websocket_fragments);


						if ( sock->websocket_fragment_start_opcode == WSF_TEXT ){
							msg->msg[sock->websocket_fragments_length] = '\0';
							/* RFC 6455 Section 5.6: Text frames must contain valid UTF-8 */
							char* utf8_error = NULL;
							if (is_utf8(msg->msg, sock->websocket_fragments_length, &utf8_error) != 0) {
								LOG(WEBSOCKET_LOG, ERROR_LEVEL, sock->socket,
									"Invalid UTF-8 in reassembled text frame: %s", utf8_error ? utf8_error : "unknown error");
								WebserverFree(msg->guid);
								WebserverFree(msg->msg);
								WebserverFree(msg->url);
								WebserverFree(msg);
								sock->websocket_fragment_start_opcode = 0;
								sock->websocket_fragments_length = 0;
								goto close_socket_invalid_utf8;
							}
						}

						sock->websocket_fragment_start_opcode = 0;
						sock->websocket_fragments_length = 0;
						insert_websocket_input_queue(msg);

					}
				}

				break;

			case WSF_CLOSE:
				/* RFC 6455: Control frames MUST NOT be fragmented */
				if (wsf.fin == 0) {
					LOG(WEBSOCKET_LOG, ERROR_LEVEL, sock->socket,
						"%s", "Protocol error: CLOSE control frame must not be fragmented");
					goto close_socket_protocol_error;
				}
				/* RFC 6455: Control frame payload MUST be <= 125 bytes */
				if (wsf.real_length > 125) {
					LOG(WEBSOCKET_LOG, ERROR_LEVEL, sock->socket,
						"Protocol error: CLOSE payload too large (%llu > 125)",
						(unsigned long long)wsf.real_length);
					goto close_socket_protocol_error;
				}
				sendCloseFrame(sock);
				// sanitizer sagt das schreiben hier ist nicht gelockt
				// aber der socket sollte schon gelockt sein wenn der handler aufgerufen wird
				//PlatformLockMutex(&sock->socket_mutex);
				sock->closeSocket = 1;
				//PlatformUnlockMutex(&sock->socket_mutex);
#ifdef ENABLE_DEVEL_WARNINGS
				#warning thread sanitizer sagt hier wird ohne lock geschrieben
#endif
				return 0;

			case WSF_PONG:
				/* RFC 6455: Control frames MUST NOT be fragmented */
				if (wsf.fin == 0) {
					LOG(WEBSOCKET_LOG, ERROR_LEVEL, sock->socket,
						"%s", "Protocol error: PONG control frame must not be fragmented");
					goto close_socket_protocol_error;
				}
				/* RFC 6455: Control frame payload MUST be <= 125 bytes */
				if (wsf.real_length > 125) {
					LOG(WEBSOCKET_LOG, ERROR_LEVEL, sock->socket,
						"Protocol error: PONG payload too large (%llu > 125)",
						(unsigned long long)wsf.real_length);
					goto close_socket_protocol_error;
				}
				/* Skip over payload - use real_length, not ui which may be stale */
				offset += wsf.real_length;
				break;

			case WSF_PING:
				/* RFC 6455: Control frames MUST NOT be fragmented */
				if (wsf.fin == 0) {
					LOG(WEBSOCKET_LOG, ERROR_LEVEL, sock->socket,
						"%s", "Protocol error: PING control frame must not be fragmented");
					goto close_socket_protocol_error;
				}
				/* RFC 6455: Control frame payload MUST be <= 125 bytes */
				if (wsf.real_length > 125) {
					LOG(WEBSOCKET_LOG, ERROR_LEVEL, sock->socket,
						"Protocol error: PING payload too large (%llu > 125)",
						(unsigned long long)wsf.real_length);
					goto close_socket_protocol_error;
				}
				for (ui = 0; ui < wsf.real_length; ui++){
					if (wsf.mask_bit == 1) {
						sock->websocket_buffer[offset + ui] = sock->websocket_buffer[offset + ui] ^ wsf.mask[ui % 4];
					}
				}
				sendWebsocketFrame(WSF_PONG, sock->websocket_guid,(const  char* ) &sock->websocket_buffer[offset] , wsf.real_length );
				offset += wsf.real_length;
				break;

			case WSF_RESERVED1:
			case WSF_RESERVED2:
			case WSF_RESERVED3:
			case WSF_RESERVED4:
			case WSF_RESERVED5:
			case WSF_RESERVED6:
			case WSF_RESERVED7:
			case WSF_RESERVED8:
			case WSF_RESERVED9:
			case WSF_RESERVED10:
				LOG(WEBSOCKET_LOG, ERROR_LEVEL, sock->socket,
					"Protocol error: reserved opcode %d received", wsf.opcode);
				goto close_socket_protocol_error;

			/*default:
				printf("Websocket Frame V8 Opcode %d nicht behandelt\n", wsf.opcode);
				return 0;*/
			}

			/* 
			 Wenn alle Bytes aus dem websocket_buffer verarbeitet wurden
			 beide offset wieder auf anfang setzen
			*/
			if (offset == sock->websocket_buffer_offset) {
				sock->websocket_buffer_offset = 0;
				offset = 0;
				break;
			}
		}
		continue;

		recopy_buffer:

			/* Wenn bearbeitete Frames im Buffer sind letztes Frame nach vorne kopieren */
			if ( last_frame_start > 0 ) {
				diff = sock->websocket_buffer_offset - last_frame_start;
				for (i = 0; i < diff; i++) {
					sock->websocket_buffer[i] = sock->websocket_buffer[last_frame_start + i];
				}
				sock->websocket_buffer_offset = diff;
			}else{
				printf("recopy_buffer last_frame_start = 0 \n");
			}
			offset = 0;

		read_more_data:
			continue;
	}

	return 0;

close_socket_error:
	sendCloseFrame(sock);
	sock->closeSocket = 1;
	return -1;

close_socket_protocol_error:
	/* RFC 6455: Status 1002 = Protocol Error */
	sendCloseFrameWithStatus(sock->websocket_guid, 1002);
	sock->closeSocket = 1;
	return -1;

close_socket_invalid_utf8:
	/* RFC 6455: Status 1007 = Invalid frame payload data (e.g., non-UTF-8 text) */
	sendCloseFrameWithStatus(sock->websocket_guid, 1007);
	sock->closeSocket = 1;
	return -1;
}

int recFrame(socket_info *sock) {

	if (sock == 0){
		return -1;
	}

	if (sock->header->SecWebSocketVersion > 6){
		return recFrameV8(sock);
	}

	LOG( WEBSOCKET_LOG, ERROR_LEVEL, sock->socket,"%s", "Websocket Frame < 7 not handled");
	return -1;
}

#endif
