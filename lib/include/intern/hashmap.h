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
 * @file hashmap.h
 * @brief Dynamic hash map with string keys
 *
 * A hash table implementation optimized for string key lookups.
 * Provides O(1) average case for get/put/remove operations.
 * Automatically resizes when load factor exceeds threshold.
 *
 * Enable _WEBSERVER_HASHMAP_DEBUG_ for debug logging.
 */

#ifndef _HASHMAP_H_
#define _HASHMAP_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque type for the hash map */
typedef struct hashmap hashmap_t;

/* Available hash functions */
typedef enum {
    HASHMAP_HASH_FNV1A,     /* Default - fast, good distribution */
    HASHMAP_HASH_JENKINS    /* Jenkins One-at-a-time - better distribution */
} hashmap_hash_type;

/* Callback type for foreach iteration */
typedef void (*hashmap_foreach_fn)(const char* key, void* value, void* user_data);

/* Callback type for freeing values */
typedef void (*hashmap_free_fn)(void* value);

/**
 * Create a new hash map with default hash function (FNV-1a)
 * @return New hash map or NULL on allocation failure
 */
hashmap_t* hashmap_create(void);

/**
 * Create a new hash map with specified hash function
 * @param hash_type The hash function to use
 * @return New hash map or NULL on allocation failure
 */
hashmap_t* hashmap_create_ex(hashmap_hash_type hash_type);

/**
 * Destroy a hash map and optionally free all values
 * @param map The hash map to destroy
 * @param free_value Optional callback to free each value (can be NULL)
 */
void hashmap_destroy(hashmap_t* map, hashmap_free_fn free_value);

/**
 * Get a value by key
 * @param map The hash map
 * @param key The key to look up (must not be NULL)
 * @return The value or NULL if not found
 */
void* hashmap_get(hashmap_t* map, const char* key);

/**
 * Insert or update a key-value pair
 * @param map The hash map
 * @param key The key (will be copied)
 * @param value The value to store
 * @return 0 on success, -1 on allocation failure
 * @note If key already exists, the old value is replaced (not freed)
 */
int hashmap_put(hashmap_t* map, const char* key, void* value);

/**
 * Remove a key-value pair
 * @param map The hash map
 * @param key The key to remove
 * @return The removed value or NULL if not found
 * @note The caller is responsible for freeing the returned value
 */
void* hashmap_remove(hashmap_t* map, const char* key);

/**
 * Get the number of entries in the map
 * @param map The hash map
 * @return Number of key-value pairs
 */
size_t hashmap_count(hashmap_t* map);

/**
 * Iterate over all entries in the map
 * @param map The hash map
 * @param callback Function called for each entry
 * @param user_data User data passed to callback
 * @note Iteration order is undefined
 */
void hashmap_foreach(hashmap_t* map, hashmap_foreach_fn callback, void* user_data);

/**
 * Iterate over all entries in sorted key order (for debugging)
 * @param map The hash map
 * @param callback Function called for each entry in sorted order
 * @param user_data User data passed to callback
 * @note This is slower than hashmap_foreach - use only for debugging
 */
void hashmap_foreach_sorted(hashmap_t* map, hashmap_foreach_fn callback, void* user_data);

#ifdef __cplusplus
}
#endif

#endif /* _HASHMAP_H_ */
