/*

SPDX-License-Identifier: MPL-2.0

 libCWebUI
 Copyright (C) 2007 - 2019  Ramin Seyed-Moussavi

 Projekt URL : https://github.com/lordrasmus/libcwebui


 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at https://mozilla.org/MPL/2.0/.

*/

/**
 * @file hashmap.c
 * @brief Dynamic hash map with string keys
 */

#include "webserver.h"
#include <stdlib.h>  /* qsort */

/* Initial bucket count (must be power of 2) */
#define HASHMAP_INITIAL_CAPACITY 16

/* Load factor threshold for resize (75%) */
#define HASHMAP_LOAD_FACTOR_NUM 3
#define HASHMAP_LOAD_FACTOR_DEN 4

/* Maximum key length */
#define HASHMAP_MAX_KEY_LEN 4096

/* Entry in the hash map */
typedef struct hashmap_entry {
	char* key;
	void* value;
	struct hashmap_entry* next;  /* Chain for collision handling */
} hashmap_entry_t;

/* Hash map structure */
struct hashmap {
	hashmap_entry_t** buckets;  /* Array of bucket pointers */
	uint32_t capacity;          /* Number of buckets (always power of 2) */
	uint32_t count;             /* Number of entries */
	hashmap_hash_type hash_type; /* Hash function to use */
};

/**
 * FNV-1a hash function - fast and good distribution for strings
 */
static uint32_t hash_fnv1a(const char* str, uint32_t max_len)
{
	uint32_t hash = 2166136261U;  /* FNV offset basis */
	uint32_t i;

	for (i = 0; i < max_len && str[i] != '\0'; i++) {
		hash ^= (uint8_t)str[i];
		hash *= 16777619U;  /* FNV prime */
	}

	return hash;
}

/**
 * Jenkins One-at-a-time hash - better distribution than FNV-1a
 */
static uint32_t hash_jenkins(const char* str, uint32_t max_len)
{
	uint32_t hash = 0;
	uint32_t i;

	for (i = 0; i < max_len && str[i] != '\0'; i++) {
		hash += (uint8_t)str[i];
		hash += (hash << 10);
		hash ^= (hash >> 6);
	}

	hash += (hash << 3);
	hash ^= (hash >> 11);
	hash += (hash << 15);

	return hash;
}

/**
 * Calculate hash using the map's configured hash function
 */
static uint32_t hash_string(hashmap_t* map, const char* str, uint32_t max_len)
{
	switch (map->hash_type) {
		case HASHMAP_HASH_JENKINS:
			return hash_jenkins(str, max_len);
		case HASHMAP_HASH_FNV1A:
		default:
			return hash_fnv1a(str, max_len);
	}
}

/**
 * Get bucket index for a hash value
 */
static uint32_t bucket_index(hashmap_t* map, uint32_t hash)
{
	/* Fast modulo for power of 2: hash & (capacity - 1) */
	return hash & (map->capacity - 1);
}

/**
 * Check if resize is needed and perform it
 */
static int hashmap_resize_if_needed(hashmap_t* map)
{
	uint32_t new_capacity;
	hashmap_entry_t** new_buckets;
	uint32_t i;

	/* Check load factor: count/capacity > LOAD_FACTOR_NUM/LOAD_FACTOR_DEN */
	if (map->count * HASHMAP_LOAD_FACTOR_DEN <= map->capacity * HASHMAP_LOAD_FACTOR_NUM) {
		return 0;  /* No resize needed */
	}

	/* Double the capacity */
	new_capacity = map->capacity * 2;
	if (new_capacity < map->capacity) {
		LOG(HASHMAP_LOG, ERROR_LEVEL, 0, "hashmap_resize: capacity overflow");
		return -1;
	}

#ifdef _WEBSERVER_HASHMAP_DEBUG_
	LOG(HASHMAP_LOG, DEBUG_LEVEL, 0, "hashmap_resize: %u -> %u buckets (count=%u)",
		map->capacity, new_capacity, map->count);
#endif

	new_buckets = (hashmap_entry_t**)WebserverMalloc(new_capacity * sizeof(hashmap_entry_t*));
	if (!new_buckets) {
		LOG(HASHMAP_LOG, ERROR_LEVEL, 0, "hashmap_resize: allocation failed for %u buckets",
			new_capacity);
		return -1;
	}
	memset(new_buckets, 0, new_capacity * sizeof(hashmap_entry_t*));

	/* Rehash all entries */
	for (i = 0; i < map->capacity; i++) {
		hashmap_entry_t* entry = map->buckets[i];
		while (entry) {
			hashmap_entry_t* next = entry->next;
			uint32_t hash = hash_string(map, entry->key, HASHMAP_MAX_KEY_LEN);
			uint32_t idx = hash & (new_capacity - 1);

			entry->next = new_buckets[idx];
			new_buckets[idx] = entry;

			entry = next;
		}
	}

	WebserverFree(map->buckets);
	map->buckets = new_buckets;
	map->capacity = new_capacity;

	return 0;
}

hashmap_t* hashmap_create_ex(hashmap_hash_type hash_type)
{
	hashmap_t* map = (hashmap_t*)WebserverMalloc(sizeof(hashmap_t));
	if (!map) {
		LOG(HASHMAP_LOG, ERROR_LEVEL, 0, "hashmap_create: allocation failed");
		return NULL;
	}

	map->buckets = (hashmap_entry_t**)WebserverMalloc(HASHMAP_INITIAL_CAPACITY * sizeof(hashmap_entry_t*));
	if (!map->buckets) {
		LOG(HASHMAP_LOG, ERROR_LEVEL, 0, "hashmap_create: bucket allocation failed");
		WebserverFree(map);
		return NULL;
	}
	memset(map->buckets, 0, HASHMAP_INITIAL_CAPACITY * sizeof(hashmap_entry_t*));

	map->capacity = HASHMAP_INITIAL_CAPACITY;
	map->count = 0;
	map->hash_type = hash_type;

#ifdef _WEBSERVER_HASHMAP_DEBUG_
	LOG(HASHMAP_LOG, DEBUG_LEVEL, 0, "hashmap_create: map=%p capacity=%u hash_type=%d",
		(void*)map, map->capacity, hash_type);
#endif

	return map;
}

hashmap_t* hashmap_create(void)
{
	return hashmap_create_ex(HASHMAP_HASH_FNV1A);
}

void hashmap_destroy(hashmap_t* map, hashmap_free_fn free_value)
{
	uint32_t i;

	if (!map) {
		return;
	}

#ifdef _WEBSERVER_HASHMAP_DEBUG_
	LOG(HASHMAP_LOG, DEBUG_LEVEL, 0, "hashmap_destroy: map=%p count=%u capacity=%u",
		(void*)map, map->count, map->capacity);
#endif

	for (i = 0; i < map->capacity; i++) {
		hashmap_entry_t* entry = map->buckets[i];
		while (entry) {
			hashmap_entry_t* next = entry->next;

			WebserverFree(entry->key);
			if (free_value && entry->value) {
				free_value(entry->value);
			}
			WebserverFree(entry);

			entry = next;
		}
	}

	WebserverFree(map->buckets);
	WebserverFree(map);
}

void* hashmap_get(hashmap_t* map, const char* key)
{
	uint32_t hash;
	uint32_t idx;
	hashmap_entry_t* entry;

	if (!map || !key) {
		return NULL;
	}

	hash = hash_string(map, key, HASHMAP_MAX_KEY_LEN);
	idx = bucket_index(map, hash);

	entry = map->buckets[idx];
	while (entry) {
		if (strncmp(entry->key, key, HASHMAP_MAX_KEY_LEN) == 0) {
#ifdef _WEBSERVER_HASHMAP_DEBUG_
			LOG(HASHMAP_LOG, DEBUG_LEVEL, 0, "hashmap_get: key='%s' -> value=%p",
				key, entry->value);
#endif
			return entry->value;
		}
		entry = entry->next;
	}

#ifdef _WEBSERVER_HASHMAP_DEBUG_
	LOG(HASHMAP_LOG, DEBUG_LEVEL, 0, "hashmap_get: key='%s' -> not found", key);
#endif

	return NULL;
}

int hashmap_put(hashmap_t* map, const char* key, void* value)
{
	uint32_t hash;
	uint32_t idx;
	hashmap_entry_t* entry;
	char* key_copy;

	if (!map || !key) {
		return -1;
	}

	hash = hash_string(map, key, HASHMAP_MAX_KEY_LEN);
	idx = bucket_index(map, hash);

	/* Check if key already exists */
	entry = map->buckets[idx];
	while (entry) {
		if (strncmp(entry->key, key, HASHMAP_MAX_KEY_LEN) == 0) {
#ifdef _WEBSERVER_HASHMAP_DEBUG_
			LOG(HASHMAP_LOG, DEBUG_LEVEL, 0, "hashmap_put: update key='%s' old=%p new=%p",
				key, entry->value, value);
#endif
			entry->value = value;
			return 0;
		}
		entry = entry->next;
	}

	/* Create new entry */
	key_copy = Webserver_strndup(key, HASHMAP_MAX_KEY_LEN);
	if (!key_copy) {
		LOG(HASHMAP_LOG, ERROR_LEVEL, 0, "hashmap_put: key copy failed for '%s'", key);
		return -1;
	}

	entry = (hashmap_entry_t*)WebserverMalloc(sizeof(hashmap_entry_t));
	if (!entry) {
		LOG(HASHMAP_LOG, ERROR_LEVEL, 0, "hashmap_put: entry allocation failed for '%s'", key);
		WebserverFree(key_copy);
		return -1;
	}

	entry->key = key_copy;
	entry->value = value;
	entry->next = map->buckets[idx];
	map->buckets[idx] = entry;
	map->count++;

#ifdef _WEBSERVER_HASHMAP_DEBUG_
	LOG(HASHMAP_LOG, DEBUG_LEVEL, 0, "hashmap_put: insert key='%s' value=%p count=%u",
		key, value, map->count);
#endif

	/* Resize if needed */
	if (hashmap_resize_if_needed(map) < 0) {
		LOG(HASHMAP_LOG, WARNING_LEVEL, 0,
			"hashmap_put: resize failed, continuing with degraded performance (count=%u, capacity=%u)",
			map->count, map->capacity);
	}

	return 0;
}

void* hashmap_remove(hashmap_t* map, const char* key)
{
	uint32_t hash;
	uint32_t idx;
	hashmap_entry_t* entry;
	hashmap_entry_t* prev = NULL;

	if (!map || !key) {
		return NULL;
	}

	hash = hash_string(map, key, HASHMAP_MAX_KEY_LEN);
	idx = bucket_index(map, hash);

	entry = map->buckets[idx];
	while (entry) {
		if (strncmp(entry->key, key, HASHMAP_MAX_KEY_LEN) == 0) {
			void* value = entry->value;

			if (prev) {
				prev->next = entry->next;
			} else {
				map->buckets[idx] = entry->next;
			}

#ifdef _WEBSERVER_HASHMAP_DEBUG_
			LOG(HASHMAP_LOG, DEBUG_LEVEL, 0, "hashmap_remove: key='%s' value=%p count=%u",
				key, value, map->count - 1);
#endif

			WebserverFree(entry->key);
			WebserverFree(entry);
			map->count--;

			return value;
		}
		prev = entry;
		entry = entry->next;
	}

#ifdef _WEBSERVER_HASHMAP_DEBUG_
	LOG(HASHMAP_LOG, DEBUG_LEVEL, 0, "hashmap_remove: key='%s' not found", key);
#endif

	return NULL;
}

size_t hashmap_count(hashmap_t* map)
{
	return map ? map->count : 0;
}

void hashmap_foreach(hashmap_t* map, hashmap_foreach_fn callback, void* user_data)
{
	uint32_t i;

	if (!map || !callback) {
		return;
	}

#ifdef _WEBSERVER_HASHMAP_DEBUG_
	LOG(HASHMAP_LOG, DEBUG_LEVEL, 0, "hashmap_foreach: map=%p count=%u",
		(void*)map, map->count);
#endif

	for (i = 0; i < map->capacity; i++) {
		hashmap_entry_t* entry = map->buckets[i];
		while (entry) {
			callback(entry->key, entry->value, user_data);
			entry = entry->next;
		}
	}
}

/* Comparison function for qsort */
static int compare_keys(const void* a, const void* b)
{
	const char* const* ka = (const char* const*)a;
	const char* const* kb = (const char* const*)b;
	return strcmp(*ka, *kb);
}

void hashmap_foreach_sorted(hashmap_t* map, hashmap_foreach_fn callback, void* user_data)
{
	const char** keys;
	uint32_t i, j;
	hashmap_entry_t* entry;

	if (!map || !callback || map->count == 0) {
		return;
	}

#ifdef _WEBSERVER_HASHMAP_DEBUG_
	LOG(HASHMAP_LOG, DEBUG_LEVEL, 0, "hashmap_foreach_sorted: map=%p count=%u",
		(void*)map, map->count);
#endif

	/* Collect all keys */
	keys = (const char**)WebserverMalloc(map->count * sizeof(char*));
	if (!keys) {
		LOG(HASHMAP_LOG, WARNING_LEVEL, 0,
			"hashmap_foreach_sorted: key array allocation failed, falling back to unsorted");
		hashmap_foreach(map, callback, user_data);
		return;
	}

	j = 0;
	for (i = 0; i < map->capacity; i++) {
		entry = map->buckets[i];
		while (entry) {
			keys[j++] = entry->key;
			entry = entry->next;
		}
	}

	/* Sort keys */
	qsort(keys, map->count, sizeof(char*), compare_keys);

	/* Iterate in sorted order */
	for (i = 0; i < map->count; i++) {
		void* value = hashmap_get(map, keys[i]);
		callback(keys[i], value, user_data);
	}

	WebserverFree(keys);
}
