#ifndef LCC_MAP_H
#define LCC_MAP_H

#include <stddef.h>
#include "lcc_string.h"

struct _lcc_map_t;
typedef void (*lcc_map_dtor_fn)(struct _lcc_map_t *self, void *value, void *data);

#define LCC_MAP_FLAGS_DEL   0x00000001      /* node was deleted */
#define LCC_MAP_FLAGS_USED  0x00000002      /* node was occupied */

typedef struct _lcc_map_node_t
{
    long hash;
    long flags;
    void *value;
    lcc_string_t *key;
} lcc_map_node_t;

typedef struct _lcc_map_t
{
    size_t count;
    size_t capacity;
    size_t value_size;
    lcc_map_node_t *bucket;

    void *dtor_data;
    lcc_map_dtor_fn dtor_fn;
} lcc_map_t;

void lcc_map_free(lcc_map_t *self);
void lcc_map_init(lcc_map_t *self, size_t value_size, lcc_map_dtor_fn dtor, void *data);

char lcc_map_pop(lcc_map_t *self, lcc_string_t *key, void *data);
char lcc_map_get(lcc_map_t *self, lcc_string_t *key, void *data);
char lcc_map_set(lcc_map_t *self, lcc_string_t *key, void *old, const void *new);

char lcc_map_pop_string(lcc_map_t *self, const char *key, void *data);
char lcc_map_get_string(lcc_map_t *self, const char *key, void *data);
char lcc_map_set_string(lcc_map_t *self, const char *key, void *old, const void *new);

#endif /* LCC_MAP_H */
