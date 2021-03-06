#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lcc_map.h"

#define LCC_MAP_INIT_CAP    67
#define LCC_MAP_LOAD_FAC    3 / 4

static inline long _lcc_djb_hash(lcc_string_t *key)
{
    /* 5381 is just a number that, in testing,
     * resulted in fewer collisions and better avalanching */
    long hash = 5381;
    size_t size = key->len;
    const char *data = key->buf;

    /* hash every character */
    while (size--)
        hash = ((hash << 5) + hash) + (*data++);

    /* invert the hash */
    return ~hash;
}

static inline char _lcc_is_prime(size_t value)
{
    for (size_t i = 2; i <= value / 2; i++)
        if ((value % i) == 0)
            return 0;

    return 1;
}

static inline size_t _lcc_next_prime(size_t value)
{
    while (!(_lcc_is_prime(++value)));
    return value;
}

static inline lcc_map_node_t *_lcc_find_node(lcc_map_t *self, lcc_string_t *key, long *hashp)
{
    /* calculate the hash and index */
    long hash = _lcc_djb_hash(key);
    size_t index = hash % self->capacity;

    /* store the hash value as needed */
    if (hashp)
        *hashp = hash;

    /* quadratic probing */
    for (size_t p, i = 0; i < self->capacity; i++)
    {
        /* move to next index */
        p = index + i * i;
        p %= self->capacity;

        /* empty nodes, cannot be after this */
        if (!(self->bucket[p].flags))
            break;

        /* skip deleted nodes */
        if (self->bucket[p].flags & LCC_MAP_FLAGS_DEL)
            continue;

        /* absolute equals, definately found */
        if (self->bucket[p].key == key)
            return &(self->bucket[p]);

        /* check for hash, key length and key string */
        if ((self->bucket[p].hash == hash) &&
            (self->bucket[p].key->len == key->len) &&
            (memcmp(self->bucket[p].key->buf, key->buf, key->len) == 0))
            return &(self->bucket[p]);
    }

    /* not found */
    return NULL;
}

static void _lcc_map_rehash(lcc_map_t *self)
{
    /* create a new bucket */
    size_t capacity = _lcc_next_prime(self->capacity * 2);
    lcc_map_node_t *bucket = calloc(capacity, sizeof(lcc_map_node_t));

    /* rehash all items */
    for (size_t i = 0; i < self->capacity; i++)
    {
        /* only rehash those are in-use */
        if (self->bucket[i].flags == LCC_MAP_FLAGS_USED)
        {
            /* calculate new index */
            size_t index = self->bucket[i].hash % capacity;
            lcc_map_node_t *slot = NULL;

            /* probe for an available slot */
            for (size_t p, j = 0; j < capacity; j++)
            {
                /* move to next index */
                p = index + j * j;
                p %= capacity;

                /* found an empty slot */
                if (!(bucket[p].flags))
                {
                    slot = &(bucket[p]);
                    break;
                }
            }

            /* must exists */
            if (!slot)
            {
                fprintf(stderr, "*** FATAL: rehash failed\n");
                abort();
            }

            /* copy all the attributes */
            slot->key = self->bucket[i].key;
            slot->hash = self->bucket[i].hash;
            slot->flags = self->bucket[i].flags;
            slot->value = self->bucket[i].value;
        }
    }

    /* replace with new one */
    free(self->bucket);
    self->bucket = bucket;
    self->capacity = capacity;
}

void lcc_map_free(lcc_map_t *self)
{
    /* clear items if any */
    for (size_t i = 0; self->dtor_fn && (i < self->capacity); i++)
    {
        if (self->bucket[i].flags == LCC_MAP_FLAGS_USED)
        {
            self->dtor_fn(self, self->bucket[i].value, self->dtor_data);
            lcc_string_unref(self->bucket[i].key);
            free(self->bucket[i].value);
        }
    }

    /* release the bucket */
    free(self->bucket);
}

void lcc_map_init(lcc_map_t *self, size_t value_size, lcc_map_dtor_fn dtor, void *data)
{
    /* initial map bucket */
    self->count = 0;
    self->bucket = calloc(LCC_MAP_INIT_CAP, sizeof(lcc_map_node_t));
    self->capacity = LCC_MAP_INIT_CAP;
    self->value_size = value_size;

    /* destructor info */
    self->dtor_fn = dtor;
    self->dtor_data = data;
}

char lcc_map_pop(lcc_map_t *self, lcc_string_t *key, void *data)
{
    /* lookup node in bucket */
    lcc_map_node_t *node = _lcc_find_node(self, key, NULL);

    /* node not found */
    if (!node)
        return 0;

    /* copy the old value as needed */
    if (data)
        memcpy(data, node->value, self->value_size);

    /* otherwise, destroy it */
    else if (self->dtor_fn)
        self->dtor_fn(self, node->value, self->dtor_data);

    /* release key and value memory */
    free(node->value);
    lcc_string_unref(node->key);

    /* set the deleted flags */
    self->count--;
    node->flags |= LCC_MAP_FLAGS_DEL;
    return 1;
}

char lcc_map_get(lcc_map_t *self, lcc_string_t *key, void **data)
{
    /* lookup node in bucket */
    lcc_map_node_t *node = _lcc_find_node(self, key, NULL);

    /* node not found */
    if (!node)
        return 0;

    /* retain the value address as needed */
    if (data)
        *data = node->value;

    /* but at least we found the node */
    return 1;
}

char lcc_map_set(lcc_map_t *self, lcc_string_t *key, void *data, const void *new)
{
    /* lookup node in bucket */
    long hash;
    lcc_map_node_t *node = _lcc_find_node(self, key, &hash);

    /* found in bucket */
    if (node)
    {
        /* retain the value address as needed */
        if (data)
            memcpy(data, node->value, self->value_size);

        /* destroy the old value */
        else if (self->dtor_fn)
            self->dtor_fn(self, node->value, self->dtor_data);

        /* replace the value */
        memcpy(node->value, new, self->value_size);
        return 1;
    }

    /* rehash as needed */
    if (self->count >= self->capacity * LCC_MAP_LOAD_FAC)
        _lcc_map_rehash(self);

    /* calculate node index */
    node = NULL;
    size_t index = hash % self->capacity;

    /* quadratic probing algorithm */
    for (size_t p, i = 0; i < self->capacity; i++)
    {
        /* move to next index */
        p = index + i * i;
        p %= self->capacity;

        /* use empty node or reuse deleted node */
        if ((self->bucket[p].flags == 0) ||
            (self->bucket[p].flags & LCC_MAP_FLAGS_DEL))
        {
            node = &(self->bucket[p]);
            break;
        }
    }

    /* must exists */
    if (!node)
    {
        fprintf(stderr, "*** FATAL: no available space for new node\n");
        abort();
    }

    /* create a new node */
    node->key = lcc_string_copy(key);
    node->hash = hash;
    node->flags = LCC_MAP_FLAGS_USED;
    node->value = malloc(self->value_size);
    memcpy(node->value, new, self->value_size);

    /* update node counter */
    self->count++;
    return 0;
}

char lcc_map_pop_string(lcc_map_t *self, const char *key, void *data)
{
    lcc_string_t *k = lcc_string_from(key);
    char ret = lcc_map_pop(self, k, data);
    lcc_string_unref(k);
    return ret;
}

char lcc_map_get_string(lcc_map_t *self, const char *key, void **data)
{
    lcc_string_t *k = lcc_string_from(key);
    char ret = lcc_map_get(self, k, data);
    lcc_string_unref(k);
    return ret;
}

char lcc_map_set_string(lcc_map_t *self, const char *key, void *data, const void *new)
{
    lcc_string_t *k = lcc_string_from(key);
    char ret = lcc_map_set(self, k, data, new);
    lcc_string_unref(k);
    return ret;
}
