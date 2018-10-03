#include <stdlib.h>
#include <string.h>

#include "lcc_array.h"

#define PTR_INDEX(self, index) \
    ((void *)((uintptr_t)self->items + (index) * self->item_size))

void lcc_array_free(lcc_array_t *self)
{
    /* destruct every item */
    if (self->dtor_fn)
        for (size_t i = 0; i < self->count; i++)
            self->dtor_fn(self, PTR_INDEX(self, i), self->dtor_data);

    /* clear the item buffer */
    free(self->items);
}

void lcc_array_init(lcc_array_t *self, size_t item_size, lcc_array_dtor_fn dtor, void *data)
{
    /* initialize an empty array */
    self->count = 0;
    self->items = NULL;
    self->item_size = item_size;

    /* set item destructor */
    self->dtor_fn = dtor;
    self->dtor_data = data;
}

void *lcc_array_top(lcc_array_t *self)
{
    if (!(self->count))
        return NULL;
    else
        return PTR_INDEX(self, self->count - 1);
}

void *lcc_array_get(lcc_array_t *self, size_t index)
{
    if (index >= self->count)
        return NULL;
    else
        return PTR_INDEX(self, index);
}

void *lcc_array_set(lcc_array_t *self, size_t index, void *data)
{
    /* check for index */
    if (index >= self->count)
        return NULL;

    /* destroy old item */
    if (self->dtor_fn)
        self->dtor_fn(self, PTR_INDEX(self, index), self->dtor_data);

    /* set the new item */
    memcpy(PTR_INDEX(self, index), data, self->item_size);
    return PTR_INDEX(self, index);
}

char lcc_array_pop(lcc_array_t *self, void *data)
{
    /* check for count */
    if (!(self->count))
        return 0;

    /* copy item as needed */
    if (data)
        memcpy(data, PTR_INDEX(self, self->count - 1), self->item_size);

    /* or destroy the item */
    else if (self->dtor_fn)
        self->dtor_fn(self, PTR_INDEX(self, self->count - 1), self->dtor_data);

    /* resize the memory and counter */
    self->count--;
    self->items = realloc(self->items, self->count * self->item_size);
    return 1;
}

void lcc_array_append(lcc_array_t *self, void *data)
{
    /* resize the memory and counter */
    self->count++;
    self->items = realloc(self->items, self->count * self->item_size);

    /* copy new item */
    memcpy(PTR_INDEX(self, self->count - 1), data, self->item_size);
}

char lcc_array_remove(lcc_array_t *self, size_t index)
{
    /* check for index */
    if (index >= self->count)
        return 0;

    /* destroy old item */
    if (self->dtor_fn)
        self->dtor_fn(self, PTR_INDEX(self, index), self->dtor_data);

    /* move everything forward by one */
    if (index < self->count - 1)
    {
        memmove(
            PTR_INDEX(self, index),
            PTR_INDEX(self, index + 1),
            (self->count - index - 1) * self->item_size
        );
    }

    /* resize the memory and counter */
    self->count--;
    self->items = realloc(self->items, self->count * self->item_size);
    return 1;
}
