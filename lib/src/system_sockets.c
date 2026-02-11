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

#ifdef __CDT_PARSER__
	#define __BASE_FILE__
#endif

#ifdef DMALLOC
#include <dmalloc/dmalloc.h>
#endif



/*
 http://tangentsoft.net/wskfaq/articles/bsd-compatibility.html
 http://publib.boulder.ibm.com/infocenter/iseries/v5r3/index.jsp?topic=/rzab6/rzab6xnonblock.htm
 http://www.wangafu.net/~nickm/libevent-book/01_intro.html
*/

#ifdef WEBSERVER_USE_WNFS
	/* socket für den nfs zugriff */
	int wnfs_socket = 0;
#endif


int WebserverStartConnectionManager(void) {
	int socket;
	socket_info* info;


	socket = PlatformGetSocket(getConfigInt("port"), globals.config.connections);

	if (socket < 0) {
		LOG(CONNECTION_LOG, ERROR_LEVEL, 0, "Error : Server Socket %d konnte nicht erzeugt werden werden\n", getConfigInt("port"));
		return -1;
	}

	info = WebserverMallocSocketInfo();
	PlatformCreateMutex(&info->socket_mutex);
	info->active = 1;

#ifdef WEBSERVER_USE_SSL
	info->use_ssl = 0;
#endif

	info->port = getConfigInt("port");
	info->socket = socket;
	info->server = 1;
	PlatformSetNonBlocking(socket);
	addSocketContainer(info);
	addEventSocketReadPersist(info);



	/* AB hier wird der SSL Socket initialisiert */

#ifdef WEBSERVER_USE_SSL
	int ssl_port = getConfigInt("ssl_port");
	if ( ssl_port > 0 ){
		socket = PlatformGetSocket(getConfigInt("ssl_port"), globals.config.connections);
		if (socket < 0) {
			LOG(CONNECTION_LOG, ERROR_LEVEL, 0, "Error : Server SSL Socket %d konnte nicht erzeugt werden werden", getConfigInt("ssl_port"));
			return -1;
		}

		info = WebserverMallocSocketInfo();
		PlatformCreateMutex(&info->socket_mutex);
		info->active = 1;
		info->use_ssl = 1;
		info->port = ssl_port;
		info->socket = socket;
		info->server = 1;
		PlatformSetNonBlocking(socket);
		addSocketContainer(info);
		addEventSocketReadPersist(info);
	}

#endif

#ifdef WEBSERVER_USE_SSL
	if ( ssl_port > 0 ){
		LOG(CONNECTION_LOG, NOTICE_LEVEL, 0, "webserver listening on port %d & port %d (SSL) ( PID %d )",
			getConfigInt("port"), getConfigInt("ssl_port"), getpid());
	}else{
		LOG( CONNECTION_LOG, NOTICE_LEVEL, 0, "webserver listening on port %d ( PID %d )",
			getConfigInt("port"), getpid());
	}
#else
	LOG( CONNECTION_LOG, NOTICE_LEVEL, 0, "webserver listening on port %d ( PID %d )",
		getConfigInt("port"), PlatformGetPID());
#endif

#ifdef WEBSERVER_USE_WNFS

	socket = PlatformGetSocket(4444, 1 );
	if (socket < 0) {
		LOG(CONNECTION_LOG, ERROR_LEVEL, 0, "Error : Server SSL Socket %d konnte nicht erzeugt werden werden", getConfigInt("ssl_port"));
		return -1;
	}

	info = WebserverMallocSocketInfo();
	PlatformCreateMutex(&info->socket_mutex);
	info->active = 1;
#ifdef WEBSERVER_USE_SSL
	info->use_ssl = 0;
#endif
	info->port = 4444;
	info->socket = socket;
	info->server = 2;
	PlatformSetNonBlocking(socket);
	addSocketContainer(info);
	addEventSocketReadPersist(info);

	LOG( CONNECTION_LOG, NOTICE_LEVEL, 0, "Webserver NFS Online auf Port %d ",info->port);

#endif

	return 0;
}


static socket_info* addClientSocket(int s, char ssl) {
	socket_info* info;
	info = WebserverMallocSocketInfo();
	if (info != 0) {
		PlatformCreateMutex(&info->socket_mutex);
		info->header = WebserverMallocHttpRequestHeader();
#ifdef WEBSERVER_USE_SSL
		info->use_ssl = ssl;
#endif
		info->client = 1;
		info->server = 0;
		info->active = 1;
		info->socket = s;
		addSocketContainer(info);
		addEventSocketRead(info); /*  | EV_PERSIST| EV_WRITE */
		return info;
	}
	return 0;
}

void reCopyHeaderBuffer(socket_info* sock, unsigned int end) {
	unsigned int i, i2;
	i2 = end; /* end ist das letzte geparste zeichen */
	for (i = 0; i < sock->header_buffer_pos - end; i++, i2++) {
		sock->header_buffer[i] = sock->header_buffer[i2];
	}
	sock->header_buffer[i] = '\0';
	sock->header_buffer_pos = i;
}

void WebserverConnectionManagerCloseRequest(socket_info* sock) {

	sock->active = 0;
	sock->closeSocket = 1;
#ifdef WEBSERVER_USE_WEBSOCKETS
	if ( sock->isWebsocket == 1 ) {
		/* laenge 1 um keinen malloc mit 0 bytes zu machen */
		
		websocket_queue_msg* msg = create_websocket_input_queue_msg(WEBSOCKET_SIGNAL_DISCONNECT,sock->websocket_guid,sock->header->url,1);
		insert_websocket_input_queue( msg);
		
#if _WEBSERVER_CONNECTION_DEBUG_ > 1
		LOG ( CONNECTION_LOG,NOTICE_LEVEL, sock->socket, "Websocket Close Request" );
#endif
	}
#endif

	delEventSocketAll(sock);
	deleteSocket(sock);

	WebserverCloseSocket(sock);

	WebserverFreeSocketInfo(sock);
}

post_handler post_handle_func = 0;

static void call_pre_post_handler( socket_info* sock ){

	if ( post_handle_func != 0 ){
		post_handle_func( "pre", sock->header->url );
	}

}

static void call_post_post_handler( socket_info* sock ){

	if ( post_handle_func != 0 ){
		post_handle_func( "post", sock->header->url );
	}

}

static int recv_post_payload( socket_info* sock, const char* buffer, uint32_t len){

	/*
	LOG(CONNECTION_LOG,NOTICE_LEVEL,sock->socket,"POST Data %d of %d -> %d",sock->header->post_buffer_pos,sock->header->contentlenght, len);
	*/

	if ( sock->header->post_buffer == 0 ){
		/* WebserverMalloc ruft exit(1) bei Fehler auf, kann nicht NULL zurückgeben */
		sock->header->post_buffer = WebserverMalloc( sock->header->contentlenght + 1 );
		/* um Payload Buffer beim Debug ausgeben zu können */
		sock->header->post_buffer[sock->header->contentlenght] = '\0';

		call_pre_post_handler( sock );

	}

	if ( ( sock->header->post_buffer_pos + len) <= sock->header->contentlenght ){

		memcpy( &sock->header->post_buffer[sock->header->post_buffer_pos], buffer , len );
		sock->header->post_buffer_pos += len;
		sock->header_buffer_pos = 0;

	}else{

		uint32_t diff = sock->header->contentlenght - sock->header->post_buffer_pos;

		memcpy( &sock->header->post_buffer[sock->header->post_buffer_pos], buffer , diff );
		sock->header->post_buffer_pos += diff;

		if ( len > diff ){
			sock->header_buffer_pos = diff;
			reCopyHeaderBuffer(sock, diff);
			// TODO verarbeitung von weiteren header bytes noch testen
			call_post_post_handler( sock );

			return 1;
		}

		sock->header_buffer_pos = 0;
	}

	if ( sock->header->post_buffer_pos == sock->header->contentlenght ){

		call_post_post_handler( sock );

		return 1;  
	}

	return 0;
}


static int check_post_header( socket_info* sock ){

	//LOG(CONNECTION_LOG,NOTICE_LEVEL,sock->socket,"header->contentlenght %" PRIu64 " ",sock->header->contentlenght );

	if ( sock->header->contentlenght > (uint64_t)getConfigInt("max_post_size") ){
		LOG(CONNECTION_LOG,ERROR_LEVEL,sock->socket,"header->contentlenght > max_post_size ( %" PRIu64 " > %d ) ",sock->header->contentlenght, getConfigInt("max_post_size") );
		return -1;
	}

	/*if ( sock->header->contenttype == 0 ){
		LOG(CONNECTION_LOG,ERROR_LEVEL,sock->socket,"%s","header->contenttype == 0 ");
		//LOG(CONNECTION_LOG,ERROR_LEVEL,sock->socket,"%s",sock->header_buffer);
		// nicht einfach den Buffer ausgeben
		return -1;
	}*/

	if ( ( sock->header->contenttype == MULTIPART_FORM_DATA ) && ( sock->header->boundary == 0 ) ){
		LOG(CONNECTION_LOG,ERROR_LEVEL,sock->socket,"%s","header->boundary == 0 ");
		//LOG(CONNECTION_LOG,ERROR_LEVEL,sock->socket,"%s",sock->header_buffer);
		// nicht einfach den Buffer ausgeben
		return -1;
	}

	return 0;
}

/*
 *
 * Return Values :
 *
 * 		0 = Header noch nicht zuende
 * 		1 = Header zuende aber noch Daten vorhanden
 *	   -1 = Fehler beim Header empfangen
 *
 */

static int handleClientHeaderData(socket_info* sock) {
	/*
	 * len2 must be initialized to -6 (no complete line recognized).
	 * When the header buffer is full, recv() is skipped and we jump directly
	 * to the ParseHeader check. If header_complete == 1 (e.g. during POST body
	 * handling), ParseHeader is also skipped, and len2 would be uninitialized.
	 * With -6 as default, we safely return 0 and wait for more data.
	 */
	int len2 = -6;
	unsigned int buffer_length = WEBSERVER_MAX_HEADER_LINE_LENGHT * 1;
	unsigned int parsed = 0;

	unsigned int this_post_read = 0;

	sock->send_file_info = 0;

	if (sock->header_buffer == 0) {
		sock->header_buffer = (char*) WebserverMalloc( buffer_length );
		sock->header_buffer_pos = 0;
		sock->header_buffer_size = buffer_length;
	}

	while (1) {

		if ( sock->header_buffer_pos < buffer_length) {
			int len = 0;
			unsigned char* p =(unsigned char*) &sock->header_buffer[sock->header_buffer_pos];
			int read_length = buffer_length - sock->header_buffer_pos;
			len = WebserverRecv(sock, p ,read_length , 0);
			#ifdef _WEBSERVER_SOCKET_DEBUG_
				LOG(CONNECTION_LOG,NOTICE_LEVEL,sock->socket,"Client Header Recv %d",len);
			#endif

			if ((len == CLIENT_NO_MORE_DATA) && (sock->header_buffer_pos == 0)) {
				return 0;
			}

			if ((len == CLIENT_DISCONNECTED) || (len == CLIENT_UNKNOWN_ERROR) || (len == SSL_PROTOCOL_ERROR)) {
#if _WEBSERVER_CONNECTION_DEBUG_ > 1
				LOG ( CONNECTION_LOG,NOTICE_LEVEL,sock->socket,"Client Disconnected" );
#endif
				sock->closeSocket = 1;
				return -1;
			}
			if ( ( sock->header->method == HTTP_POST ) && ( sock->header->header_complete == 1 ) )  {
				/*
				LOG(CONNECTION_LOG,NOTICE_LEVEL,sock->socket,"POST %d of %d",sock->header->post_buffer_pos,sock->header->contentlenght);
				*/
				if ( sock->header->contentlenght > sock->header->post_buffer_pos ) {
					if ( len == 0 ){
						return 0;
					}
					if ( 1 == recv_post_payload( sock ,sock->header_buffer, len ) ){
						sock->header->header_complete = 0;
						return 1;	/* Aktuellen POST Request  gelesen, weiter Daten sind der nächste Header */
					}
				}
				// Die schleife unterbrechen damit andere Sockets dran kommen
				this_post_read+=len;
				if ( this_post_read > 20000 ){
					return 0;
				}

				if ( len == 0 ){
					return 0;
				}

				continue;
			}

			sock->header_buffer_pos += len;
		}

		if ( sock->header->header_complete == 0 ){
			len2 = ParseHeader(sock, sock->header, sock->header_buffer, sock->header_buffer_pos, &parsed);
			if (len2 > 0) {
				reCopyHeaderBuffer(sock, parsed);
				continue;
			}else{
				if ( parsed > 0 ){
					reCopyHeaderBuffer(sock, parsed);
					parsed = 0;
				}
			}
		}

		/* Keine Zeile erkannt weiter Daten lesen */
		if (len2 == -6) {
			/* zur sicherheit nochmal prüfen ob der header buffer voll ist
			 * wen ja haben wir hier einen internen fehler */
			if ( ( buffer_length - sock->header_buffer_pos ) == 0 ){
				sendInternalError(sock);
				sock->closeSocket = 1;
				return -1;
			}
			return 0;
		}

		/* Header Zeile zu lang */
		if (len2 == 0) {
			sendMethodBadRequestLineToLong(sock);
			LOG ( HEADER_PARSER_LOG,NOTICE_LEVEL,sock->socket,"sendMethodBadRequestLineToLong > %d",WEBSERVER_MAX_HEADER_LINE_LENGHT );
			sock->closeSocket = 1;
			return -1;
		}

		/* methode nicht erlaubt */
		if (len2 == -4) {
			if ( sock->header->no_websocket_support == 1 ){
				LOG ( HEADER_PARSER_LOG,NOTICE_LEVEL,sock->socket,"%s","Websocket Support disabled" );
			}else{
				sendMethodNotAllowed(sock);
				LOG ( HEADER_PARSER_LOG,NOTICE_LEVEL,sock->socket,"methodNotAllowed -> %s",sock->header->error_method );
			}
			sock->closeSocket = 1;
			return -1;
		}
#ifdef WEBSERVER_USE_WEBSOCKETS
		if ( 1 == checkIskWebsocketConnection(sock) ){
			sock->header->header_complete = 0;
			return 1;
		}
#endif

		/* Header zuende aber noch weitere daten vorhanden */
		if (len2 == -3) {
			if ( sock->header->method == HTTP_POST ){
				if ( -1 == check_post_header( sock ) ){
					sock->header->error = 1;
					sendMethodBadRequest(sock);
					LOG ( HEADER_PARSER_LOG,NOTICE_LEVEL,sock->socket,"%s","POST Method Header Error" );
					return -1;
				}

				/* POST ohne Body (Content-Length: 0) direkt verarbeiten */
				if ( sock->header->contentlenght == 0 ) {
					sock->header->header_complete = 0;
					return 1;
				}

				// Das muss hier sein weil noch Teil der Daten im Header Buffer sein können
				if ( 1 == recv_post_payload( sock, sock->header_buffer, sock->header_buffer_pos )){
					sock->header->header_complete = 0;
					return 1; /* Aktuellen POST Request  gelesen, weiter Daten sind der nächste Header */
				}

				continue;
			}
			sock->header->header_complete = 0;
			return 1;
		}

		/* Header ist zuende und keine weiteren daten */
		if (len2 == -2){
			sock->header_buffer_pos = 0;
			sock->header_buffer[0] = '\0';
			if ( sock->header->method == HTTP_POST ){
				if ( -1 == check_post_header( sock ) ){
					sock->header->error = 1;
					sendMethodBadRequest(sock);
					LOG ( HEADER_PARSER_LOG,NOTICE_LEVEL,sock->socket,"%s","POST Method Header Error" );
					return -1;
				}

				/* POST ohne Body (Content-Length: 0) direkt verarbeiten */
				if ( sock->header->contentlenght == 0 ) {
					sock->header->header_complete = 0;
					return 1;
				}

				/* Header ist zuende aber der POST Payload fehlt noch */
				return 0;
			}
#if _WEBSERVER_CONNECTION_DEBUG_ > 4
			LOG ( HEADER_PARSER_LOG,NOTICE_LEVEL,sock->socket,"%s","Client Header Complete" );
#endif
			sock->header->header_complete = 0;
			return 1;
		}

		if (len2 == -5) {
			LOG ( HEADER_PARSER_LOG,NOTICE_LEVEL,sock->socket,"%s","Websocket handshake Error" );
			sock->closeSocket = 1;
			return -1;
		}
			



	}
	return -1;
}

static int checkConnectionKeepAlive(socket_info *sock) {

	if (sock->header->Connection != 0) {
		if (strcasecmp(sock->header->Connection, "keep-alive") != 0) {
			return -1;
		} else {
			return 1;
		}
	}

	if (sock->header->isHttp1_1 == 1) {
		return 1;
	}
	return -1;
}

/*
 *	Return Werte
 *
 *		-1 = Socket Fehler , Verbindung beenden
 *		0  = Header nicht zuende es müssen noch Daten gelesen werden
 *		1  = Request wurde komplett gelesen und Output generiert
 *		2  = Websocket handling , Events wurden eingetragen
 *
 */

static int handleClient(socket_info* sock) {
	int ret;

#ifdef _WEBSERVER_SOCKET_DEBUG_
	LOG ( CONNECTION_LOG,NOTICE_LEVEL,sock->socket,"Client Data" );
#endif



	ret = handleClientHeaderData(sock);
#if _WEBSERVER_CONNECTION_DEBUG_ > 4
	LOG ( CONNECTION_LOG,NOTICE_LEVEL,sock->socket,"handleClientData ret : %d",ret );
#endif

	/* das hier tritt auf wenn der header noch nicht zuende ist */
	if (ret == 0) {
		return 0;
	}
	if (ret == 1) {
#ifdef WEBSERVER_USE_WEBSOCKETS
		if ( sock->header->isWebsocket == 1 )
		{
			#if _WEBSERVER_CONNECTION_DEBUG_ > 1
				LOG ( CONNECTION_LOG,NOTICE_LEVEL,sock->socket,"Websocket Connected" );
			#endif
			initWebsocketStructures(sock);
			sock->isWebsocket = 1;
			startWebsocketConnection ( sock );
			addEventSocketReadWritePersist ( sock );
			return 2;
		}

		/* Websocket Protokol Version wird nicht unterstützt */
		if ( sock->header->isWebsocket == 2 )
		{
			#if _WEBSERVER_CONNECTION_DEBUG_ > 2
				LOG ( CONNECTION_LOG,NOTICE_LEVEL,sock->socket,"isWebsocket 2" );
			#endif
			generateOutputBuffer(sock);
			addEventSocketWritePersist(sock);
			return 2;
		}

#endif

#if _WEBSERVER_HANDLER_DEBUG_ > 4
		LOG ( HANDLER_LOG,NOTICE_LEVEL,sock->socket,"Handle Web Request" );
#endif
		sock->use_output_compression = 0;
		/* das hier tritt auf wenn mehrer requests ueber einen Socket im Burst gesendet werden */
		if (handleWebRequest(sock) < 0) {
			return -1;
		}

		// sonderfall wenn nur \r\n gesendet wird
		if (  sock->header->parsed_bytes != 0 ) {
			generateOutputBuffer(sock);
		}

		WebserverResetHttpRequestHeader(sock->header);
		return 1;

	}

	if (ret == -1) {
		return -1;
	}

	return 0;
}


static char sendData(socket_info* sock, const unsigned char *buffer, const unsigned long buffer_size, FILE_OFFSET *buffer_send_pos) {
	int ret;
	int to_send;
	SOCKET_SEND_STATUS status;

	while( *buffer_send_pos < (FILE_OFFSET)buffer_size ) {
		to_send = buffer_size - *buffer_send_pos;
		if (to_send > WRITE_DATA_SIZE){
			to_send = WRITE_DATA_SIZE;
		}

		status = WebserverSend(sock, &buffer[ *buffer_send_pos ], to_send, 0, &ret);
#if _WEBSERVER_CONNECTION_DEBUG_ > 4
		LOG ( CONNECTION_LOG,ERROR_LEVEL,sock->socket,"%d of %d %d",ret,to_send,status );
#endif
		switch (status) {
		case SOCKET_SEND_NO_MORE_DATA:
			*buffer_send_pos += ret;
			break;

		case SOCKET_SEND_CLIENT_DISCONNECTED:
			return CLIENT_DICONNECTED;

		case SOCKET_SEND_SEND_BUFFER_FULL:
			*buffer_send_pos += ret;
			return DATA_PENDING;

		case SOCKET_SEND_SSL_ERROR:
			return CLIENT_DICONNECTED;

		case SSL_PROTOCOL_ERROR:
			return CLIENT_DICONNECTED;

		default:
			LOG(CONNECTION_LOG, ERROR_LEVEL, sock->socket, "send error not handled %d ( %d )", sock->socket, status);
			return CLIENT_DICONNECTED;
		}
	}

	return CLIENT_NO_MORE_DATA;
}


static CLIENT_WRITE_DATA_STATUS handleClientWriteDataSendRamFile(socket_info* sock, socket_file_infos* file) {
	int ret;
	ret = sendData(sock, file->file_info->Data, file->file_info->DataLenght,&file->file_send_pos);
	switch (ret) {
	case CLIENT_NO_MORE_DATA:
		break;
	case DATA_PENDING:
		return DATA_PENDING;
	case CLIENT_DICONNECTED:
		file->file_send_pos = 0;
		file->file_info = 0;
		return CLIENT_DICONNECTED;
	default:
		file->file_send_pos = 0;
		file->file_info = 0;
		LOG(CONNECTION_LOG, ERROR_LEVEL, sock->socket, "unhandled send status file ram pos : %"FILE_OFF_PRINT_INT" status : %d",
				FILE_OFF_CAST(file->file_send_pos), ret);
		return CLIENT_DICONNECTED;
	}
	file->file_send_pos = 0;
	file->file_info = 0;
	return NO_MORE_DATA;
}



#ifdef LINUX
static CLIENT_WRITE_DATA_STATUS handleClientWriteDataSendFileSystem_sendfile(socket_info* sock, socket_file_infos* file) {
	FILE_OFFSET offset;
	int fd, diff, send;

	offset = file->file_send_pos;

	//printf("handleClientWriteDataSendFileSystem_sendfile : %s\n",sock->file_infos.file_info->Url);

	// TODO(lordrasmus) : an fs anpassen

	fd = PlatformOpenDataReadStream( file->file_info->FilePath);
	diff = file->file_info->DataLenght - file->file_send_pos;

	//printf("handleClientWriteDataSendFileSystem_sendfile: Diff %d DataLenght %d\n", diff, sock->file_infos.file_info->DataLenght );

	send = sendfile(sock->socket, fd, &offset, diff);

	PlatformCloseDataStream();

	if (send > 0) {
		file->file_send_pos += send;

		if ( file->file_info->DataLenght > file->file_send_pos ){
			 return DATA_PENDING;
		}

		file->file_send_pos = 0;
		file->file_info = 0;
		return NO_MORE_DATA;
	}

	if (send == -1) {
		switch (errno) {
		/* Nonblocking I/O has been selected using O_NONBLOCK and the write would block. */
		case EAGAIN:
			return DATA_PENDING;
			/* The input file was not opened for reading or the output file was not opened for writing. */
		case EBADF:
			return CLIENT_DICONNECTED;
			/* Bad address. */
		case EFAULT:
			return CLIENT_DICONNECTED;
			/* Descriptor is not valid or locked, or an mmap(2)-like operation is not available for in_fd. */
		case EINVAL:
			return CLIENT_DICONNECTED;
			/* Unspecified error while reading from in_fd. */
		case EIO:
			return CLIENT_DICONNECTED;
			/* Insufficient memory to read from in_fd. */
		case ENOMEM:
			return CLIENT_DICONNECTED;
		}
	}

	return CLIENT_DICONNECTED;
}
#endif

static CLIENT_WRITE_DATA_STATUS handleClientWriteDataNotCached(socket_info* sock, socket_file_infos* file) {
#ifdef WEBSERVER_USE_SSL
	if (sock->use_ssl == 1) {
		return handleClientWriteDataNotCachedReadWrite(sock, file);
	} else {
#ifdef LINUX
		return handleClientWriteDataSendFileSystem_sendfile(sock, file);
#else
		return handleClientWriteDataNotCachedReadWrite(sock, file);
#endif
	}
#else

#ifdef LINUX
	return handleClientWriteDataSendFileSystem_sendfile(sock, file);
#else
	return handleClientWriteDataNotCachedReadWrite(sock, file);
#endif

#endif
}


static CLIENT_WRITE_DATA_STATUS handleClientWriteDataSendOutputBuffer(socket_info* sock, output_struct *output) {
	int ret;

#ifdef _WEBSERVER_CONNECTION_SEND_DEBUG_
	LOG ( CONNECTION_LOG,ERROR_LEVEL,sock->socket,"handleClientWriteData Send Output Buffer" );
#endif


	if ( output->header.buffer != 0 ){
		ret = sendData( sock, (unsigned char*)output->header.buffer, output->header.buffer_size, &output->header.buffer_send_pos );
		switch (ret) {
			case CLIENT_NO_MORE_DATA:
				WebserverFree( output->header.buffer );
				output->header.buffer = 0;
				output->header.buffer_send_pos = 0;
				break;
			case DATA_PENDING:
				return DATA_PENDING;
			case CLIENT_DICONNECTED:
				goto client_diconnected_header;
			default:
				LOG(CONNECTION_LOG, ERROR_LEVEL, sock->socket, "unhandled send status output_header_buffer pos : %"FILE_OFF_PRINT_INT" status : %d",
						FILE_OFF_CAST(output->header.buffer_send_pos), ret);
				goto client_diconnected_header;
		}
	}
	
	if ( output->body.buffer != 0 ){
		ret = sendData( sock, (unsigned char*)output->body.buffer, output->body.buffer_size, &output->body.buffer_send_pos );
		switch (ret) {
			case CLIENT_NO_MORE_DATA:
				WebserverFree( output->body.buffer );
				output->body.buffer = 0;
				output->body.buffer_send_pos = 0;
				break;
			case DATA_PENDING:
				return DATA_PENDING;
			case CLIENT_DICONNECTED:
				goto client_diconnected_main;
			default:
				LOG(CONNECTION_LOG, ERROR_LEVEL, sock->socket, "unhandled send status output_main_buffer pos : %"FILE_OFF_PRINT_INT" status : %d",
						FILE_OFF_CAST(output->body.buffer_send_pos), ret);
				goto client_diconnected_main;
		}
	}

	if (output->file_infos.file_info != 0) {

		#ifdef _WEBSERVER_CONNECTION_SEND_DEBUG_
			LOG ( CONNECTION_LOG,ERROR_LEVEL,sock->socket,"handleClientWriteData Send File  Bytes %ld Pos %ld", (long)output->file_infos.file_info->DataLenght,(long)output->file_infos.file_send_pos );
		#endif

		if (output->file_infos.file_info->RamCached == 1) {
			return handleClientWriteDataSendRamFile( sock , &output->file_infos);
		} else {

			return handleClientWriteDataNotCached(sock, &output->file_infos);

		}
	}


	return NO_MORE_DATA;

client_diconnected_header:
	if ( output->header.buffer != 0 ){
		WebserverFree( output->header.buffer );
		output->header.buffer = 0;
	}
	output->header.buffer_send_pos = 0;

client_diconnected_main:
	if ( output->body.buffer != 0 ){
		WebserverFree( output->body.buffer );
		output->body.buffer = 0;
	}
	output->body.buffer_send_pos = 0;


	return CLIENT_DICONNECTED;
}

CLIENT_WRITE_DATA_STATUS handleClientWriteData(socket_info* sock) {
	CLIENT_WRITE_DATA_STATUS status_ret;

	while ( ws_list_size( &sock->output_list ) > 0 )
	{
		output_struct *output = (output_struct*)ws_list_get_at( &sock->output_list, 0 );

		if( output != 0 ){
			status_ret = handleClientWriteDataSendOutputBuffer(sock, output);
			if (status_ret != NO_MORE_DATA){
				return status_ret;
			}
		}

		ws_list_delete( &sock->output_list, output );
		WebserverFree( output );

	}

	return NO_MORE_DATA;


}



void handleServer(socket_info* sock) {
	int c;
	unsigned int port;
	socket_info *client_sock;

	while (1) {
		c = PlatformAccept(sock, &port);
		if (c == -1){
			return;
		}

#ifdef WEBSERVER_USE_WNFS
		if (sock->server == 2) {
			if ( wnfs_socket != 0 ){
				PlatformClose( wnfs_socket );
			}
			wnfs_socket = c;

			LOG ( CONNECTION_LOG,NOTICE_LEVEL,c,"WNFS Connection from %s",sock->client_ip_str );

			/*client_sock = addClientSocket(c, 0);
			strncpy(client_sock->client_ip_str, sock->client_ip_str, INET_ADDRSTRLEN);
			client_sock->port = port;*/
			continue;
		}
#endif

#ifdef WEBSERVER_USE_SSL
		if (sock->use_ssl == 1) {

#if _WEBSERVER_CONNECTION_DEBUG_ > 1
			LOG ( CONNECTION_LOG,NOTICE_LEVEL,c,"SSL Connection from %s",sock->client_ip_str );
#endif
			PlatformSetNonBlocking(c);
			client_sock = addClientSocket(c, 1);
			client_sock->run_ssl_accept = 1;
			WebserverSSLInit(client_sock);


		} else {
			PlatformSetNonBlocking(c);
#if _WEBSERVER_CONNECTION_DEBUG_ > 1
			LOG ( CONNECTION_LOG,NOTICE_LEVEL,c,"Normal Connection from %s",sock->client_ip_str );
#endif
			client_sock = addClientSocket(c, 0);
		}
#else

		PlatformSetNonBlocking(c);
#if _WEBSERVER_CONNECTION_DEBUG_ > 1
		LOG(CONNECTION_LOG, NOTICE_LEVEL, c, "Normal Connection from %s", sock->client_ip_str);
#endif
		client_sock = addClientSocket(c, 0);

#endif


		snprintf(client_sock->client_ip_str, INET_ADDRSTRLEN, "%s", sock->client_ip_str);
		client_sock->port = port;

#ifdef WEBSERVER_USE_IPV6
		client_sock->v6_client = sock->v6_client;
#endif
	}

}

int WebserverCloseSocket(socket_info* sock) {

#ifdef WEBSERVER_USE_WEBSOCKETS
	if ( sock->isWebsocket == 1 ) {
#if _WEBSERVER_CONNECTION_DEBUG_ > 1
		LOG( CONNECTION_LOG, NOTICE_LEVEL, sock->socket, "Closing Websocket Connection" );
#endif
	}else{
#endif
#if _WEBSERVER_CONNECTION_DEBUG_ > 1
		LOG( CONNECTION_LOG, NOTICE_LEVEL, sock->socket, "Closing Client Connection" );
#endif
#ifdef WEBSERVER_USE_WEBSOCKETS
	}
#endif

#ifdef WEBSERVER_USE_SSL
	if (sock->use_ssl == 1) {
		WebserverSSLCloseSockets(sock);
		return PlatformCloseSocket(sock->socket);
	} else {
		return PlatformCloseSocket(sock->socket);
	}
#else
	return PlatformCloseSocket(sock->socket);
#endif
}

CLIENT_WRITE_DATA_STATUS handleClientWriteDataNotCachedReadWrite(socket_info* sock, socket_file_infos* file) {
	int ret;
	CLIENT_WRITE_DATA_STATUS ret_v;
	SOCKET_SEND_STATUS status;
	uint32_t to_read, diff;
	unsigned char *buffer;

#ifdef _WEBSERVER_FILESYSTEM_DEBUG_
		LOG ( CONNECTION_LOG,NOTICE_LEVEL,sock->socket,"Send File from Disk Soll %ld Pos %ld",file->file_info->DataLenght,file->file_send_pos );
#endif

	buffer = (unsigned char *) WebserverMalloc( WRITE_DATA_SIZE );
	PlatformOpenDataReadStream( file->file_info->FilePath);
	PlatformSeekToPosition( file->file_send_pos);

	while (1) {
		diff = file->file_info->DataLenght - file->file_send_pos;
		if (diff > WRITE_DATA_SIZE){
			to_read = WRITE_DATA_SIZE;
		}else{
			to_read = diff;
		}


		FILE_OFFSET ret2 = PlatformReadBytes(buffer, to_read);
		if ( ret2 != to_read ){
			printf("Error: read mismatch %"FILE_OFF_PRINT_INT" != %d\n",FILE_OFF_CAST(ret2),to_read);
		}

		status = WebserverSend(sock, buffer, to_read, 0, &ret);

		switch (status) {
		case SOCKET_SEND_NO_MORE_DATA:
#if _WEBSERVER_CONNECTION_DEBUG_ >= 5
			LOG ( SOCKET_LOG,NOTICE_LEVEL,sock->socket,"SOCKET_SEND_NO_MORE_DATA : %ld status : %d ret : %d",file->file_send_pos,status,ret );
#endif
			file->file_send_pos += ret;
			if ( file->file_info->DataLenght == file->file_send_pos) {
				ret_v = NO_MORE_DATA;
				goto send_ende;
			}

			break;

		case SOCKET_SEND_CLIENT_DISCONNECTED:
			ret_v = CLIENT_DICONNECTED;
			goto send_ende;

		case SOCKET_SEND_SEND_BUFFER_FULL:
			file->file_send_pos += ret;
			PlatformCloseDataStream();
			WebserverFree(buffer);
			return DATA_PENDING;

		default:
			LOG(SOCKET_LOG, ERROR_LEVEL, sock->socket, "unhandled send status file file pos : %"FILE_OFF_PRINT_INT" status : %d",
					FILE_OFF_CAST(file->file_send_pos), status);
			ret_v = CLIENT_DICONNECTED;
			goto send_ende;
		}

		if ( file->file_info->DataLenght == file->file_send_pos) {
			ret_v = NO_MORE_DATA;
			break;
		}
	}

	send_ende:
		file->file_info = 0;
	PlatformCloseDataStream();
	WebserverFree(buffer);
	return ret_v;
}



void WebserverConnectionManagerStartLoop(void) {

	_set_main_thread();

	initSocketContainer();

	while (1) {

		if (WebserverStartConnectionManager() < 0) {
			LOG(CONNECTION_LOG, ERROR_LEVEL, 0, "%s","Error : ConnectionManager konnte nicht gestartet werden");
			return;
		}

		waitEvents();
		return;
	}

}

void handleer( int a, short b, void *t ) {
	int ret;
	socket_info* sock = (socket_info*) t;
	CLIENT_WRITE_DATA_STATUS status_ret = UNDEFINED;


#if _WEBSERVER_HANDLER_DEBUG_ > 3

	if ( b == EVENT_TIMEOUT ){ LOG ( HANDLER_LOG,NOTICE_LEVEL,sock->socket,"EV_TIMEOUT" ); }
	if ( b == EVENT_READ ) {   LOG ( HANDLER_LOG,NOTICE_LEVEL,sock->socket,"EV_READ" );    }
	if ( b == EVENT_WRITE ) {  LOG ( HANDLER_LOG,NOTICE_LEVEL,sock->socket,"EV_WRITE" );   }

#endif

	if ( b == EVENT_SIGNAL ) {
		LOG( MESSAGE_LOG, NOTICE_LEVEL,sock->socket,"%s","EV_SIGNAL" );
		LOG( MESSAGE_LOG, ERROR_LEVEL, sock->socket,"EV_SIGNAL not handled: %d", b );
		return;
	}

	if ( a != sock->socket ){
		LOG( MESSAGE_LOG, ERROR_LEVEL, sock->socket,"a != sock->socket : %d", b );
		return;
	}

	if ( sock->extern_handle != 0 )
	{
		sock->extern_handle(sock->socket, sock->extern_handle_data_ptr);
		return;
	}

#ifdef WEBSERVER_USE_WEBSOCKETS
	if ( sock->isWebsocket == 1 )
	{
		websocket_event_handler(sock,(EVENT_TYPES)b);
		return;
	}
#endif

	if (sock->closeSocket == 1) {
		WebserverConnectionManagerCloseRequest(sock);
		return;
	}

	if (sock->server == 1) {
		handleServer(sock);
		return;
	}

	if (sock->server == 2) {
		handleServer(sock);
		return;
	}

	if (sock->client == 1) {
		if (b == EVENT_READ) {

			#ifdef WEBSERVER_USE_SSL

			sock->ssl_pending = 0;
			if(sock->use_ssl == 1){

				if (sock->run_ssl_accept == 1) {
					ret = WebserverSSLAccept(sock);
					if (ret == CLIENT_NO_MORE_DATA) {
						addEventSocketRead(sock);
						return;
					}
					if (ret == SSL_ACCEPT_OK) {
						// Wenn das SSL Accept OK ist das normale HTTP Header handling starten
					}
					if (ret == SSL_PROTOCOL_ERROR) {
						WebserverConnectionManagerCloseRequest(sock);
						return;
					}
					if (ret == NO_SSL_CONNECTION_ERROR) {
						WebserverConnectionManagerCloseRequest(sock);
						return;
					}
				}

			}
			#endif



			ret = handleClient(sock);
			if (ret < 0) {
				WebserverConnectionManagerCloseRequest(sock);
				return;
			}
			// TODO Websocket handling testen
			if ( ret == 2 ){ // Websocket wurde behandelt
				return;
			}

			if( ( ret == 0 ) && ( WebserverSSLPending ( sock ) == 0 ) ){  // Header unvollständig weitere daten lesen
				addEventSocketRead( sock );
				return;
			}

			while(( sock->header_buffer_pos > 0) || ( WebserverSSLPending ( sock ) == 1 ) ){

				ret = handleClient(sock);
				if (ret < 0) {
					WebserverConnectionManagerCloseRequest(sock);
					return;
				}
				// Header nicht zuende , weitere daten lesen
				if( ret == 0 ){
					break;
				}
			}

			// Wenn Output Daten generiert wurden diese erstmal schreiben, nach dem schreiben dann wieder input daten lesen
			if ( ws_list_size( &sock->output_list ) > 0 ){
				addEventSocketWritePersist(sock);
			}else{
				addEventSocketRead( sock );
			}

			return;

		}
		if (b == EVENT_WRITE) {
			status_ret = handleClientWriteData(sock);
			switch (status_ret) {
			case NO_MORE_DATA:
#if _WEBSERVER_CONNECTION_DEBUG_ > 4
				LOG ( CONNECTION_LOG,NOTICE_LEVEL,sock->socket,"request finished" );
#endif
				delEventSocketWritePersist(sock);

				// TODO Keep Alive scheint nicht korrekt zu funktionieren
				if (checkConnectionKeepAlive(sock) == 1) {
					// TODO prüfen ob das noch gebraucht wird
					if (sock->header_buffer_pos > 0) {
						if (sock->closeSocket == 1) {
							WebserverConnectionManagerCloseRequest(sock);
							return;
						} else {
							addEventSocketRead(sock);
							return;
						}
					} else {
						if (sock->closeSocket == 1) {
							WebserverConnectionManagerCloseRequest(sock);
							return;
						}
						addEventSocketRead(sock);
						return;
					}
				}

				#ifdef WEBSERVER_USE_WEBSOCKETS
				if( sock->header->isWebsocket == 2 ){
					addEventSocketRead(sock);
					return;
				}
				#endif

				WebserverConnectionManagerCloseRequest(sock);
				return;

			case DATA_PENDING:
				break;
			case CLIENT_DICONNECTED:
#ifdef _WEBSERVER_CONNECTION_DEBUG_
				LOG ( CONNECTION_LOG,WARNING_LEVEL,sock->socket,"%s","request finished client disconnected" );
#endif
				delEventSocketWritePersist(sock);
				WebserverConnectionManagerCloseRequest(sock);
				return;
			case UNDEFINED:
				LOG(CONNECTION_LOG, ERROR_LEVEL, sock->socket, "%s","UNDEFINED STATUS");
				return;
			}
		}
		return;
	}
}

uint32_t getSocketInfoSize(socket_info* sock) {
	uint32_t ret = sizeof(socket_info);

	if (sock->header_buffer != 0){
		ret += WEBSERVER_MAX_HEADER_LINE_LENGHT + 1; /* header_buffer */
	}
	if (sock->header != 0){
		ret += sizeof(HttpRequestHeader); /* header */
	}
#ifdef WEBSERVER_USE_WEBSOCKETS
	if(sock->websocket_buffer != 0){
		ret+= WebserverMallocedSize(sock->websocket_buffer);
	}

	if(sock->websocket_guid != 0){
		ret+= WEBSERVER_GUID_LENGTH+1;
	}
#endif

	ws_list_iterator_start( &sock->output_list );
	while( ws_list_iterator_hasnext ( &sock->output_list ) ){
		output_struct* out = (output_struct*) ws_list_iterator_next( &sock->output_list );
		ret += out->header.buffer_size;
		ret += out->body.buffer_size;
	}

	return ret;
}

