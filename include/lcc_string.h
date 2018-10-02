#ifndef LCC_STRING_H
#define LCC_STRING_H

#include <stdlib.h>
#include <string.h>

typedef struct _lcc_string_t
{
    int ref;
    char *buf;
    size_t len;
    size_t capacity;
} lcc_string_t;

void lcc_string_unref(lcc_string_t *self);
lcc_string_t *lcc_string_ref(lcc_string_t *self);
lcc_string_t *lcc_string_copy(lcc_string_t *self);

lcc_string_t *lcc_string_new(void);
lcc_string_t *lcc_string_from(const char *s);
lcc_string_t *lcc_string_from_size(const char *s, size_t len);

#endif /* LCC_STRING_H */
