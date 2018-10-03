#ifndef LCC_ARRAY_H
#define LCC_ARRAY_H

#include <stddef.h>

struct _lcc_array_t;
typedef void (*lcc_array_dtor_fn)(struct _lcc_array_t *self, void *item, void *data);

typedef struct _lcc_array_t
{
    void *items;
    size_t count;
    size_t item_size;

    void *dtor_data;
    lcc_array_dtor_fn dtor_fn;
} lcc_array_t;

#define LCC_ARRAY_STATIC_INIT(_item_size, _dtor_fn, _dtor_data)   { \
    .count      = 0,                                                \
    .items      = NULL,                                             \
    .item_size  = _item_size,                                       \
    .dtor_fn    = _dtor_fn,                                         \
    .dtor_data  = _dtor_data,                                       \
}

void lcc_array_free(lcc_array_t *self);
void lcc_array_init(lcc_array_t *self, size_t item_size, lcc_array_dtor_fn dtor, void *data);

void *lcc_array_top(lcc_array_t *self);
void *lcc_array_get(lcc_array_t *self, size_t index);
void *lcc_array_set(lcc_array_t *self, size_t index, void *data);

char lcc_array_pop(lcc_array_t *self, void *data);
void lcc_array_append(lcc_array_t *self, void *data);
char lcc_array_remove(lcc_array_t *self, size_t index);

#endif /* LCC_ARRAY_H */
