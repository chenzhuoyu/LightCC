#include "lcc_string.h"

void lcc_string_unref(lcc_string_t *self)
{
    if (!(--self->ref))
    {
        free(self->buf);
        free(self);
    }
}

lcc_string_t *lcc_string_ref(lcc_string_t *self)
{
    self->ref++;
    return self;
}

lcc_string_t *lcc_string_copy(lcc_string_t *self)
{
    /* allocate new string */
    lcc_string_t *new = malloc(sizeof(lcc_string_t));

    /* make an identical copy except the buffer pointer */
    new->ref = 1;
    new->len = self->len;
    new->buf = malloc(self->capacity);
    new->capacity = self->capacity;

    /* copy string content */
    strncpy(new->buf, self->buf, new->capacity);
    return new;
}

lcc_string_t *lcc_string_new(void)
{
    /* allocate new string */
    lcc_string_t *self = malloc(sizeof(lcc_string_t));

    /* initial string buffer */
    self->ref = 1;
    self->len = 0;
    self->buf = NULL;
    self->capacity = 0;
    return self;
}

lcc_string_t *lcc_string_from(const char *s)
{
    size_t len = strlen(s);
    return lcc_string_from_size(s, len);
}

lcc_string_t *lcc_string_from_size(const char *s, size_t len)
{
    /* initial capacity (NULL-terminator included, no need to subtract by one) */
    size_t cap = (len / 256 + 1) * 256;
    lcc_string_t *self = malloc(sizeof(lcc_string_t));

    /* allocate string buffer */
    self->ref = 1;
    self->len = len;
    self->buf = malloc(cap);
    self->capacity = cap;

    /* copy the initial string */
    strncpy(self->buf, s, self->capacity);
    return self;
}
