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


#ifdef WEBSERVER_USE_BINARY_FORMAT

static uint64_t read_uint64( const unsigned char* data, uint64_t* offset_p ){

	uint64_t val;

	int offset = *offset_p;

	val  = ((uint64_t)data[offset++])<<0;
	val += ((uint64_t)data[offset++])<<8;
	val += ((uint64_t)data[offset++])<<16;
	val += ((uint64_t)data[offset++])<<24;
	val += ((uint64_t)data[offset++])<<32;
	val += ((uint64_t)data[offset++])<<40;
	val += ((uint64_t)data[offset++])<<48;
	val += ((uint64_t)data[offset++])<<56;
	*offset_p = offset;

	return val;

}

static uint32_t read_uint32( const unsigned char* data, uint64_t* offset_p ){

	uint32_t val;

	int offset = *offset_p;

	val  = ((uint32_t)data[offset++])<<0;
	val += ((uint32_t)data[offset++])<<8;
	val += ((uint32_t)data[offset++])<<16;
	val += ((uint32_t)data[offset++])<<24;

	*offset_p = offset;

	return val;
}

static const char* read_string( const unsigned char* data, uint64_t* offset_p, uint32_t* length){

	const unsigned char* ret;

	*length = read_uint32( data, offset_p );

	ret = &data[*offset_p];

	*offset_p += *length + 1;

	return (const char*) ret;
}


static void set_file_url( WebserverFileInfo *file, const char *alias, const char *name){

	char tmp[512];
	snprintf(tmp,512,"%s%s",alias,name);
	tmp[511] = '\0';

	char* last = &tmp[strlen(tmp)-1];

	char* p = strstr( tmp, "//" );
	while ( p != 0 ){
		memmove( p , &p[1], strlen( &p[1] ));
		*last = '\0';
		last--;
		p = strstr( tmp, "//" );
	}

	copyURL(file, tmp);
}


static const unsigned char* read_file( const char *alias, const unsigned char* data, uint64_t* offset_p, uint32_t* compressed_p ){

	const char *name;
	const char *etag;
	const unsigned char *ret;
	uint32_t size = 0;
	uint32_t real_size,compresed_size;
	uint32_t template;

	WebserverFileInfo *file = 0;

	file = (WebserverFileInfo*) WebserverMalloc ( sizeof ( WebserverFileInfo ) );
	memset(file, 0, sizeof(WebserverFileInfo));

	*compressed_p = read_uint32( data, offset_p );

	name = read_string( data, offset_p, &size );

	etag = read_string( data, offset_p, &size );

	template = read_uint32( data, offset_p );

	// TODO(lordrasmus) lastmodified noch einbauen
	//file->lastmodified = etag;
	//file->lastmodifiedLength = strlen ( etag );


#ifndef WEBSERVER_DISABLE_CACHE
	file->etag = etag;
	file->etagLength = strlen ( (char*)etag );
#endif

	file->fs_type = FS_BINARY;
	file->RamCached = 1;
	file->FilePrefix = alias;

	
	copyFilePath( file, name);
	
	set_file_url( file, alias, name );

	setFileType( file );

	if ( *compressed_p > 0 ){

		real_size = read_uint32( data, offset_p );
		compresed_size = read_uint32( data, offset_p );

		ret = &data[*offset_p];

		file->CompressedData = ret;
		file->CompressedDataLenght = compresed_size;
		file->RealDataLenght = real_size;

		*offset_p += compresed_size;

		file->DataLenght = compresed_size;

		//printf("   Name : %s ( real %" PRIu32 " / comp %" PRIu32 " ) \n",name,real_size,compresed_size);


		if ( template ){

			unsigned long decomp_size = real_size + 32;

			unsigned char* decomp_buffer = WebserverMalloc( decomp_size );

			file->TemplateFile = 1;

			size_t new_size = 0;

			switch ( *compressed_p ){

				case 2: 	// deflate compression

					new_size = tinfl_decompress_mem_to_mem( decomp_buffer, decomp_size, ret, compresed_size, 0 );

					break;
			}

			if ( new_size != real_size ){
				printf("Decomp Size mismatch %zu != %d\n",new_size, real_size );
				exit(1);
			}

			ret = decomp_buffer;
			file->DataLenght = real_size;
			*compressed_p = 0;
		}
		

	}else{

		real_size = read_uint32( data, offset_p );

		//printf("   Name : %s ( %" PRIu32 " ) \n",name,real_size);

		ret = &data[*offset_p];

		file->DataLenght = real_size;

		*offset_p += real_size + 1;

		if ( template ){
			file->TemplateFile = 1;
		}
	}



	file->Compressed = *compressed_p;
	file->Data = ret;

#ifdef _WEBSERVER_FILESYSTEM_CACHE_DEBUG_
	LOG (FILESYSTEM_LOG,NOTICE_LEVEL,0, "Add File Info Node" );
#endif

	addFileToCache(file);

	return ret;

}


void read_binary_data( const unsigned char* data ){
	uint64_t offset = 0;
	uint32_t size = 0;
	const char *alias;
	uint64_t time;
	uint32_t files;
	uint32_t compressed;


	alias = read_string( data, &offset, &size );
	time = read_uint64( data, &offset );
	files = read_uint32( data, &offset );

	LOG(FILESYSTEM_LOG,NOTICE_LEVEL,0,"%s","loading binary data");
	LOG(FILESYSTEM_LOG,NOTICE_LEVEL,0,"Alias : %s",alias);
	LOG(FILESYSTEM_LOG,NOTICE_LEVEL,0,"Time  : %" PRIu64 "",time);
	LOG(FILESYSTEM_LOG,NOTICE_LEVEL,0,"Files : %" PRIu32 "",files);

	for ( uint32_t i = 0 ; i < files ; i++ ){
		read_file( alias, data , &offset , &compressed);

	}

}

#endif
