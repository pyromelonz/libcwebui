/*

SPDX-License-Identifier: MPL-2.0

 libCWebUI
 Copyright (C) 2007 - 2019  Ramin Seyed-Moussavi

 Projekt URL : https://github.com/lordrasmus/libcwebui

 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at https://mozilla.org/MPL/2.0/. 

*/



#if __GNUC__ > 2

#include "webserver.h"

#include <math.h>

#ifdef DMALLOC
#include <dmalloc/dmalloc.h>
#endif


unsigned long VISIBLE_ATTR allocated = 0 ;
unsigned long allocated_max = 0;

#ifdef _WEBSERVER_MEMORY_DEBUG_
    int VISIBLE_ATTR print_blocks_now = 0;
#endif


/*
#define USE_MALLOC_CACHE
*/

#ifdef _WEBSERVER_MEMORY_DEBUG_
list_t block_list;
#endif

extern list_t chunk_cache;

#ifdef USE_MALLOC_CACHE
list_t malloc_cache[20];
#warning "Auf uClibc gibt es assertions"
#endif

WS_MUTEX mem_mutex;

#if 0
static void *freeChunkCallBack(const void *restrict free_element) {
	html_chunk* chunk = (html_chunk*)free_element;
	if (chunk->text != 0){
		WebserverFree(chunk->text);
	}
	WebserverFree(chunk);
	return 0;
}
#endif

void initMemoryManager(void) {

#ifdef USE_MALLOC_CACHE
	int i;
#endif

	PlatformCreateMutex(&mem_mutex);

#ifdef _WEBSERVER_MEMORY_DEBUG_
	ws_list_init(&block_list);
#endif

#ifdef USE_MALLOC_CACHE
	for (i = 0; i < 20; i++)
	ws_list_init(&malloc_cache[i]);
#endif

	ws_list_init(&chunk_cache);

	/* ws_list_attributes_freer(&chunk_cache,freeChunkCallBack); */
#ifdef ENABLE_DEVEL_WARNINGS	
	#warning "chunk cache elemente richtig freigeben"
#endif
}



void freeMemoryManager(void) {
	PlatformLockMutex(&mem_mutex);

#ifdef USE_MALLOC_CACHE
	for (i = 0; i < 20; i++)
	ws_list_destroy(&malloc_cache[i]);
#endif

	ws_list_destroy(&chunk_cache);
}

#ifdef _WEBSERVER_MEMORY_DEBUG_

typedef struct {
	char* filename;
	unsigned int fileline;
	unsigned int size;
	unsigned int real_size;
	void* real_pointer;
	void* pointer;
}memory_block;



static void addBlock(memory_block* block) {
	ws_list_append(&block_list, block, 0);
}

static void delBlock(void* p_block) {
	memory_block* block = (memory_block*) p_block;
	ws_list_delete(&block_list, block);
}

void printBlocks(void) {
	memory_block* block;
	unsigned long blocks = 0;
	unsigned long size = 0;
	int i = 0;
	char buffer[100];
	FILE* file;

	while(1){
		sprintf(buffer,"/tmp/webserver_memory_%d.txt",i);
		if ( 0 != access(buffer, R_OK)){
			break;
		}
		i++;
	}

	file = fopen(buffer, "w");
	fprintf(file,"Start Speicher Debug\n");

	PlatformLockMutex(&mem_mutex);

	ws_list_iterator_start(&block_list);
	while ((block = (memory_block*) ws_list_iterator_next(&block_list))) {
		blocks++;
		//size += WebserverMallocedRealSize(block->pointer);
		size += block->real_size;
		fprintf(file,"Size : %d File :%s  Line :%d \n", block->size, block->filename, block->fileline);
	}
	ws_list_iterator_stop(&block_list);

	fprintf(file,"Ende Speicher Debug Blocks: %ld Size : %ld ( %ld )\n", blocks,size,allocated);

	PlatformUnlockMutex(&mem_mutex);

	fclose( file );
}

#endif

#ifdef USE_MALLOC_CACHE

#ifdef __GNUC__
#define clzl(x)  __builtin_clzl(x)
#ifdef __LP64__
#define LONG_BITS 63
#pragma message("64Bit")
#endif
#if ULONG_MAX == 0xFFFFFFFFL
#define LONG_BITS 31
#pragma message("32Bit")
#endif
#endif

unsigned int clz(unsigned long size) {
#ifdef _MSC_VER
	int r = 0;
	unsigned long tmp = 1 << r;
	while ( tmp < size ) {
		r++;
		tmp = 1 << r;
	}
	return r;
#endif

#ifdef __GNUC__
	return LONG_BITS - clzl(size);
#endif
}

#define MAX_BLOCK_SIZE_INDEX 15	// max 16384 byte blocks cachen
#define PREE_ALOCATE_BLOCKS 5

unsigned long calc_malloc_index(unsigned long size) {
	unsigned int r;
	unsigned long m;
	r = clz(size);
	m = 1 << r;
	if (m < size) r++;
	m = 1 << r;
	return r;
}

void* get_cached_malloc(unsigned long size) {
	void *ret = 0;
	unsigned long m;
	unsigned long malloc_size = calc_malloc_index(size);
	int i;

	if (malloc_size < MAX_BLOCK_SIZE_INDEX) {
		if (malloc_cache[malloc_size].numels > 0) {
			//m = 1 << malloc_size;
			ret = ws_list_extract_at(&malloc_cache[malloc_size], 0);
		} else {
			m = 1 << malloc_size;
			for(i=0;i<PREE_ALOCATE_BLOCKS;i++) {
				ret = PlatformMalloc(m);
				ws_list_append(&malloc_cache[malloc_size], ret);
			}
			ret = PlatformMalloc(m);
		}
	}
	if (ret == 0) {
		m = 1 << malloc_size;
		ret = PlatformMalloc(m);

	}
	return ret;
}

void insert_cached_malloc(void* mem, unsigned long size) {
	//unsigned long m;
	unsigned int n = calc_malloc_index(size);

	if (n < MAX_BLOCK_SIZE_INDEX) {
		//m = 1 << n;
		ws_list_append(&malloc_cache[n], mem);
	} else {
		PlatformFree(mem);
	}
}

#endif

#pragma GCC visibility push(default)

#ifndef __BIGGEST_ALIGNMENT__
	#define __BIGGEST_ALIGNMENT__ 8
#endif

#if __BIGGEST_ALIGNMENT__ < __SIZEOF_LONG__
	#define MEM_OFFSET __SIZEOF_LONG__
#else
	#define MEM_OFFSET __BIGGEST_ALIGNMENT__
#endif

#ifdef _WEBSERVER_MEMORY_DEBUG_
void* real_WebserverMalloc(const unsigned long size, const char* function, const char* filename, int fileline ) {
#else
void* real_WebserverMalloc(const unsigned long size ) {
#endif
	unsigned long *s;
	unsigned int add_size = 0, real_alloc;
	char* ret;

#ifdef _WEBSERVER_MEMORY_DEBUG_
	char* ret2;
	memory_block* block;
	add_size += sizeof(memory_block);
	add_size += __BIGGEST_ALIGNMENT__ * 4;
#endif

	add_size += __BIGGEST_ALIGNMENT__ + sizeof(unsigned long);

	real_alloc = size + add_size;

	if ( size != 0 ){
		WS_ASSERT( real_alloc == add_size)
	}

	PlatformLockMutex(&mem_mutex);

#ifdef USE_MALLOC_CACHE
	ret = (char*)get_cached_malloc(real_alloc);
#else
	ret = (char*) PlatformMalloc(real_alloc);
#endif
	if (ret == 0) {
		printf("Memory malloc Error\n");
		exit(1);
	}
#ifdef _WEBSERVER_MEMORY_DEBUG_
	ret2 = ret;
#endif

#if __BIGGEST_ALIGNMENT__ == 16
	s = (unsigned long*) __ws_assume_aligned(ret, 8 );
	*s = real_alloc;
	ret = __ws_assume_aligned(ret + MEM_OFFSET, 8 );
#else
	s = (unsigned long*) __ws_assume_aligned(ret, __BIGGEST_ALIGNMENT__ );
	*s = real_alloc;
	ret = __ws_assume_aligned(ret + MEM_OFFSET, __BIGGEST_ALIGNMENT__ );
#endif

	allocated += real_alloc;
	if (allocated_max < allocated){
		allocated_max = allocated;
	}


#ifdef _WEBSERVER_MEMORY_DEBUG_
	block = (memory_block*) __ws_assume_aligned(ret,__BIGGEST_ALIGNMENT__);
	block->filename = (char*) filename;
	block->fileline = fileline;
	block->size = size;
	block->real_size = real_alloc;
	block->real_pointer = ret2;

	addBlock(block);

	ret += sizeof(memory_block);
	memset(ret , 0xAB, __BIGGEST_ALIGNMENT__ * 2);
	ret += __BIGGEST_ALIGNMENT__ * 2;
	memset(ret + size, 0xAB, __BIGGEST_ALIGNMENT__ * 2);

	block->pointer = ret;
#endif

	PlatformUnlockMutex(&mem_mutex);

	return (void*) ret;
}

void* WebserverRealloc(void *mem, const unsigned long size ) {
	unsigned long* s;
	char* p;
	char* p2 = mem;
	void* ptr = realloc(p2 - MEM_OFFSET,  size  + __BIGGEST_ALIGNMENT__ + sizeof(unsigned long) );
	if ( ptr == 0 ){
		printf("Memory realloc Error\n");
		exit(1);
	}

	p = (char*) ptr;

#if __BIGGEST_ALIGNMENT__ == 16
	s = (unsigned long*) __ws_assume_aligned(p, 8 );
#else
	s = (unsigned long*) __ws_assume_aligned(p, __BIGGEST_ALIGNMENT__ );
#endif

	*s = size + __BIGGEST_ALIGNMENT__ + sizeof(unsigned long);

	p2 = ptr;
#if __BIGGEST_ALIGNMENT__ == 16
	return __ws_assume_aligned( p2 + MEM_OFFSET, 8 );
#else
	return __ws_assume_aligned( p2 + MEM_OFFSET, __BIGGEST_ALIGNMENT__ );
#endif

#ifdef ENABLE_DEVEL_WARNINGS
	#warning "an Memory Debug anpassen"

#endif	
}

static void *get_real_pointer(void* mem){
	char* p;
	if (mem == 0){
		return 0;
	}
	p = (char*) mem;

#ifdef _WEBSERVER_MEMORY_DEBUG_

	p -= __BIGGEST_ALIGNMENT__ * 2;
	p -= sizeof(memory_block);

#endif

	p -= MEM_OFFSET;

	return p;
}

#ifdef _WEBSERVER_MEMORY_DEBUG_

static void *get_block_pointer(void* mem){
	char* p;
	if (mem == 0){
		return 0;
	}
	p = (char*) mem;

	p -= __BIGGEST_ALIGNMENT__ * 2;
	p -= sizeof(memory_block);

	return p;
}

#endif

unsigned long WebserverMallocedSize(void* mem){
	unsigned long* s;
	unsigned long size;
	char* p;

	if (mem == 0){
		return 0;
	}

#ifdef _WEBSERVER_MEMORY_DEBUG_

	memory_block* block = get_block_pointer( mem );

#endif

	p = get_real_pointer( mem );
#if __BIGGEST_ALIGNMENT__ == 16
	s = (unsigned long*) __ws_assume_aligned(p, 8 );
#else
	s = (unsigned long*) __ws_assume_aligned(p, __BIGGEST_ALIGNMENT__ );
#endif

	size = ( *s ) - ( __BIGGEST_ALIGNMENT__ + sizeof(unsigned long) );

#ifdef _WEBSERVER_MEMORY_DEBUG_
	size -= __BIGGEST_ALIGNMENT__ * 4;
	size -= sizeof(memory_block);

	if ( size != block->size){
		printf("Error Verifieng Memory size\n");
	}

#endif

	return size;
}



unsigned long WebserverMallocedRealSize(void* mem){
	unsigned long* s;
	unsigned long size;
	char* p;

	if (mem == 0){
		return 0;
	}

	p = get_real_pointer( mem );
#if __BIGGEST_ALIGNMENT__ == 16
	s = (unsigned long*) __ws_assume_aligned(p, 8 );
#else
	s = (unsigned long*) __ws_assume_aligned(p, __BIGGEST_ALIGNMENT__ );
#endif
	size = ( *s );


	return size;
}

void WebserverFree(void *mem) {
	char* p;
#ifdef _WEBSERVER_MEMORY_DEBUG_
	int  i;
	unsigned char *check;
	memory_block* block;
#endif

	unsigned long size;

	if (mem == 0){
		return;
	}

	PlatformLockMutex(&mem_mutex);

#ifdef _WEBSERVER_MEMORY_DEBUG_

	block = (memory_block*) __ws_assume_aligned(get_block_pointer(mem), __BIGGEST_ALIGNMENT__ );
	delBlock(block);

#endif

	p = get_real_pointer( mem );

	size = WebserverMallocedRealSize( mem );

#ifdef _WEBSERVER_MEMORY_DEBUG_
	if( size != block->real_size ){
		printf("Memory Size Info Error %ld %d\n",size, block->real_size );
	}

	if ( p != block->real_pointer ){
		printf("Memory Pointer Error 0x%lX 0x%lX\n",(unsigned long)p, (unsigned long)block->pointer );
	}
#endif

	allocated -= size;


#ifdef _WEBSERVER_MEMORY_DEBUG_
	check = ((unsigned char*)block) + sizeof(memory_block);
	for (i = 0; i < __BIGGEST_ALIGNMENT__ * 2 ; i++) {
		if (check[i] != 0xAB) {
			printf("Memory Start Fence Error : Offset %d ",i);
			printf("Size : %d File :%s  Line :%d \n", block->size, block->filename, block->fileline);
			break;
		}
	}

	check = ((unsigned char*)mem) + WebserverMallocedSize(mem);
	for (i = 0; i < __BIGGEST_ALIGNMENT__ * 2 ; i++) {
		if (check[i] != 0xAB) {
			printf("Memory End Fence Error : Offset %d ",i);
			printf("Size : %d File :%s  Line :%d \n", block->size, block->filename, block->fileline);
			break;
		}
	}

#endif

#ifdef USE_MALLOC_CACHE
	insert_cached_malloc((void*) p, *s);
#else
	PlatformFree((void*) p);
#endif


	PlatformUnlockMutex(&mem_mutex);

}

#pragma GCC visibility pop





#endif
