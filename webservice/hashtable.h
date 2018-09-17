#ifndef _TIGERA_HASHTABLE__H__
#define _TIGERA_HASHTABLE__H__

#include "includes.h"

/* Synchronized chained Hash Table implementation.
 * Collision resolution by separate chaining with linked lists
 * and synchronizing access with rwlock (assuming much more reads
 * than writes would take place).
 */

typedef uint32_t hash_t;

typedef int (*hashtable_cmp_t)(const void *value, const void *key);
typedef hash_t (*hashtable_hash_t)(const void *key);
typedef void (*hashtable_free_t)(void *value);

typedef struct hashtable hashtable_t;

typedef enum hashtable_error hashtable_error_t;

enum hashtable_error {
     HASHTABLE_E_SUCCESS /**< operation was successfull */
    ,HASHTABLE_E_FOUND   /**< value already saved in hashtable */
    ,HASHTABLE_E_ERROR   /**< operation was unsuccessfull */
};

hashtable_t *
hashtable_new(hashtable_cmp_t cmp, hashtable_hash_t hash, hashtable_free_t value_free, size_t nr_buckets);

void
hashtable_free(hashtable_t *table);

/**
 * Tries to add value to the hashtable.
 *
 * Element is not added if it already exists.
 *
 * @param hashtable
 * @param value 
 * @param key
 * @return hashtable error @see hastable_error_t
 *         HASHTABLE_E_SUCCESS: value successfully added
 *         HASHTABLE_E_FOUND: value was not added, already existed
 *         HASHTABLE_E_ERROR: value was not added, internal error (e.g. run out of memory) 
 */
hashtable_error_t 
hashtable_add(hashtable_t *hashtable, void *value, const void *key);

/**
 * Tries to return value corresponding to informed key.
 *
 * If value is not found, NULL is returned.
 *
 * Pointer to value internally is stored is returned under
 * the assumption that this hash table does not allow
 * removals.
 */
void *
hashtable_find(hashtable_t *hashtable, const void *key);

size_t
hashtable_get_collisions(hashtable_t *hashtable);

#endif /* _TIGERA_HASHTABLE__H__ */
