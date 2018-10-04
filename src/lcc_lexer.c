#include <ctype.h>
#include <errno.h>
#include <libgen.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "lcc_lexer.h"

/*** Tokens ***/

void lcc_token_free(lcc_token_t *self)
{
    if (self)
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

        self->prev->next = self->next;
        self->next->prev = self->prev;
        free(self);
    }
}

void lcc_token_init(lcc_token_t *self)
{
    self->prev = self;
    self->next = self;
    self->type = LCC_TK_EOF;
}

void lcc_token_attach(lcc_token_t *self, lcc_token_t *tail)
{
    tail->next = self;
    tail->prev = self->prev;
    self->prev->next = tail;
    self->prev = tail;
}

lcc_token_t *lcc_token_from_eof(void)
{
    lcc_token_t *self = malloc(sizeof(lcc_token_t));
    self->prev = self;
    self->next = self;
    self->type = LCC_TK_EOF;
    return self;
}

lcc_token_t *lcc_token_from_ident(lcc_string_t *ident)
{
    lcc_token_t *self = malloc(sizeof(lcc_token_t));
    self->prev = self;
    self->next = self;
    self->type = LCC_TK_IDENT;
    self->ident = ident;
    return self;
}

lcc_token_t *lcc_token_from_keyword(lcc_keyword_t keyword)
{
    lcc_token_t *self = malloc(sizeof(lcc_token_t));
    self->prev = self;
    self->next = self;
    self->type = LCC_TK_KEYWORD;
    self->keyword = keyword;
    return self;
}

lcc_token_t *lcc_token_from_operator(lcc_operator_t operator)
{
    lcc_token_t *self = malloc(sizeof(lcc_token_t));
    self->prev = self;
    self->next = self;
    self->type = LCC_TK_OPERATOR;
    self->operator = operator;
    return self;
}

lcc_token_t *lcc_token_from_int(int value)
{
    lcc_token_t *self = malloc(sizeof(lcc_token_t));
    self->prev = self;
    self->next = self;
    self->type = LCC_TK_LITERAL;
    self->literal.type = LCC_LT_INT;
    self->literal.v_int = value;
    return self;
}

lcc_token_t *lcc_token_from_long(long value)
{
    lcc_token_t *self = malloc(sizeof(lcc_token_t));
    self->prev = self;
    self->next = self;
    self->type = LCC_TK_LITERAL;
    self->literal.type = LCC_LT_LONG;
    self->literal.v_long = value;
    return self;
}

lcc_token_t *lcc_token_from_longlong(long long value)
{
    lcc_token_t *self = malloc(sizeof(lcc_token_t));
    self->prev = self;
    self->next = self;
    self->type = LCC_TK_LITERAL;
    self->literal.type = LCC_LT_LONGLONG;
    self->literal.v_longlong = value;
    return self;
}

lcc_token_t *lcc_token_from_uint(unsigned int value)
{
    lcc_token_t *self = malloc(sizeof(lcc_token_t));
    self->prev = self;
    self->next = self;
    self->type = LCC_TK_LITERAL;
    self->literal.type = LCC_LT_UINT;
    self->literal.v_uint = value;
    return self;
}

lcc_token_t *lcc_token_from_ulong(unsigned long value)
{
    lcc_token_t *self = malloc(sizeof(lcc_token_t));
    self->prev = self;
    self->next = self;
    self->type = LCC_TK_LITERAL;
    self->literal.type = LCC_LT_ULONG;
    self->literal.v_ulong = value;
    return self;
}

lcc_token_t *lcc_token_from_ulonglong(unsigned long long value)
{
    lcc_token_t *self = malloc(sizeof(lcc_token_t));
    self->prev = self;
    self->next = self;
    self->type = LCC_TK_LITERAL;
    self->literal.type = LCC_LT_ULONGLONG;
    self->literal.v_ulonglong = value;
    return self;
}

lcc_token_t *lcc_token_from_float(float value)
{
    lcc_token_t *self = malloc(sizeof(lcc_token_t));
    self->prev = self;
    self->next = self;
    self->type = LCC_TK_LITERAL;
    self->literal.type = LCC_LT_FLOAT;
    self->literal.v_float = value;
    return self;
}

lcc_token_t *lcc_token_from_double(double value)
{
    lcc_token_t *self = malloc(sizeof(lcc_token_t));
    self->prev = self;
    self->next = self;
    self->type = LCC_TK_LITERAL;
    self->literal.type = LCC_LT_DOUBLE;
    self->literal.v_double = value;
    return self;
}

lcc_token_t *lcc_token_from_longdouble(long double value)
{
    lcc_token_t *self = malloc(sizeof(lcc_token_t));
    self->prev = self;
    self->next = self;
    self->type = LCC_TK_LITERAL;
    self->literal.type = LCC_LT_LONGDOUBLE;
    self->literal.v_longdouble = value;
    return self;
}

lcc_token_t *lcc_token_from_char(lcc_string_t *value)
{
    lcc_token_t *self = malloc(sizeof(lcc_token_t));
    self->prev = self;
    self->next = self;
    self->type = LCC_TK_LITERAL;
    self->literal.type = LCC_LT_CHAR;
    self->literal.v_char = value;
    return self;
}

lcc_token_t *lcc_token_from_string(lcc_string_t *value)
{
    lcc_token_t *self = malloc(sizeof(lcc_token_t));
    self->prev = self;
    self->next = self;
    self->type = LCC_TK_LITERAL;
    self->literal.type = LCC_LT_STRING;
    self->literal.v_string = value;
    return self;
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

typedef struct __lcc_directive_bits_t
{
    long bits;
    const char *name;
} _lcc_directive_bits_t;

static const _lcc_directive_bits_t DIRECTIVES[] = {
    { LCC_LXDN_INCLUDE , "include" },
    { LCC_LXDN_DEFINE  , "define"  },
    { LCC_LXDN_UNDEF   , "undef"   },
    { LCC_LXDN_IF      , "if"      },
    { LCC_LXDN_IFDEF   , "ifdef"   },
    { LCC_LXDN_IFNDEF  , "ifndef"  },
    { LCC_LXDN_ELSE    , "else"    },
    { LCC_LXDN_ENDIF   , "endif"   },
    { LCC_LXDN_PRAGMA  , "pragma"  },
    { 0                , NULL      },
};

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
    /* check for EOF and EOL flags */
    if ((self->flags & LCC_LXF_END))
    {
        /* check for sub-state */
        switch (self->substate)
        {
            /* in the middle of nothing, just ignore it */
            case LCC_LX_SUBSTATE_NULL:
                break;

            /* in the middle of parsing identifiers, accept or commit it */
            case LCC_LX_SUBSTATE_NAME:
            {
                /* convert to string */
                char *p = self->token_buffer.buf;
                size_t n = self->token_buffer.len;
                lcc_string_t *s = lcc_string_from_buffer(p, n);

                /* attach to token chain */
                lcc_token_attach(&(self->tokens), lcc_token_from_ident(s));
                lcc_token_buffer_reset(&(self->token_buffer));
                break;
            }

            /* in the middle of parsing numbers, check for number suffix */
            case LCC_LX_SUBSTATE_NUMBER:
            {
                // TODO: check number suffix
                break;
            }

            /* in the middle of parsing chars or strings, definately an error */
            case LCC_LX_SUBSTATE_CHARS:
            case LCC_LX_SUBSTATE_STRING:
            case LCC_LX_SUBSTATE_CHARS_ESCAPE:
            case LCC_LX_SUBSTATE_STRING_ESCAPE:
            {
                _lcc_lexer_error(self, lcc_string_from_format("Unexpected %s", self->flags & LCC_LXF_EOF ? "EOF" : "EOL"));
                return;
            }

            /* in the middle of parsing operators, accept or commit what already got */
            case LCC_LX_SUBSTATE_OPERATOR_PLUS    : lcc_token_attach(&(self->tokens), lcc_token_from_operator(LCC_OP_PLUS)); break;
            case LCC_LX_SUBSTATE_OPERATOR_MINUS   : lcc_token_attach(&(self->tokens), lcc_token_from_operator(LCC_OP_MINUS)); break;
            case LCC_LX_SUBSTATE_OPERATOR_STAR    : lcc_token_attach(&(self->tokens), lcc_token_from_operator(LCC_OP_STAR)); break;
            case LCC_LX_SUBSTATE_OPERATOR_SLASH   : lcc_token_attach(&(self->tokens), lcc_token_from_operator(LCC_OP_SLASH)); break;
            case LCC_LX_SUBSTATE_OPERATOR_PERCENT : lcc_token_attach(&(self->tokens), lcc_token_from_operator(LCC_OP_PERCENT)); break;
            case LCC_LX_SUBSTATE_OPERATOR_EQU     : lcc_token_attach(&(self->tokens), lcc_token_from_operator(LCC_OP_ASSIGN)); break;
            case LCC_LX_SUBSTATE_OPERATOR_GT      : lcc_token_attach(&(self->tokens), lcc_token_from_operator(LCC_OP_GT)); break;
            case LCC_LX_SUBSTATE_OPERATOR_GT_GT   : lcc_token_attach(&(self->tokens), lcc_token_from_operator(LCC_OP_BSHR)); break;
            case LCC_LX_SUBSTATE_OPERATOR_LT      : lcc_token_attach(&(self->tokens), lcc_token_from_operator(LCC_OP_LT)); break;
            case LCC_LX_SUBSTATE_OPERATOR_LT_LT   : lcc_token_attach(&(self->tokens), lcc_token_from_operator(LCC_OP_BSHL)); break;
            case LCC_LX_SUBSTATE_OPERATOR_EXCL    : lcc_token_attach(&(self->tokens), lcc_token_from_operator(LCC_OP_LNOT)); break;
            case LCC_LX_SUBSTATE_OPERATOR_AMP     : lcc_token_attach(&(self->tokens), lcc_token_from_operator(LCC_OP_BAND)); break;
            case LCC_LX_SUBSTATE_OPERATOR_BAR     : lcc_token_attach(&(self->tokens), lcc_token_from_operator(LCC_OP_BOR)); break;
            case LCC_LX_SUBSTATE_OPERATOR_CARET   : lcc_token_attach(&(self->tokens), lcc_token_from_operator(LCC_OP_BXOR)); break;
        }

        /* EOF or EOL when parsing compiler directive, commit it */
        if (self->flags & LCC_LXF_DIRECTIVE)
            self->state = LCC_LX_STATE_COMMIT;
        else
            self->state = LCC_LX_STATE_ACCEPT;

        /* clear those flags */
        self->flags &= ~LCC_LXF_END;
        return;
    }

    /* check for sub-state */
    switch (self->substate)
    {
        /* initial sub-state */
        case LCC_LX_SUBSTATE_NULL:
        {
            /* check for character */
            switch (self->ch)
            {
                /* identifiers */
                case '_':
                case 'a' ... 'z':
                case 'A' ... 'Z':
                {
                    self->substate = LCC_LX_SUBSTATE_NAME;
                    lcc_token_buffer_append(&(self->token_buffer), self->ch);
                    break;
                }

                /* chars */
                case '\'':
                {
                    self->substate = LCC_LX_SUBSTATE_CHARS;
                    break;
                }

                /* numbers */
                case '.':
                case '0' ... '9':
                {
                    self->substate = LCC_LX_SUBSTATE_NUMBER;
                    lcc_token_buffer_append(&(self->token_buffer), self->ch);
                    break;
                }

                /* strings */
                case '"':
                {
                    self->substate = LCC_LX_SUBSTATE_STRING;
                    break;
                }

                /* single-character operators */
                case '~':
                case '(':
                case ')':
                case '[':
                case ']':
                case '{':
                case '}':
                case ':':
                case ',':
                case ';':
                case '?':
                {
#pragma clang diagnostic push
#pragma ide diagnostic ignored "missing_default_case"

                    /* assign operators */
                    switch (self->ch)
                    {
                        case '~': lcc_token_attach(&(self->tokens), lcc_token_from_operator(LCC_OP_BINV)); break;
                        case '(': lcc_token_attach(&(self->tokens), lcc_token_from_operator(LCC_OP_LBRACKET)); break;
                        case ')': lcc_token_attach(&(self->tokens), lcc_token_from_operator(LCC_OP_RBRACKET)); break;
                        case '[': lcc_token_attach(&(self->tokens), lcc_token_from_operator(LCC_OP_LINDEX)); break;
                        case ']': lcc_token_attach(&(self->tokens), lcc_token_from_operator(LCC_OP_RINDEX)); break;
                        case '{': lcc_token_attach(&(self->tokens), lcc_token_from_operator(LCC_OP_LBLOCK)); break;
                        case '}': lcc_token_attach(&(self->tokens), lcc_token_from_operator(LCC_OP_RBLOCK)); break;
                        case ':': lcc_token_attach(&(self->tokens), lcc_token_from_operator(LCC_OP_COLON)); break;
                        case ',': lcc_token_attach(&(self->tokens), lcc_token_from_operator(LCC_OP_COMMA)); break;
                        case ';': lcc_token_attach(&(self->tokens), lcc_token_from_operator(LCC_OP_SEMICOLON)); break;
                        case '?': lcc_token_attach(&(self->tokens), lcc_token_from_operator(LCC_OP_QUESTION)); break;
                    }

#pragma clang diagnostic pop

                    /* for sure this is the only character */
                    self->state = LCC_LX_STATE_ACCEPT;
                    return;
                }

                /* multi-character operators */
                case '+': self->substate = LCC_LX_SUBSTATE_OPERATOR_PLUS; break;
                case '-': self->substate = LCC_LX_SUBSTATE_OPERATOR_MINUS; break;
                case '*': self->substate = LCC_LX_SUBSTATE_OPERATOR_STAR; break;
                case '/': self->substate = LCC_LX_SUBSTATE_OPERATOR_SLASH; break;
                case '%': self->substate = LCC_LX_SUBSTATE_OPERATOR_PERCENT; break;
                case '=': self->substate = LCC_LX_SUBSTATE_OPERATOR_EQU; break;
                case '>': self->substate = LCC_LX_SUBSTATE_OPERATOR_GT; break;
                case '<': self->substate = LCC_LX_SUBSTATE_OPERATOR_LT; break;
                case '!': self->substate = LCC_LX_SUBSTATE_OPERATOR_EXCL; break;
                case '&': self->substate = LCC_LX_SUBSTATE_OPERATOR_AMP; break;
                case '|': self->substate = LCC_LX_SUBSTATE_OPERATOR_BAR; break;
                case '^': self->substate = LCC_LX_SUBSTATE_OPERATOR_CARET; break;

                /* other characters */
                default:
                {
                    /* whitespaces, skip it */
                    if (isspace(self->ch))
                        break;

                    /* GNU ext :: dollar in identifier */
                    if ((self->ch == '$') && (self->gnuext & LCC_LX_GNUX_DOLLAR_IDENT))
                    {
                        self->substate = LCC_LX_SUBSTATE_NAME;
                        lcc_token_buffer_append(&(self->token_buffer), self->ch);
                        break;
                    }

                    /* other unknown characters */
                    if (isprint(self->ch))
                        _lcc_lexer_error(self, lcc_string_from_format("Invalid character '%c'", self->ch));
                    else
                        _lcc_lexer_error(self, lcc_string_from_format("Invalid character '\\x%.2x'", (uint8_t)(self->ch)));

                    return;
                }
            }

            /* continue shifting */
            self->state = LCC_LX_STATE_SHIFT;
            break;
        }

        /* identifiers */
        case LCC_LX_SUBSTATE_NAME:
        {
            /* it's a valid identifier */
            if ((self->ch == '_') ||
                ((self->ch >= 'a') && (self->ch <= 'z')) ||
                ((self->ch >= 'A') && (self->ch <= 'Z')) ||
                ((self->ch >= '0') && (self->ch <= '9')) ||
                ((self->ch == '$') && (self->gnuext & LCC_LX_GNUX_DOLLAR_IDENT)))
            {
                self->state = LCC_LX_STATE_SHIFT;
                lcc_token_buffer_append(&(self->token_buffer), self->ch);
                break;
            }

            /* convert to string */
            char *p = self->token_buffer.buf;
            size_t n = self->token_buffer.len;
            lcc_string_t *s = lcc_string_from_buffer(p, n);

            /* attach to token chain */
            lcc_token_attach(&(self->tokens), lcc_token_from_ident(s));
            lcc_token_buffer_reset(&(self->token_buffer));

            /* keep this character */
            self->state = LCC_LX_STATE_ACCEPT_KEEP;
            break;
        }

        // TODO: implement chars number string
        case LCC_LX_SUBSTATE_CHARS:
        case LCC_LX_SUBSTATE_STRING:
        case LCC_LX_SUBSTATE_NUMBER:
        {
            self->state = LCC_LX_STATE_SHIFT;
            lcc_token_buffer_reset(&(self->token_buffer));
            break;
        }

        /** very complex operator logic **/

        #define ACCEPT1_OR_KEEP(expect, ac_token, keep_token) {                             \
            if (self->ch == (expect))                                                       \
            {                                                                               \
                self->state = LCC_LX_STATE_ACCEPT;                                          \
                lcc_token_attach(&(self->tokens), lcc_token_from_operator(ac_token));       \
            }                                                                               \
            else                                                                            \
            {                                                                               \
                self->state = LCC_LX_STATE_ACCEPT_KEEP;                                     \
                lcc_token_attach(&(self->tokens), lcc_token_from_operator(keep_token));     \
            }                                                                               \
                                                                                            \
            break;                                                                          \
        }

        #define ACCEPT2_OR_KEEP(expect1, ac_token1, expect2, ac_token2, keep_token) {       \
            switch (self->ch)                                                               \
            {                                                                               \
                case expect1:                                                               \
                {                                                                           \
                    self->state = LCC_LX_STATE_ACCEPT;                                      \
                    lcc_token_attach(&(self->tokens), lcc_token_from_operator(ac_token1));  \
                    break;                                                                  \
                }                                                                           \
                                                                                            \
                case expect2:                                                               \
                {                                                                           \
                    self->state = LCC_LX_STATE_ACCEPT;                                      \
                    lcc_token_attach(&(self->tokens), lcc_token_from_operator(ac_token2));  \
                    break;                                                                  \
                }                                                                           \
                                                                                            \
                default:                                                                    \
                {                                                                           \
                    self->state = LCC_LX_STATE_ACCEPT_KEEP;                                 \
                    lcc_token_attach(&(self->tokens), lcc_token_from_operator(keep_token)); \
                    break;                                                                  \
                }                                                                           \
            }                                                                               \
                                                                                            \
            break;                                                                          \
        }

        #define SHIFT_ACCEPT_OR_KEEP(expect1, shift, expect2, ac_token, keep_token) {       \
            switch (self->ch)                                                               \
            {                                                                               \
                case expect1:                                                               \
                {                                                                           \
                    self->state = LCC_LX_STATE_SHIFT;                                       \
                    self->substate = shift;                                                 \
                    break;                                                                  \
                }                                                                           \
                                                                                            \
                case expect2:                                                               \
                {                                                                           \
                    self->state = LCC_LX_STATE_ACCEPT;                                      \
                    lcc_token_attach(&(self->tokens), lcc_token_from_operator(ac_token));   \
                    break;                                                                  \
                }                                                                           \
                                                                                            \
                default:                                                                    \
                {                                                                           \
                    self->state = LCC_LX_STATE_ACCEPT_KEEP;                                 \
                    lcc_token_attach(&(self->tokens), lcc_token_from_operator(keep_token)); \
                    break;                                                                  \
                }                                                                           \
            }                                                                               \
                                                                                            \
            break;                                                                          \
        }

        /* one- or two-character operators */
        case LCC_LX_SUBSTATE_OPERATOR_STAR    : ACCEPT1_OR_KEEP('=', LCC_OP_IMUL, LCC_OP_STAR   )   /* * *= */
        case LCC_LX_SUBSTATE_OPERATOR_SLASH   : ACCEPT1_OR_KEEP('=', LCC_OP_IDIV, LCC_OP_SLASH  )   /* / /= */
        case LCC_LX_SUBSTATE_OPERATOR_PERCENT : ACCEPT1_OR_KEEP('=', LCC_OP_IMOD, LCC_OP_PERCENT)   /* % %= */
        case LCC_LX_SUBSTATE_OPERATOR_EQU     : ACCEPT1_OR_KEEP('=', LCC_OP_EQ  , LCC_OP_ASSIGN )   /* = == */
        case LCC_LX_SUBSTATE_OPERATOR_EXCL    : ACCEPT1_OR_KEEP('=', LCC_OP_NEQ , LCC_OP_LNOT   )   /* ! != */
        case LCC_LX_SUBSTATE_OPERATOR_AMP     : ACCEPT1_OR_KEEP('=', LCC_OP_IAND, LCC_OP_BAND   )   /* & &= */
        case LCC_LX_SUBSTATE_OPERATOR_BAR     : ACCEPT1_OR_KEEP('=', LCC_OP_IOR , LCC_OP_BOR    )   /* | |= */
        case LCC_LX_SUBSTATE_OPERATOR_CARET   : ACCEPT1_OR_KEEP('=', LCC_OP_IXOR, LCC_OP_BXOR   )   /* ^ ^= */

        /* one- or two-character and incremental operators */
        case LCC_LX_SUBSTATE_OPERATOR_PLUS    : ACCEPT2_OR_KEEP('=', LCC_OP_IADD, '+', LCC_OP_INCR, LCC_OP_PLUS )   /* + ++ += */
        case LCC_LX_SUBSTATE_OPERATOR_MINUS   : ACCEPT2_OR_KEEP('=', LCC_OP_ISUB, '-', LCC_OP_DECR, LCC_OP_MINUS)   /* - -- -= */

        /* shift or comparison operators */
        case LCC_LX_SUBSTATE_OPERATOR_GT      : SHIFT_ACCEPT_OR_KEEP('>', LCC_LX_SUBSTATE_OPERATOR_GT_GT, '=', LCC_OP_GEQ, LCC_OP_GT)   /* > >> >= >>= */
        case LCC_LX_SUBSTATE_OPERATOR_LT      : SHIFT_ACCEPT_OR_KEEP('>', LCC_LX_SUBSTATE_OPERATOR_LT_LT, '=', LCC_OP_LEQ, LCC_OP_LT)   /* < << <= <<= */

        /* bit-shifting operators */
        case LCC_LX_SUBSTATE_OPERATOR_GT_GT   : ACCEPT1_OR_KEEP('=', LCC_OP_ISHR, LCC_OP_BSHR)  /* >> >>= */
        case LCC_LX_SUBSTATE_OPERATOR_LT_LT   : ACCEPT1_OR_KEEP('=', LCC_OP_ISHL, LCC_OP_BSHL)  /* << <<= */

        #undef ACCEPT1_OR_KEEP
        #undef ACCEPT2_OR_KEEP
        #undef SHIFT_ACCEPT_OR_KEEP
    }
}

static void _lcc_handle_directive(lcc_lexer_t *self)
{
    /* update directive name */
    if (!(self->flags & LCC_LXDN_MASK))
    {
        /* get the directive name token */
        lcc_string_t *name = self->tokens.prev->ident;
        const _lcc_directive_bits_t *pb;

        /* find directive by name */
        for (pb = DIRECTIVES; pb->name; pb++)
            if (!(strcmp(pb->name, name->buf)))
                break;

        /* no such directive */
        if (!(pb->name))
        {
            _lcc_lexer_error(self, lcc_string_from_format("Unknown compiler directive \"%s\"", name->buf));
            return;
        }

        /* set the name bit, and remove the token */
        self->flags |= pb->bits;
        lcc_token_free(self->tokens.prev);
    }
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
    /* release all chained tokens */
    while (self->tokens.next != &(self->tokens))
        lcc_token_free(self->tokens.next);

    /* clear other tables */
    lcc_array_free(&(self->files));
    lcc_token_buffer_free(&(self->token_buffer));
    lcc_string_array_free(&(self->include_paths));
    lcc_string_array_free(&(self->library_paths));
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

    /* token list and token buffer */
    lcc_token_init(&(self->tokens));
    lcc_token_buffer_init(&(self->token_buffer));

    /* initial lexer state */
    self->ch = 0;
    self->file = lcc_array_top(&(self->files));
    self->state = LCC_LX_STATE_INIT;
    self->substate = LCC_LX_SUBSTATE_NULL;

    /* initial flags and GNU extensions */
    self->flags = 0;
    self->gnuext = 0;

    /* default error handling */
    self->error_fn = _lcc_error_default;
    self->error_data = NULL;
    return 1;
}

char lcc_lexer_next(lcc_lexer_t *self)
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
                            self->state = LCC_LX_STATE_GOT_CHAR;
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
                        self->state = (self->file->flags & LCC_FF_LNODIR) ? LCC_LX_STATE_GOT_CHAR : LCC_LX_STATE_GOT_DIRECTIVE;
                        break;
                    }

                    /* generic character */
                    default:
                    {
                        self->state = LCC_LX_STATE_GOT_CHAR;
                        break;
                    }
                }

                break;
            }

            /* pop file from file stack */
            case LCC_LX_STATE_POP_FILE:
            {
                /* set EOF flags to flush the preprocessor */
                self->state = LCC_LX_STATE_GOT_CHAR;
                self->flags |= LCC_LXF_EOF;

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
                /* set EOL flags to flush the preprocessor */
                self->state = LCC_LX_STATE_GOT_CHAR;
                self->flags |= LCC_LXF_EOL;

                /* move to next line */
                self->file->row++;
                self->file->col = 0;
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

            /* we got a new character */
            case LCC_LX_STATE_GOT_CHAR:
            {
                /* not a space, prohibit compiler directive on this line */
                if (!(isspace(self->ch)))
                    self->file->flags |= LCC_FF_LNODIR;

                /* handle all sub-states */
                _lcc_handle_substate(self);
                break;
            }

            /* we got a compiler directive */
            case LCC_LX_STATE_GOT_DIRECTIVE:
            {
                self->state = LCC_LX_STATE_SHIFT;
                self->flags |= LCC_LXF_DIRECTIVE;
                self->file->flags |= LCC_FF_LNODIR;
                break;
            }

            /* commit parsed compiler directive */
            case LCC_LX_STATE_COMMIT:
            {
                printf("** commit directive %.16lx\n", self->flags);
                for (lcc_token_t *t = self->tokens.next; t != &(self->tokens); t = t->next)
                {
                    if (t->type == LCC_TK_IDENT)
                        printf("ident %p: %s\n", t, t->ident->buf);
                    else
                        printf("oper  %p: %d\n", t, t->operator);
                }
                printf("** commit directive done\n");

                /* release all chained tokens */
                while (self->tokens.next != &(self->tokens))
                    lcc_token_free(self->tokens.next);

                /* clear directive flags */
                self->flags &= LCC_LXF_MASK;
                self->flags &= ~LCC_LXF_DIRECTIVE;
                self->file->flags &= ~LCC_FF_LNODIR;

                /* reset state and sub-state */
                self->state = LCC_LX_STATE_SHIFT;
                self->substate = LCC_LX_SUBSTATE_NULL;
                break;
            }

            /* token accepted */
            case LCC_LX_STATE_ACCEPT:
            case LCC_LX_STATE_ACCEPT_KEEP:
            {
                /* shift next character if needed */
                self->state = (self->state == LCC_LX_STATE_ACCEPT) ? LCC_LX_STATE_SHIFT : LCC_LX_STATE_GOT_CHAR;
                self->substate = LCC_LX_SUBSTATE_NULL;

                /* still parsing directives, don't accept now */
                if (self->flags & LCC_LXF_DIRECTIVE)
                {
                    _lcc_handle_directive(self);
                    break;
                }

                printf("** accept token\n");
                for (lcc_token_t *t = self->tokens.next; t != &(self->tokens); t = t->next)
                {
                    if (t->type == LCC_TK_IDENT)
                        printf("ident %p: %s\n", t, t->ident->buf);
                    else if (t->type == LCC_TK_OPERATOR)
                        printf("oper  %p: %d\n", t, t->operator);
                    else
                        printf("token %p: %d\n", t, t->type);
                }
                printf("** accept token done\n");

                /* release all chained tokens */
                while (self->tokens.next != &(self->tokens))
                    lcc_token_free(self->tokens.next);

                // TODO: accept token
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

void lcc_lexer_set_gnu_ext(lcc_lexer_t *self, lcc_lexer_gnu_ext_t name, char enabled)
{
    if (enabled)
        self->gnuext |= name;
    else
        self->gnuext &= ~name;
}

void lcc_lexer_set_error_handler(lcc_lexer_t *self, lcc_lexer_on_error_fn error_fn, void *data)
{
    self->error_fn = error_fn;
    self->error_data = data;
}
