#include <errno.h>
#include <libgen.h>
#include <stdlib.h>
#include <string.h>

#include "lcc_lexer.h"

/*** Tokens ***/

void lcc_token_free(lcc_token_t *self)
{
    switch (self->type)
    {
        case LCC_TK_EOF:
            break;

        case LCC_TK_IDENT:
        {
            lcc_string_unref(self->ident);
            break;
        }

        case LCC_TK_LITERAL:
        {
            switch (self->literal.type)
            {
                case LCC_LT_INT:
                case LCC_LT_LONG:
                case LCC_LT_LONGLONG:
                case LCC_LT_UINT:
                case LCC_LT_ULONG:
                case LCC_LT_ULONGLONG:
                case LCC_LT_FLOAT:
                case LCC_LT_DOUBLE:
                case LCC_LT_LONGDOUBLE:
                    break;

                case LCC_LT_CHAR:
                {
                    lcc_string_unref(self->literal.v_char);
                    break;
                }

                case LCC_LT_STRING:
                {
                    lcc_string_unref(self->literal.v_string);
                    break;
                }
            }

            break;
        }

        case LCC_TK_KEYWORD:
        case LCC_TK_OPERATOR:
            break;
    }
}

void lcc_token_init(lcc_token_t *self)
{
    memset(self, 0, sizeof(lcc_token_t));
    self->type = LCC_TK_EOF;
}

void lcc_token_set_ident(lcc_token_t *self, lcc_string_t *ident)
{
    lcc_token_free(self);
    self->type = LCC_TK_IDENT;
    self->ident = ident;
}

void lcc_token_set_keyword(lcc_token_t *self, lcc_keyword_t keyword)
{
    lcc_token_free(self);
    self->type = LCC_TK_KEYWORD;
    self->keyword = keyword;
}

void lcc_token_set_operator(lcc_token_t *self, lcc_operator_t operator)
{
    lcc_token_free(self);
    self->type = LCC_TK_OPERATOR;
    self->operator = operator;
}

void lcc_token_set_int(lcc_token_t *self, int value)
{
    lcc_token_free(self);
    self->type = LCC_TK_LITERAL;
    self->literal.type = LCC_LT_INT;
    self->literal.v_int = value;
}

void lcc_token_set_long(lcc_token_t *self, long value)
{
    lcc_token_free(self);
    self->type = LCC_TK_LITERAL;
    self->literal.type = LCC_LT_LONG;
    self->literal.v_long = value;
}

void lcc_token_set_longlong(lcc_token_t *self, long long value)
{
    lcc_token_free(self);
    self->type = LCC_TK_LITERAL;
    self->literal.type = LCC_LT_LONGLONG;
    self->literal.v_longlong = value;
}

void lcc_token_set_uint(lcc_token_t *self, unsigned int value)
{
    lcc_token_free(self);
    self->type = LCC_TK_LITERAL;
    self->literal.type = LCC_LT_UINT;
    self->literal.v_uint = value;
}

void lcc_token_set_ulong(lcc_token_t *self, unsigned long value)
{
    lcc_token_free(self);
    self->type = LCC_TK_LITERAL;
    self->literal.type = LCC_LT_ULONG;
    self->literal.v_ulong = value;
}

void lcc_token_set_ulonglong(lcc_token_t *self, unsigned long long value)
{
    lcc_token_free(self);
    self->type = LCC_TK_LITERAL;
    self->literal.type = LCC_LT_ULONGLONG;
    self->literal.v_ulonglong = value;
}

void lcc_token_set_float(lcc_token_t *self, float value)
{
    lcc_token_free(self);
    self->type = LCC_TK_LITERAL;
    self->literal.type = LCC_LT_FLOAT;
    self->literal.v_float = value;
}

void lcc_token_set_double(lcc_token_t *self, double value)
{
    lcc_token_free(self);
    self->type = LCC_TK_LITERAL;
    self->literal.type = LCC_LT_DOUBLE;
    self->literal.v_double = value;
}

void lcc_token_set_longdouble(lcc_token_t *self, long double value)
{
    lcc_token_free(self);
    self->type = LCC_TK_LITERAL;
    self->literal.type = LCC_LT_LONGDOUBLE;
    self->literal.v_longdouble = value;
}

void lcc_token_set_char(lcc_token_t *self, lcc_string_t *value)
{
    lcc_token_free(self);
    self->type = LCC_TK_LITERAL;
    self->literal.type = LCC_LT_CHAR;
    self->literal.v_char = value;
}

void lcc_token_set_string(lcc_token_t *self, lcc_string_t *value)
{
    lcc_token_free(self);
    self->type = LCC_TK_LITERAL;
    self->literal.type = LCC_LT_STRING;
    self->literal.v_string = value;
}

/*** Source File ***/

static const lcc_file_t INVALID_FILE = {
    .col = -1,
    .row = -1,
    .pos = -1,
    .name = NULL,
    .flags = LCC_FF_INVALID,
    .lines = {},
};

lcc_file_t lcc_file_open(const char *fname)
{
    /* open file and load from it */
    FILE *fp = fopen(fname, "rb");
    lcc_file_t ret = lcc_file_from_file(fname, fp);

    /* close file */
    if (fp) fclose(fp);
    return ret;
}

lcc_file_t lcc_file_from_file(const char *fname, FILE *fp)
{
    /* check for file pointer */
    if (!fp)
        return INVALID_FILE;

    /* result file */
    lcc_file_t result = {
        .col = 0,
        .row = 0,
        .pos = 0,
        .name = lcc_string_from(fname),
        .flags = 0,
        .lines = LCC_STRING_ARRAY_STATIC_INIT,
    };

    /* line buffer and line size */
    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;

    /* read every line */
    while ((linelen = getline(&line, &linecap, fp)) > 0)
    {
        /* remove new line */
        if (linelen && (line[linelen - 1] == '\n'))
            line[--linelen] = 0;

        /* windows uses "\r\n" as newline delimiter */
        if (linelen && (line[linelen - 1] == '\r'))
            line[--linelen] = 0;

        /* append to line buffer */
        lcc_string_array_append(&(result.lines), lcc_string_from_buffer(line, (size_t) linelen));
    }

    /* check for errors */
    if (!ferror(fp))
    {
        free(line);
        return result;
    }
    else
    {
        free(line);
        lcc_string_unref(result.name);
        lcc_string_array_free(&(result.lines));
        return INVALID_FILE;
    }
}

lcc_file_t lcc_file_from_string(const char *fname, const char *data, size_t size)
{
    /* check for content pointer */
    if (!data)
    {
        errno = EINVAL;
        return INVALID_FILE;
    }

    /* result file */
    lcc_file_t result = {
        .col = 0,
        .row = 0,
        .pos = 0,
        .name = lcc_string_from(fname),
        .flags = 0,
        .lines = LCC_STRING_ARRAY_STATIC_INIT,
    };

    /* split every line */
    char *p;
    while ((p = memchr(data, '\n', size)))
    {
        /* check for "\r\n" */
        if ((p == data) || (p[-1] != '\r'))
            lcc_string_array_append(&(result.lines), lcc_string_from_buffer(data, p - data));
        else
            lcc_string_array_append(&(result.lines), lcc_string_from_buffer(data, p - data - 1));

        /* skip the "\n" */
        size -= (p - data + 1);
        data = p + 1;
    }

    /* remove "\r" if any */
    if (size && (data[size - 1] == '\r'))
        size--;

    /* add to line buffer */
    lcc_string_array_append(&(result.lines), lcc_string_from_buffer(data, size));
    return result;
}

/*** Token Buffer ***/

void lcc_token_buffer_free(lcc_token_buffer_t *self)
{
    free(self->buf);
    self->buf = NULL;
    self->len = self->cap = 0;
}

void lcc_token_buffer_init(lcc_token_buffer_t *self)
{
    self->len = 0;
    self->cap = 256;
    self->buf = malloc(self->cap + 1);
    self->buf[0] = 0;
}

void lcc_token_buffer_reset(lcc_token_buffer_t *self)
{
    self->len = 0;
    self->buf[0] = 0;
}

void lcc_token_buffer_append(lcc_token_buffer_t *self, char ch)
{
    /* check buffer length */
    if (self->len >= self->cap)
    {
        self->cap *= 2;
        self->buf = realloc(self->buf, self->cap + 1);
    }

    /* append to buffer */
    self->buf[self->len] = ch;
    self->buf[++self->len] = 0;
}

/*** Lexer Object ***/

static void _lcc_file_dtor(lcc_array_t *self, void *item, void *data)
{
    lcc_string_unref(((lcc_file_t *)item)->name);
    lcc_string_array_free(&(((lcc_file_t *)item)->lines));
}

static lcc_string_t *_lcc_get_dir_name(lcc_string_t *name)
{
    /* get it's directory name */
    char *buf = strndup(name->buf, name->len);
    lcc_string_t *ret = lcc_string_from(dirname(buf));

    /* free the string buffer */
    free(buf);
    return ret;
}

void lcc_lexer_free(lcc_lexer_t *self)
{
    lcc_token_free(&(self->token));
    lcc_array_free(&(self->files));
    lcc_token_buffer_free(&(self->token_buffer));
    lcc_string_array_free(&(self->include_paths));
}

char lcc_lexer_init(lcc_lexer_t *self, lcc_file_t file)
{
    /* check for file flags */
    if (file.flags & LCC_FF_INVALID)
        return 0;

    // TODO: init predefined syms (psyms)

    /* initial source file */
    lcc_array_init(&(self->files), sizeof(lcc_file_t), _lcc_file_dtor, NULL);
    lcc_array_append(&(self->files), &file);

    /* initial include search directory */
    lcc_string_array_init(&(self->include_paths));
    lcc_string_array_append(&(self->include_paths), _lcc_get_dir_name(file.name));

    /* initial library search directory */
    lcc_string_array_init(&(self->library_paths));
    lcc_string_array_append(&(self->library_paths), _lcc_get_dir_name(file.name));

    /* initial token and token buffer */
    lcc_token_init(&(self->token));
    lcc_token_buffer_init(&(self->token_buffer));

    /* initial lexer state */
    self->file = lcc_array_top(&(self->files));
    self->state = LCC_LX_STATE_INIT;
    return 1;
}

char lcc_lexer_next(lcc_lexer_t *self, lcc_token_t *token)
{
    for (;;)
    {
        switch (self->state)
        {
            /* initial lexer state, reset token buffer */
            case LCC_LX_STATE_INIT:
            {
                self->state = LCC_LX_STATE_SHIFT;
                lcc_token_buffer_reset(&(self->token_buffer));
                break;
            }

            /* shift-in character */
            case LCC_LX_STATE_SHIFT:
            {
                break;
            }

            /* token accepted */
            case LCC_LX_STATE_ACCEPT:
                return 1;

            /* token rejected */
            case LCC_LX_STATE_REJECT:
                return 0;
        }
    }
}

void lcc_lexer_add_define(lcc_lexer_t *self, const char *name, const char *value)
{
    // TODO: add define
}

void lcc_lexer_add_include_path(lcc_lexer_t *self, const char *path)
{
    // TODO: add include path
}

void lcc_lexer_add_library_path(lcc_lexer_t *self, const char *path)
{
    // TODO: add library path
}

void lcc_lexer_set_error_handler(lcc_lexer_t *self, lcc_lexer_on_error_fn error_fn, void *data)
{
    self->error_fn = error_fn;
    self->error_data = data;
}
