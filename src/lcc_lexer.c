#include <ctype.h>
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

void lcc_token_set_eof(lcc_token_t *self)
{
    lcc_token_free(self);
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
    .col = 0,
    .row = 0,
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
    if (!(ferror(fp)))
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

static void _lcc_lexer_error(lcc_lexer_t *self, lcc_string_t *message)
{
    /* invoke the error handler if any */
    if (self->error_fn)
    {
        self->error_fn(
            self,
            self->file->name,
            self->file->row,
            self->file->col,
            message,
            LCC_LXET_ERROR,
            self->error_data
        );
    }

    /* advance state machine to REJECT */
    self->state = LCC_LX_STATE_REJECT;
}

static void _lcc_lexer_warning(lcc_lexer_t *self, lcc_string_t *message)
{
    /* invoke the error handler if any */
    if (self->error_fn)
    {
        self->error_fn(
            self,
            self->file->name,
            self->file->row,
            self->file->col,
            message,
            LCC_LXET_WARNING,
            self->error_data
        );
    }
}

static char _lcc_error_default(
    lcc_lexer_t             *self,
    lcc_string_t            *file,
    ssize_t                  row,
    ssize_t                  col,
    lcc_string_t            *message,
    lcc_lexer_error_type_t   type,
    void                    *data
)
{
    /* print the error message */
    fprintf(
        stderr,
        "* %s: (%s:%zd:%zd) %s\n",
        type == LCC_LXET_ERROR ? "ERROR" : "WARNING",
        file->buf,
        row + 1,
        col + 1,
        message->buf
    );

    /* cannot continue if it's an error */
    return type != LCC_LXET_ERROR;
}

static void _lcc_handle_substate(lcc_lexer_t *self)
{
    // TODO: handle substate
    if ((self->flags & LCC_LXF_EOF) ||
        (self->flags & LCC_LXF_EOL))
    {
        self->state = LCC_LX_STATE_ACCEPT;
        return;
    }

    printf("SUBSTATE(%d): %c\n", self->substate, self->ch);
    self->state = LCC_LX_STATE_SHIFT;
}

static inline char _lcc_check_line_cont(lcc_file_t *fp, lcc_string_t *line)
{
    /* check for every character after this */
    for (size_t i = fp->col; i < line->len; i++)
        if (!(isspace(line->buf[i])))
            return 0;

    /* it is a line continuation, but with whitespaces */
    return 1;
}

static inline lcc_string_t *_lcc_get_dir_name(lcc_string_t *name)
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
    self->ch = 0;
    self->file = lcc_array_top(&(self->files));
    self->state = LCC_LX_STATE_INIT;
    self->substate = LCC_LX_SUBSTATE_NULL;

    /* default error handling */
    self->error_fn = _lcc_error_default;
    self->error_data = NULL;
    return 1;
}

char lcc_lexer_next(lcc_lexer_t *self, lcc_token_t **token)
{
    for (;;)
    {
        switch (self->state)
        {
            /* initial lexer state */
            case LCC_LX_STATE_INIT:
            {
                self->state = LCC_LX_STATE_SHIFT;
                self->file->flags &= ~LCC_FF_LNODIR;
                lcc_token_buffer_reset(&(self->token_buffer));
                break;
            }

            /* shift-in character */
            case LCC_LX_STATE_SHIFT:
            {
                /* get the current line */
                lcc_file_t *file = self->file;
                lcc_string_t *line = lcc_string_array_get(&(file->lines), file->row);

                /* line out of range, it's EOF
                 * pop the current file from file stack */
                if (!line)
                {
                    self->state = LCC_LX_STATE_POP_FILE;
                    break;
                }

                /* limit maximum line length */
                if (line->len > LCC_LEXER_MAX_LINE_LEN)
                {
                    _lcc_lexer_error(self, lcc_string_from("Line too long"));
                    break;
                }

                /* EOL, move to next line */
                if (file->col >= line->len)
                {
                    self->state = LCC_LX_STATE_NEXT_LINE;
                    break;
                }

                /* read the current character */
                self->ch = line->buf[file->col];
                file->col++;

                /* check current character */
                switch (self->ch)
                {
                    /* maybe line continuation */
                    case '\\':
                    {
                        /* line continuation, move to next line */
                        if (file->col == line->len)
                        {
                            self->state = LCC_LX_STATE_NEXT_LINE_CONT;
                            break;
                        }

                        /* check if it's a generic character */
                        if (!(_lcc_check_line_cont(file, line)))
                        {
                            self->state = LCC_LX_STATE_ADVANCE;
                            break;
                        }

                        /* line continuation, but with whitespaces after "\\",
                         * issue a warning and move to next line */
                        self->state = LCC_LX_STATE_NEXT_LINE_CONT;
                        _lcc_lexer_warning(self, lcc_string_from("Whitespaces after line continuation"));
                        break;
                    }

                    /* maybe compiler directive */
                    case '#':
                    {
                        // TODO: handle compiler directive
                    }

                    /* generic character */
                    default:
                    {
                        self->state = LCC_LX_STATE_ADVANCE;
                        break;
                    }
                }

                break;
            }

            /* pop file from file stack */
            case LCC_LX_STATE_POP_FILE:
            {
                /* set EOF flags to flush the sub-state */
                self->flags |= LCC_LXF_EOF;
                _lcc_handle_substate(self);
                self->flags &= ~LCC_LXF_EOF;

                /* no files left, it's an EOS (End-Of-Source) */
                if (self->files.count == 1)
                {
                    self->flags |= LCC_LXF_EOS;
                    break;
                }

                /* pop the file from stack */
                if (!(lcc_array_pop(&(self->files), NULL)))
                {
                    fprintf(stderr, "*** FATAL: empty file stack\n");
                    abort();
                }

                /* get the new stack top */
                self->file = lcc_array_top(&(self->files));
                break;
            }

            /* move to next line */
            case LCC_LX_STATE_NEXT_LINE:
            {
                /* set EOL flags to flush the sub-state */
                self->flags |= LCC_LXF_EOL;
                _lcc_handle_substate(self);
                self->flags &= ~LCC_LXF_EOL;

                /* move to next line, and reset compiler directive flags */
                self->file->row++;
                self->file->col = 0;
                self->file->flags &= ~LCC_FF_LNODIR;
                break;
            }

            /* move to next line (line continuation) */
            case LCC_LX_STATE_NEXT_LINE_CONT:
            {
                self->file->row++;
                self->file->col = 0;
                self->state = LCC_LX_STATE_SHIFT;
                break;
            }

            /* we got a new character, advance the substate */
            case LCC_LX_STATE_ADVANCE:
            {
                /* not a space, prohibit compiler directive on this line */
                if (!(isspace(self->ch)))
                    self->file->flags |= LCC_FF_LNODIR;

                /* handle all sub-states */
                _lcc_handle_substate(self);
                break;
            }

            /* token accepted */
            case LCC_LX_STATE_ACCEPT:
            {
                // TODO: accept token
                self->state = LCC_LX_STATE_SHIFT;
                self->substate = LCC_LX_SUBSTATE_NULL;
                return 1;
            }

            /* token rejected */
            case LCC_LX_STATE_REJECT:
            {
                // TODO: reject token
                self->state = LCC_LX_STATE_SHIFT;
                self->substate = LCC_LX_SUBSTATE_NULL;
                return 0;
            }
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
