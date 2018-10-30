#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    new->buf = malloc(self->len + 1);

    /* copy string content */
    new->buf[new->len] = 0;
    memcpy(new->buf, self->buf, self->len);
    return new;
}

lcc_string_t *lcc_string_trim(lcc_string_t *self)
{
    /* string pointers */
    size_t n = self->len;
    const char *p = self->buf;

    /* trim right */
    while (n && isspace(p[n - 1]))
        n--;

    /* trim left */
    while (n && isspace(*p))
    {
        p++;
        n--;
    }

    /* build a new string */
    return lcc_string_from_buffer(p, n);
}

lcc_string_t *lcc_string_repr(lcc_string_t *self, char is_chars)
{
    /* hexadecimal encoding table */
    size_t n = self->len;
    const char *p = self->buf;
    static const char HexTable[] = "0123456789abcdef";

    /* quote character and result string */
    char quote = (char)(is_chars ? '\'' : '\"');
    lcc_string_t *result = lcc_string_from(is_chars ? "\'" : "\"");

    /* check for each character */
    while (n--)
    {
        /* move t next character */
        char ch = *p++;

        /* check for quote character */
        if (ch == quote)
        {
            lcc_string_append_from_format(result, "\\%c", ch);
            continue;
        }

        /* other characters */
        switch (ch)
        {
            /* control characters */
            case '\t': lcc_string_append_from(result, "\\t"); break;
            case '\n': lcc_string_append_from(result, "\\n"); break;
            case '\r': lcc_string_append_from(result, "\\r"); break;
            case '\a': lcc_string_append_from(result, "\\a"); break;
            case '\b': lcc_string_append_from(result, "\\b"); break;
            case '\f': lcc_string_append_from(result, "\\f"); break;
            case '\v': lcc_string_append_from(result, "\\v"); break;
            case '\\': lcc_string_append_from(result, "\\\\"); break;

            /* normal character */
            default:
            {
                /* check for printable characters */
                if ((ch >= ' ') && (ch < 0x7f))
                {
                    lcc_string_append_from_size(result, &ch, 1);
                    break;
                }

                /* non-printable characters */
                lcc_string_append_from(result, "\\x");
                lcc_string_append_from_size(result, &(HexTable[(ch & 0xf0) >> 4]), 1);
                lcc_string_append_from_size(result, &(HexTable[(ch & 0x0f) >> 0]), 1);
                break;
            }
        }
    }

    /* add the final quote */
    lcc_string_append_from_size(result, &quote, 1);
    return result;
}

lcc_string_t *lcc_string_new(size_t size)
{
    /* allocate new string */
    lcc_string_t *self = malloc(sizeof(lcc_string_t));

    /* string length and buffer */
    self->len = size;
    self->buf = NULL;

    /* initial string buffer as needed */
    if (size)
    {
        self->buf = malloc(size + 1);
        self->buf[size] = 0;
    }

    /* have 1 reference to begin with */
    self->ref = 1;
    return self;
}

lcc_string_t *lcc_string_from(const char *s)
{
    size_t len = strlen(s);
    return lcc_string_from_buffer(s, len);
}

lcc_string_t *lcc_string_from_buffer(const char *s, size_t len)
{
    /* allocate new string */
    lcc_string_t *self = malloc(sizeof(lcc_string_t));

    /* allocate string buffer */
    self->ref = 1;
    self->len = len;
    self->buf = malloc(len + 1);

    /* copy the initial string */
    self->buf[len] = 0;
    memcpy(self->buf, s, len);
    return self;
}

lcc_string_t *lcc_string_from_format(const char *fmt, ...)
{
    va_list vargs;
    va_start(vargs, fmt);
    lcc_string_t *self = lcc_string_from_format_va(fmt, vargs);
    va_end(vargs);
    return self;
}

lcc_string_t *lcc_string_from_format_va(const char *fmt, va_list vargs)
{
    /* format the string */
    char *s = NULL;
    ssize_t len = vasprintf(&s, fmt, vargs);

    /* check for errors */
    if (!s || (len < 0))
        return NULL;

    /* allocate new string */
    lcc_string_t *self = malloc(sizeof(lcc_string_t));

    /* set the string buffer */
    self->ref = 1;
    self->buf = s;
    self->len = (size_t)len;
    return self;
}

void lcc_string_append(lcc_string_t *self, lcc_string_t *other)
{
    /* delegate append function */
    lcc_string_append_from_size(self, other->buf, other->len);
}

void lcc_string_append_from(lcc_string_t *self, const char *other)
{
    size_t len = strlen(other);
    lcc_string_append_from_size(self, other, len);
}

void lcc_string_append_from_size(lcc_string_t *self, const char *other, size_t size)
{
    /* allocate new string buffer */
    self->len += size;
    self->buf = realloc(self->buf, self->len + 1);

    /* copy new string */
    self->buf[self->len] = 0;
    memcpy(self->buf + self->len - size, other, size);
}

char lcc_string_append_from_format(lcc_string_t *self, const char *fmt, ...)
{
    va_list vargs;
    va_start(vargs, fmt);
    char ret = lcc_string_append_from_format_va(self, fmt, vargs);
    va_end(vargs);
    return ret;
}

char lcc_string_append_from_format_va(lcc_string_t *self, const char *fmt, va_list vargs)
{
    /* format the string */
    char *s = NULL;
    ssize_t len = vasprintf(&s, fmt, vargs);

    /* check for errors */
    if (!s || (len < 0))
        return 0;

    /* append to buffer and free string */
    lcc_string_append_from_size(self, s, (size_t)len);
    free(s);
    return 1;
}
