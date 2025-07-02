/*

SPDX-License-Identifier: MPL-2.0

 libCWebUI
 Copyright (C) 2007 - 2019  Ramin Seyed-Moussavi

 Projekt URL : https://github.com/lordrasmus/libcwebui

 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at https://mozilla.org/MPL/2.0/. 

*/

#ifdef __CDT_PARSER__
	#define __BASE_FILE__ base
#endif

#include "webserver.h"
#include <miniz.h>

#ifdef DMALLOC
#include <dmalloc/dmalloc.h>
#endif

#ifdef ENABLE_DEVEL_WARNINGS
	#warning feste Buffer entfernen
#endif



static void writeChunk(list_t* liste, const unsigned char* text, unsigned int length) {
	html_chunk* chunk = 0, *tmp;
	unsigned int to_write, diff, offset = 0;
	if (ws_list_size(liste) == 0) {
		chunk = WebserverMallocHtml_chunk();
		ws_list_append(liste, chunk,0);
	}

	if (chunk == 0) {
		tmp = (html_chunk*)ws_list_get_at(liste, ws_list_size(liste) - 1);
		if ( ( tmp != 0 ) && (tmp->length <= 2000) ) {
			chunk = tmp;
		}
	}

	while (length > 0) {
		if (chunk == 0) {
			chunk = WebserverMallocHtml_chunk();
			ws_list_append(liste, chunk,0);
		}
		diff = 2000 - chunk->length;
		if (length < diff){
			to_write = length;
		}else{
			to_write = diff;
		}

		memcpy(&chunk->text[chunk->length], &text[offset], to_write);
		chunk->length += to_write;
		length -= to_write;
		offset += to_write;
		chunk = 0;
	}

	/*if ((*liste) != 0) {
	 tmp = (html_chunk*) (*liste)->value;
	 if ((tmp->length + length) <= 2000) {
	 chunk = tmp;
	 if (chunk->text == 0) {
	 printf("Null Chunk Text ???\n");
	 }
	 }
	 }

	 */

}

static void writeChunkVariable(list_t* liste, ws_variable* var) {
	char buffer[20];
	ws_variable* it;
	if (var == 0){
		return;
	}

	switch (var->type) {
	case VAR_TYPE_STRING:
		writeChunk(liste, (unsigned char*) var->val.value_string, var->extra.str_len);
		break;
	case VAR_TYPE_INT:
		getWSVariableString(var, buffer, sizeof( buffer) );
		writeChunk(liste, (unsigned char*) buffer, strlen(buffer));
		break;
	case VAR_TYPE_ULONG:
		getWSVariableString(var, buffer, sizeof( buffer));
		writeChunk(liste, (unsigned char*) buffer, strlen(buffer));
		break;
    case VAR_TYPE_LONG:
		getWSVariableString(var, buffer, sizeof( buffer));
		writeChunk(liste, (unsigned char*) buffer, strlen(buffer));
		break;
	case VAR_TYPE_REF:
		writeChunkVariable(liste, var->val.value_ref);
		break;
	case VAR_TYPE_ARRAY:
		writeChunk(liste, (unsigned char*) "Array <br> ", 10);
		it = getWSVariableArrayFirst( var );
		while( it != 0 ){
			writeChunk(liste, (unsigned char*) "[", 1);
			writeChunk(liste, (unsigned char*) it->name, it->name_len);
			writeChunk(liste, (unsigned char*) ":", 1);
			writeChunkVariable( liste , it );
			writeChunk(liste, (unsigned char*) "]<br>", 5);
			it = getWSVariableArrayNext( var );
		}
		break;
	case VAR_TYPE_EMPTY:
		break;

	case VAR_TYPE_CUSTOM_DATA:
		writeChunk(liste, (unsigned char*)"custom_data", strlen("custom_data") );
		break;
	}
}

void sendHeaderChunk(socket_info* sock, const char* text, const unsigned int length) {
	writeChunk(&sock->header_chunk_list, (unsigned char*) text, length);
}

void sendHTMLChunk(socket_info* sock, const char* text, const unsigned int length) {
	if (length == 0){
		return;
	}
	if (sock->disable_output == 1){
		return;
	}
	writeChunk(&sock->html_chunk_list, (unsigned char*) text, length);
}

void VISIBLE_ATTR sendHTMLChunkVariable(socket_info* sock, ws_variable* var) {
	if (sock->disable_output == 1){
		return;
	}
	writeChunkVariable(&sock->html_chunk_list, var);
}

#ifdef WEBSERVER_USE_WEBSOCKETS
void sendWebsocketChunk(socket_info* sock, const unsigned char* text, const unsigned int length) {
	writeChunk(&sock->websocket_chunk_list, text, length);
}
#endif

unsigned long getChunkListSize(list_t* liste) {
	unsigned long ret = 0;
	html_chunk* chunk;

	ws_list_iterator_start(liste);
	while ( ( chunk = (html_chunk*)ws_list_iterator_next(liste) )) {
		ret += chunk->length;
	}
	ws_list_iterator_stop(liste);
	return ret;
}

int isChunkListbigger(list_t* liste, int bytes){
	int bigger = 0;
	int count = 0;
	html_chunk* chunk;

	ws_list_iterator_start(liste);
	while ( ( chunk = (html_chunk*)ws_list_iterator_next(liste) )) {
		count += chunk->length;
		
		if ( count > bytes ){
			bigger = 1;
			break;
		}
	}
	ws_list_iterator_stop(liste);
	
	return bigger;
}

#pragma GCC diagnostic ignored "-Wformat-nonliteral"
void vprintHeaderChunk(socket_info* sock, const char *fmt, va_list argptr) {
	int l;
	char *tmp;
	va_list argcopy;
	
	va_copy(argcopy, argptr);

	l = vsnprintf( 0, 0, fmt, argptr);
	tmp = WebserverMalloc( l + 1 ); // +1 wegen dem \0
	
	l = vsnprintf(tmp, l +1, fmt, argcopy);
	
	
	writeChunk(&sock->header_chunk_list, (unsigned char*) tmp, l);
	
	WebserverFree( tmp );
}
#pragma GCC diagnostic warning "-Wformat-nonliteral"

void printHeaderChunk(socket_info* sock, const char *fmt, ... ) {
	va_list argptr;
	
	if (sock == 0){
		return;
	}
	
	va_start ( argptr, fmt );
	vprintHeaderChunk(sock,fmt,argptr);
	va_end ( argptr );
}

#pragma GCC diagnostic ignored "-Wformat-nonliteral"
int vprintHTMLChunk(socket_info* sock, const char *fmt, va_list argptr) {
	int l;
	char *tmp;
	va_list argcopy;
	
	if (sock == 0){
		return 0;
	}
	
	// Kopie von argptr erstellen. ansonsten funktionert das zweite vsnprintf() nicht
	// weil argptr schon auf dem letzten argument steht
	va_copy(argcopy, argptr);

	l = vsnprintf( 0, 0, fmt, argptr);
	tmp = WebserverMalloc( l + 1 ); // +1 wegen dem \0

	l = vsnprintf(tmp, l + 1 , fmt, argcopy);

	writeChunk(&sock->html_chunk_list, (unsigned char*) tmp, l );
	WebserverFree( tmp );
	return l;
}
#pragma GCC diagnostic warning "-Wformat-nonliteral"

int printHTMLChunk(socket_info* sock, const char *fmt, ... ) {
	int ret=0;
	va_list argptr;
	va_start(argptr,fmt);
	ret = vprintHTMLChunk(sock,fmt,argptr);
	va_end(argptr);
	return ret;
}

#ifdef WEBSERVER_USE_WEBSOCKETS

static void vprintWebsocketChunk(socket_info* sock, const char *fmt, va_list argptr) {
	int l;
	char *tmp;
	va_list argcopy;
	
	if (sock == 0){
		return;
	}
	
	va_copy(argcopy, argptr);
	
	l = vsnprintf( 0, 0, fmt, argptr);
	tmp = WebserverMalloc( l + 1 ); // +1 wegen dem \0
	
	l = vsnprintf(tmp, l + 1, fmt, argcopy);
	
	writeChunk(&sock->websocket_chunk_list, (unsigned char*) tmp, l);
	
	WebserverFree( tmp );
}

void printWebsocketChunk(socket_info* sock, const char *fmt, ... ) {
	va_list argptr;
	va_start(argptr,fmt);
	vprintWebsocketChunk(sock,fmt,argptr);
	va_end(argptr);
}

#endif


static char* get_status( tdefl_status status ){
	switch( status ){
		case TDEFL_STATUS_BAD_PARAM: return "TDEFL_STATUS_BAD_PARAM";
		case TDEFL_STATUS_OKAY: return "TDEFL_STATUS_OKAY";
		case TDEFL_STATUS_DONE: return "TDEFL_STATUS_DONE";
		case TDEFL_STATUS_PUT_BUF_FAILED: return "TDEFL_STATUS_PUT_BUF_FAILED";
	}
	
	return "unknown";
	
}
static unsigned long writeChunksToBuffer(list_t* liste, char* out_buffer, int compress) {
	html_chunk* chunk;
	unsigned long offset = 0;
	tdefl_status status;
	tdefl_compressor *deflator = 0;
	
	if ( compress == 1 ){
		int level = 5;
		// The number of dictionary probes to use at each compression level (0-10). 0=implies fastest/minimal possible probing.
		static const mz_uint s_tdefl_num_probes[11] = { 0, 1, 6, 32,  16, 32, 128, 256,  512, 768, 1500 };
		
		// create tdefl() compatible flags (we have to compose the low-level flags ourselves, or use tdefl_create_comp_flags_from_zip_params() but that means MINIZ_NO_ZLIB_APIS can't be defined).
		mz_uint comp_flags = s_tdefl_num_probes[MZ_MIN(10, level)] | ((level <= 3) ? TDEFL_GREEDY_PARSING_FLAG : 0);
		// mit level == 0 kann man noch das anhängen
		// 		comp_flags |= TDEFL_FORCE_ALL_RAW_BLOCKS;


		deflator = tdefl_compressor_alloc();
		// Initialize the low-level compressor.
		status = tdefl_init( deflator, NULL, NULL, comp_flags);
		if (status != TDEFL_STATUS_OKAY){
			printf("tdefl_init() failed!\n");
			return EXIT_FAILURE;
		}
	}

	ws_list_iterator_start(liste);
	while ( ( chunk = (html_chunk*) ws_list_iterator_next(liste) ) ) {
		if ( compress == 1 ){
			
			size_t in_bytes = chunk->length;
			size_t out_bytes = 1000000;
			// Compress as much of the input as possible (or all of it) to the output buffer.
			status = tdefl_compress( deflator, chunk->text, &in_bytes, &out_buffer[offset], &out_bytes, TDEFL_FULL_FLUSH);
			offset += out_bytes;
			if ( status != TDEFL_STATUS_OKAY ){
				printf("status : %s %ld\n",get_status(status),offset);
			}
		}else{
			memcpy(&out_buffer[offset], chunk->text, chunk->length);
			offset += chunk->length;
		}
		WebserverFreeHtml_chunk(chunk);
	}
	ws_list_iterator_stop(liste);
	
	
	if ( compress == 1 ){
		size_t in_bytes = 0;
		size_t out_bytes = 1000000;
		status = tdefl_compress( deflator, 0, &in_bytes, &out_buffer[offset], &out_bytes, TDEFL_FINISH);
		offset += out_bytes;
		if ( status != TDEFL_STATUS_DONE ){
			printf("status : %s %ld\n",get_status(status),offset);
		}
		tdefl_compressor_free( deflator );
	}
	
	ws_list_clear(liste);
	return offset;
}

void generateOutputBuffer(socket_info* sock) {
	char* buffer;
	uint32_t offset = 0;
	
	
	output_struct *output = WebserverMalloc( sizeof( output_struct ) );
	memset( output, 0 , sizeof( output_struct ) );

	if ( ( sock->use_output_compression == 1 ) && ( sock->send_file_info == 0 ) ){
		unsigned long body_size = getChunkListSize(&sock->html_chunk_list);
		buffer = (char*) WebserverMalloc ( body_size + 1  ); /* +1 fuer Header Debug '\0' */
		
		offset = writeChunksToBuffer(&sock->html_chunk_list, &buffer[offset], 1 );
		output->body.buffer = buffer;
		output->body.buffer_size = offset;
		
		if ( body_size == 0){
			printf("url : %s\n",sock->header->url);
		}
#if 0
		double proz = offset;
		proz /= (double)body_size;
		proz *= 100.0;
		printf("orig: %lu  compress: %lu  %.2f %%\n",body_size,offset,proz );
#endif
		
		printHeaderChunk(sock, "Content-Encoding: deflate\r\n");
		printHeaderChunk(sock, "Content-Length: %"PRIu32"\r\n", offset);
		printHeaderChunk(sock, "\r\n"); /* HTTP Header beenden */
		
		unsigned long header_size = getChunkListSize(&sock->header_chunk_list);
		buffer = (char*) WebserverMalloc ( header_size + 1  ); /* +1 fuer Header Debug '\0' */
		
		offset = writeChunksToBuffer(&sock->header_chunk_list, buffer, 0);
		
		output->header.buffer = buffer;
		output->header.buffer_size = offset;
		ws_list_append( &sock->output_list, output, 0 );
		return;
	}
	
	unsigned long header_size = getChunkListSize(&sock->header_chunk_list);
	unsigned long body_size = getChunkListSize(&sock->html_chunk_list);
	
	unsigned long size = header_size + body_size;
	buffer = (char*) WebserverMalloc ( size + 1  ); /* +1 fuer Header Debug '\0' */

	offset = writeChunksToBuffer(&sock->header_chunk_list, buffer, 0);

#ifdef _WEBSERVER_HEADER_DEBUG_
	buffer[offset]='\0';
	LOG (HEADER_PARSER_LOG,NOTICE_LEVEL,sock->socket, "-------- sending Header --------\r\n %s -------- Header end --------",buffer );
#endif

	offset += writeChunksToBuffer(&sock->html_chunk_list, &buffer[offset], 0 );

#ifdef _WEBSERVER_BODY_DEBUG_
	WebServerPrintf ( "%s",buffer );
#endif

#ifdef _WEBSERVER_DEBUG_
	LOG ( CONNECTION_LOG,NOTICE_LEVEL,sock->socket, "compiled HTML Size %d ",size );
#endif
	output->body.buffer = buffer;
	output->body.buffer_size = offset;
	
	if ( sock->send_file_info != 0 ){

		output->file_infos.file_info= sock->send_file_info;
		output->file_infos.file_send_pos = 0;

		sock->send_file_info = 0;
	}

	ws_list_append( &sock->output_list, output, 0 );

}

