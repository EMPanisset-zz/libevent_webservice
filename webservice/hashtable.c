#include "hashtable.h"
#include "rwlock.h"

/**
 * Lock-free strategies or lock per bucked (as opposed to
 * global hashtable lock) might be used to make this data
 * structure more scalable.
 */

struct hashtable {
    slist_t *buckets;
    size_t size;
    size_t nr_buckets;
    size_t collisions;
    rwlock_t *rwlock; /**< assuming much more reads than writes */
    hashtable_cmp_t cmp;
    hashtable_hash_t hash;
    hashtable_free_t value_free;
};

hashtable_t *
hashtable_new(hashtable_cmp_t cmp, hashtable_hash_t hash, hashtable_free_t value_free, size_t nr_buckets)
{
    hashtable_t *hashtable = calloc(1, sizeof(hashtable_t));

    if (NULL != hashtable) {
        slist_t *buckets = calloc(nr_buckets, sizeof(slist_t));
        if (NULL == buckets) {
            goto error;
        }
        hashtable->buckets = buckets;
        hashtable->cmp = cmp;
        hashtable->hash = hash;
        hashtable->value_free = value_free;
        hashtable->nr_buckets = nr_buckets;
        
        hashtable->rwlock = rwlock_new();
        if (NULL == hashtable->rwlock) {
            goto error;
        }        
        
    }

    return hashtable;

error:
    hashtable_free(hashtable);
    return NULL;
}

void
hashtable_free(hashtable_t *hashtable)
{
    if (NULL != hashtable) {
        slist_t *bucket;
        if (NULL != hashtable->buckets) {
            for (int i = 0; i < hashtable->nr_buckets; ++i) {
                void *value = NULL;
                bucket = &hashtable->buckets[i];
                while (NULL != (value = slist_pop_front(bucket))) {
                    if (NULL != hashtable->value_free) {
                        hashtable->value_free(value);
                    }
                }
            }
            free(hashtable->buckets);
            hashtable->buckets = NULL;
        }
        rwlock_free(hashtable->rwlock);
        hashtable->rwlock = NULL;
        free(hashtable);
    }
}

static slist_t *
_hashtable_get_bucket(hashtable_t *hashtable, const void *key)
{
    slist_t *bucket = NULL;

    hash_t hash = hashtable->hash(key);
    hash = hash % hashtable->nr_buckets;
    bucket = &hashtable->buckets[hash];

    return bucket;
}

static snode_t *
_hashtable_find(hashtable_t *hashtable, slist_t *bucket, const void *key)
{
    snode_t *node = NULL;

    slist_foreach(bucket, node) {
        if (0 == hashtable->cmp(node->data, key)) {
            break;
        }
    }
    return node;
}

hashtable_error_t
hashtable_add(hashtable_t *hashtable, void *value, const void *key)
{
    hashtable_error_t error = HASHTABLE_E_SUCCESS;
    slist_t *bucket = _hashtable_get_bucket(hashtable, key);
    snode_t *node = NULL;

    rwlock_wrlock(hashtable->rwlock);
    if (NULL != slist_head(bucket)) {
        hashtable->collisions++;
    }
    node = _hashtable_find(hashtable, bucket, key);
    if (NULL == node) {
        if (NULL == slist_push_front(bucket, value)) {
            error = HASHTABLE_E_ERROR;
        }
    }
    else {
        error = HASHTABLE_E_FOUND;
    }
    rwlock_unlock(hashtable->rwlock);
    return error;
}

void *
hashtable_find(hashtable_t *hashtable, const void *key)
{
    void *value = NULL;
    slist_t *bucket = _hashtable_get_bucket(hashtable, key);
    snode_t *node = NULL;

    rwlock_rdlock(hashtable->rwlock);
    node = _hashtable_find(hashtable, bucket, key);
    if (NULL != node) {
        /* As no node removal takes place for this hashtable
         * and readers themselves don't change recovered item 
         * we can return pointer to item just found.
         */
        value = node->data;
    }
    rwlock_unlock(hashtable->rwlock);
    return value;
}

size_t
hashtable_get_collisions(hashtable_t *hashtable)
{
    return hashtable->collisions;
}
