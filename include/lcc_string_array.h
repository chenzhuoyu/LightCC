#ifndef LCC_STRING_ARRAY_H
#define LCC_STRING_ARRAY_H

#include <sys/types.h>

#include "lcc_array.h"
#include "lcc_string.h"

typedef struct _lcc_string_array_t
{
    lcc_array_t array;
} lcc_string_array_t;

/* static initializer */
extern lcc_string_array_t LCC_STRING_ARRAY_STATIC_INIT;

void lcc_string_array_free(lcc_string_array_t *self);
void lcc_string_array_init(lcc_string_array_t *self);

void lcc_string_array_remove(lcc_string_array_t *self, size_t index);
void lcc_string_array_append(lcc_string_array_t *self, lcc_string_t *str);
ssize_t lcc_string_array_index(lcc_string_array_t *self, lcc_string_t *str);

lcc_string_t *lcc_string_array_top(lcc_string_array_t *self);
lcc_string_t *lcc_string_array_pop(lcc_string_array_t *self);
lcc_string_t *lcc_string_array_get(lcc_string_array_t *self, size_t index);
lcc_string_t *lcc_string_array_set(lcc_string_array_t *self, size_t index, lcc_string_t *str);

#endif /* LCC_STRING_ARRAY_H */
