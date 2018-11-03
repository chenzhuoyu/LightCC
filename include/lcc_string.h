#ifndef LCC_STRING_H
#define LCC_STRING_H

#include <stdarg.h>
#include <stddef.h>

typedef struct _lcc_string_t
{
    int ref;
    char *buf;
    size_t len;
} lcc_string_t;

void lcc_string_unref(lcc_string_t *self);
char lcc_string_equals(lcc_string_t *self, lcc_string_t *other);

lcc_string_t *lcc_string_ref(lcc_string_t *self);
lcc_string_t *lcc_string_copy(lcc_string_t *self);
lcc_string_t *lcc_string_trim(lcc_string_t *self);
lcc_string_t *lcc_string_repr(lcc_string_t *self, char is_chars);

lcc_string_t *lcc_string_new(size_t size);
lcc_string_t *lcc_string_from(const char *s);
lcc_string_t *lcc_string_from_buffer(const char *s, size_t len);

lcc_string_t *lcc_string_from_format(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
lcc_string_t *lcc_string_from_format_va(const char *fmt, va_list vargs);

void lcc_string_append(lcc_string_t *self, lcc_string_t *other);
void lcc_string_append_from(lcc_string_t *self, const char *other);
void lcc_string_append_from_size(lcc_string_t *self, const char *other, size_t size);

char lcc_string_append_from_format(lcc_string_t *self, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
char lcc_string_append_from_format_va(lcc_string_t *self, const char *fmt, va_list vargs);

#endif /* LCC_STRING_H */
