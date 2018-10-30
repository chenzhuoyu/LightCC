#include <string.h>
#include "lcc_string_array.h"

static void _lcc_string_dtor(lcc_array_t *self, void *item, void *data)
{
    lcc_string_t **ps = item;
    lcc_string_unref(*ps);
    *ps = NULL;
}

lcc_string_array_t LCC_STRING_ARRAY_STATIC_INIT = {
    .array = LCC_ARRAY_STATIC_INIT(
         sizeof(lcc_string_t *),
         _lcc_string_dtor,
         NULL
    ),
};


void lcc_string_array_free(lcc_string_array_t *self)
{
    /* convenient alias function */
    lcc_array_free(&(self->array));
}

void lcc_string_array_init(lcc_string_array_t *self)
{
    /* initialize as array with string destructors */
    lcc_array_init(&(self->array), sizeof(lcc_string_t *), _lcc_string_dtor, NULL);
}

void lcc_string_array_remove(lcc_string_array_t *self, size_t index)
{
    /* convenient alias function */
    lcc_array_remove(&(self->array), index);
}

void lcc_string_array_append(lcc_string_array_t *self, lcc_string_t *str)
{
    /* data is the string pointer */
    lcc_array_append(&(self->array), &str);
}

ssize_t lcc_string_array_index(lcc_string_array_t *self, lcc_string_t *str)
{
    /* compare each item */
    for (size_t i = 0; i < self->array.count; i++)
        if ((lcc_string_array_get(self, i)->len == str->len) &&
            !(strncmp(lcc_string_array_get(self, i)->buf, str->buf, str->len)))
            return i;

    /* not found */
    return -1;
}

lcc_string_t *lcc_string_array_top(lcc_string_array_t *self)
{
    void *p = lcc_array_top(&(self->array));
    return p ? *(lcc_string_t **)p : NULL;
}

lcc_string_t *lcc_string_array_pop(lcc_string_array_t *self)
{
    lcc_string_t *val;
    return lcc_array_pop(&(self->array), &val) ? val : NULL;
}

lcc_string_t *lcc_string_array_get(lcc_string_array_t *self, size_t index)
{
    void *p = lcc_array_get(&(self->array), index);
    return p ? *(lcc_string_t **)p : NULL;
}

lcc_string_t *lcc_string_array_set(lcc_string_array_t *self, size_t index, lcc_string_t *str)
{
    void *p = lcc_array_set(&(self->array), index, &str);
    return p ? *(lcc_string_t **)p : NULL;
}
