#include <ctype.h>
#include <errno.h>
#include <libgen.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "lcc_lexer.h"

/*** Tokens ***/

static inline int _lcc_hex_value(char hex)
{
    if ((hex >= '0') && (hex <= '9'))
        return hex - '0';
    else if ((hex >= 'a') && (hex <= 'f'))
        return hex - 'a' + 10;
    else
        return hex - 'A' + 10;
}

static lcc_string_t *_lcc_string_eval(lcc_string_t *self, char allow_gnuext)
{
    /* string boundary */
    char *p = self->buf;
    char *q = self->buf;
    char *e = self->buf + self->len;

    /* process the entire string */
    while (p < e)
    {
        /* not an escape character, copy as is */
        if (*p != '\\')
        {
            *q++ = *p++;
            continue;
        }

        /* skip the '\\' character */
        switch (*++p)
        {
            /* these characters should be copied as is */
            case '?':
            case '"':
            case '\'':
            case '\\':
            {
                *q++ = *p++;
                break;
            }

            /* single-character escape sequence */
            case 'a': { p++; *q++ = '\a'; break; }
            case 'b': { p++; *q++ = '\b'; break; }
            case 'f': { p++; *q++ = '\f'; break; }
            case 'n': { p++; *q++ = '\n'; break; }
            case 'r': { p++; *q++ = '\r'; break; }
            case 't': { p++; *q++ = '\t'; break; }
            case 'v': { p++; *q++ = '\v'; break; }

            /* hexadecimal escape characters */
            case 'x':
            {
                /* parse exact 2 digits */
                int msb = _lcc_hex_value(*++p);
                int lsb = _lcc_hex_value(*++p);

                /* convert to character */
                p++;
                *q++ = (char)((msb << 4) | lsb);
                break;
            }

            /* octal escape characters */
            case '0' ... '7':
            {
                /* valid range of octal characters
                 * in 8-bit ASCII is '\000' ~ '\377' */
                int val = 0;
                int count = ((*p <= '3') ? 1 : 0) + 2;

                /* parse at most `count` digits */
                for (int i = 0; (i < count) && (*p >= '0') && (*p <= '7'); i++)
                {
                    val <<= 3;
                    val |= (*p++ - '0') & 0x07;
                }

                /* convert to character */
                *q++ = (char)val;
                break;
            }

            /* other characters */
            default:
            {
                /* might be GNU extension escape character */
                if ((*p == 'e') && allow_gnuext)
                {
                    p++;
                    *q++ = '\033';
                    break;
                }

                /* otherwise it's an error */
                fprintf(stderr, "*** FATAL: unrecognized escape character '\\x%02x'\n", (uint8_t)(*p));
                abort();
            }
        }
    }

    /* end the result string */
    *q = 0;
    self->len = q - p;
    return self;
}

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

                lcc_string_unref(self->literal.raw);
                break;
            }

            case LCC_TK_KEYWORD:
            case LCC_TK_OPERATOR:
                break;
        }

        lcc_token_detach(self);
        lcc_string_unref(self->src);
        free(self);
    }
}

void lcc_token_init(lcc_token_t *self)
{
    self->ref = 0;
    self->src = NULL;
    self->prev = self;
    self->next = self;
    self->type = LCC_TK_EOF;
}

void lcc_token_clear(lcc_token_t *self)
{
    /* clear all tokens on the chain */
    while (self->next != self)
        lcc_token_free(self->next);

    /* and the header node */
    lcc_token_free(self);
}

void lcc_token_attach(lcc_token_t *self, lcc_token_t *tail)
{
    tail->next = self;
    tail->prev = self->prev;
    self->prev->next = tail;
    self->prev = tail;
}

lcc_token_t *lcc_token_new(void)
{
    lcc_token_t *self = malloc(sizeof(lcc_token_t));
    self->ref = 1;
    self->src = lcc_string_new(0);
    self->prev = self;
    self->next = self;
    self->type = LCC_TK_EOF;
    return self;
}

lcc_token_t *lcc_token_copy(lcc_token_t *self)
{
    /* create a new token */
    lcc_token_t *clone = malloc(sizeof(lcc_token_t));

    /* clone by type */
    switch (self->type)
    {
        /* EOF, nothing more to clone */
        case LCC_TK_EOF:
            break;

        /* identifiers */
        case LCC_TK_IDENT:
        {
            clone->ident = lcc_string_copy(self->ident);
            break;
        }

        /* literals */
        case LCC_TK_LITERAL:
        {
            /* copy literal by type */
            switch (self->literal.type)
            {
                /* primitive types */
                case LCC_LT_INT         : clone->literal.v_int        = self->literal.v_int;        break;
                case LCC_LT_LONG        : clone->literal.v_long       = self->literal.v_long;       break;
                case LCC_LT_LONGLONG    : clone->literal.v_longlong   = self->literal.v_longlong;   break;
                case LCC_LT_UINT        : clone->literal.v_uint       = self->literal.v_uint;       break;
                case LCC_LT_ULONG       : clone->literal.v_ulong      = self->literal.v_ulong;      break;
                case LCC_LT_ULONGLONG   : clone->literal.v_ulonglong  = self->literal.v_ulonglong;  break;
                case LCC_LT_FLOAT       : clone->literal.v_float      = self->literal.v_float;      break;
                case LCC_LT_DOUBLE      : clone->literal.v_double     = self->literal.v_double;     break;
                case LCC_LT_LONGDOUBLE  : clone->literal.v_longdouble = self->literal.v_longdouble; break;

                /* character sequence and strings */
                case LCC_LT_CHAR        : clone->literal.v_char       = lcc_string_copy(self->literal.v_char  ); break;
                case LCC_LT_STRING      : clone->literal.v_string     = lcc_string_copy(self->literal.v_string); break;
            }

            /* also copy the raw value */
            clone->literal.raw = lcc_string_copy(self->literal.raw);
            clone->literal.type = self->literal.type;
            break;
        }

        /* keywords */
        case LCC_TK_KEYWORD:
        {
            clone->keyword = self->keyword;
            break;
        }

        /* operators */
        case LCC_TK_OPERATOR:
        {
            clone->operator = self->operator;
            break;
        }
    }

    /* set the new token type */
    clone->ref = self->ref;
    clone->src = lcc_string_copy(self->src);
    clone->prev = clone;
    clone->next = clone;
    clone->type = self->type;
    return clone;
}

lcc_token_t *lcc_token_detach(lcc_token_t *self)
{
    self->prev->next = self->next;
    self->next->prev = self->prev;
    self->prev = self;
    self->next = self;
    return self;
}

lcc_token_t *lcc_token_from_ident(lcc_string_t *src, lcc_string_t *ident)
{
    lcc_token_t *self = malloc(sizeof(lcc_token_t));
    self->ref = 0;
    self->src = src;
    self->prev = self;
    self->next = self;
    self->type = LCC_TK_IDENT;
    self->ident = ident;
    return self;
}

lcc_token_t *lcc_token_from_keyword(lcc_string_t *src, lcc_keyword_t keyword)
{
    lcc_token_t *self = malloc(sizeof(lcc_token_t));
    self->ref = 0;
    self->src = src;
    self->prev = self;
    self->next = self;
    self->type = LCC_TK_KEYWORD;
    self->keyword = keyword;
    return self;
}

lcc_token_t *lcc_token_from_operator(lcc_string_t *src, lcc_operator_t operator)
{
    lcc_token_t *self = malloc(sizeof(lcc_token_t));
    self->ref = 0;
    self->src = src;
    self->prev = self;
    self->next = self;
    self->type = LCC_TK_OPERATOR;
    self->operator = operator;
    return self;
}

lcc_token_t *lcc_token_from_raw(lcc_string_t *src, lcc_string_t *value)
{
    lcc_token_t *self = malloc(sizeof(lcc_token_t));
    self->ref = 0;
    self->src = src;
    self->prev = self;
    self->next = self;
    self->type = LCC_TK_LITERAL;
    self->literal.raw = value;
    self->literal.type = LCC_LT_STRING;
    self->literal.v_string = lcc_string_copy(value);
    return self;
}

lcc_token_t *lcc_token_from_char(lcc_string_t *src, lcc_string_t *value, char allow_gnuext)
{
    lcc_token_t *self = malloc(sizeof(lcc_token_t));
    self->ref = 0;
    self->src = src;
    self->prev = self;
    self->next = self;
    self->type = LCC_TK_LITERAL;
    self->literal.raw = value;
    self->literal.type = LCC_LT_CHAR;
    self->literal.v_char = _lcc_string_eval(lcc_string_copy(value), allow_gnuext);
    return self;
}

lcc_token_t *lcc_token_from_string(lcc_string_t *src, lcc_string_t *value, char allow_gnuext)
{
    lcc_token_t *self = malloc(sizeof(lcc_token_t));
    self->ref = 0;
    self->src = src;
    self->prev = self;
    self->next = self;
    self->type = LCC_TK_LITERAL;
    self->literal.raw = value;
    self->literal.type = LCC_LT_STRING;
    self->literal.v_string = _lcc_string_eval(lcc_string_copy(value), allow_gnuext);
    return self;
}

#define _LCC_NUM_BASE(num) \
    ((num[0] != '0') ? 10 : ((num[1] == 'x') || (num[1] == 'X')) ? 16 : 8)

lcc_token_t *lcc_token_from_number(lcc_string_t *src, lcc_string_t *value, lcc_literal_type_t type)
{
    /* create a new token */
    const char *num = value->buf;
    lcc_token_t *self = malloc(sizeof(lcc_token_t));

    /* set as literal */
    errno = 0;
    self->ref = 0;
    self->src = src;
    self->prev = self;
    self->next = self;
    self->type = LCC_TK_LITERAL;

    /* parse each type of numbers */
    switch (type)
    {
        /* integers */
        case LCC_LT_INT        :
        case LCC_LT_LONG       : self->literal.v_long       = strtol  (num, NULL, _LCC_NUM_BASE(num)); break;
        case LCC_LT_LONGLONG   : self->literal.v_longlong   = strtoll (num, NULL, _LCC_NUM_BASE(num)); break;
        case LCC_LT_UINT       :
        case LCC_LT_ULONG      : self->literal.v_ulong      = strtoul (num, NULL, _LCC_NUM_BASE(num)); break;
        case LCC_LT_ULONGLONG  : self->literal.v_ulonglong  = strtoull(num, NULL, _LCC_NUM_BASE(num)); break;

        /* floats */
        case LCC_LT_FLOAT      : self->literal.v_float      = strtof  (num, NULL); break;
        case LCC_LT_DOUBLE     : self->literal.v_double     = strtod  (num, NULL); break;
        case LCC_LT_LONGDOUBLE : self->literal.v_longdouble = strtold (num, NULL); break;

        /* cannot happen */
        case LCC_LT_CHAR:
        case LCC_LT_STRING:
        {
            fprintf(stderr, "*** FATAL: not a number\n");
            abort();
        }
    }

    /* preserve raw value */
    self->literal.raw = value;
    self->literal.type = type;
    return self;
}

#undef _LCC_NUM_BASE

const char *lcc_token_kw_name(lcc_keyword_t value)
{
    switch (value)
    {
        case LCC_KW_AUTO      : return "auto";
        case LCC_KW_BOOL      : return "_Bool";
        case LCC_KW_BREAK     : return "break";
        case LCC_KW_CASE      : return "case";
        case LCC_KW_CHAR      : return "char";
        case LCC_KW_COMPLEX   : return "complex";
        case LCC_KW_CONST     : return "const";
        case LCC_KW_CONTINUE  : return "continue";
        case LCC_KW_DEFAULT   : return "default";
        case LCC_KW_DO        : return "do";
        case LCC_KW_DOUBLE    : return "double";
        case LCC_KW_ELSE      : return "else";
        case LCC_KW_ENUM      : return "enum";
        case LCC_KW_EXTERN    : return "extern";
        case LCC_KW_FLOAT     : return "float";
        case LCC_KW_FOR       : return "for";
        case LCC_KW_GOTO      : return "goto";
        case LCC_KW_IF        : return "if";
        case LCC_KW_IMAGINARY : return "_Imaginary";
        case LCC_KW_INLINE    : return "inline";
        case LCC_KW_INT       : return "int";
        case LCC_KW_LONG      : return "long";
        case LCC_KW_REGISTER  : return "register";
        case LCC_KW_RESTRICT  : return "restrict";
        case LCC_KW_RETURN    : return "return";
        case LCC_KW_SHORT     : return "short";
        case LCC_KW_SIGNED    : return "signed";
        case LCC_KW_SIZEOF    : return "sizeof";
        case LCC_KW_STATIC    : return "static";
        case LCC_KW_STRUCT    : return "struct";
        case LCC_KW_SWITCH    : return "switch";
        case LCC_KW_TYPEDEF   : return "typedef";
        case LCC_KW_UNION     : return "union";
        case LCC_KW_UNSIGNED  : return "unsigned";
        case LCC_KW_VOID      : return "void";
        case LCC_KW_VOLATILE  : return "volatile";
        case LCC_KW_WHILE     : return "while";
    }

    abort();
}

const char *lcc_token_op_name(lcc_operator_t value)
{
    switch (value)
    {
        case LCC_OP_PLUS      : return "+";
        case LCC_OP_MINUS     : return "-";
        case LCC_OP_STAR      : return "*";
        case LCC_OP_SLASH     : return "/";
        case LCC_OP_PERCENT   : return "%";
        case LCC_OP_INCR      : return "++";
        case LCC_OP_DECR      : return "--";
        case LCC_OP_EQ        : return "==";
        case LCC_OP_GT        : return ">";
        case LCC_OP_LT        : return "<";
        case LCC_OP_NEQ       : return "!=";
        case LCC_OP_GEQ       : return ">=";
        case LCC_OP_LEQ       : return "<=";
        case LCC_OP_LAND      : return "&&";
        case LCC_OP_LOR       : return "||";
        case LCC_OP_LNOT      : return "!";
        case LCC_OP_BAND      : return "&";
        case LCC_OP_BOR       : return "|";
        case LCC_OP_BXOR      : return "^";
        case LCC_OP_BINV      : return "~";
        case LCC_OP_BSHL      : return "<<";
        case LCC_OP_BSHR      : return ">>";
        case LCC_OP_ASSIGN    : return "=";
        case LCC_OP_IADD      : return "+=";
        case LCC_OP_ISUB      : return "-=";
        case LCC_OP_IMUL      : return "*=";
        case LCC_OP_IDIV      : return "/=";
        case LCC_OP_IMOD      : return "%=";
        case LCC_OP_ISHL      : return "<<";
        case LCC_OP_ISHR      : return ">>";
        case LCC_OP_IAND      : return "&=";
        case LCC_OP_IXOR      : return "^=";
        case LCC_OP_IOR       : return "|=";
        case LCC_OP_QUESTION  : return "?";
        case LCC_OP_LBRACKET  : return "(";
        case LCC_OP_RBRACKET  : return ")";
        case LCC_OP_LINDEX    : return "[";
        case LCC_OP_RINDEX    : return "]";
        case LCC_OP_LBLOCK    : return "{";
        case LCC_OP_RBLOCK    : return "}";
        case LCC_OP_COLON     : return ":";
        case LCC_OP_COMMA     : return ",";
        case LCC_OP_POINT     : return ".";
        case LCC_OP_SEMICOLON : return ";";
        case LCC_OP_DEREF     : return "->";
        case LCC_OP_ELLIPSIS  : return "...";
        case LCC_OP_STRINGIZE : return "#";
        case LCC_OP_CONCAT    : return "##";
    }

    abort();
}

lcc_string_t *lcc_token_as_string(lcc_token_t *self)
{
    switch (self->type)
    {
        case LCC_TK_EOF      : return lcc_string_from("<EOF>");
        case LCC_TK_IDENT    : return lcc_string_ref(self->ident);
        case LCC_TK_LITERAL  : return lcc_string_ref(self->literal.raw);
        case LCC_TK_KEYWORD  : return lcc_string_from(lcc_token_kw_name(self->keyword));
        case LCC_TK_OPERATOR : return lcc_string_from(lcc_token_op_name(self->operator));
    }

    abort();
}

lcc_string_t *lcc_token_to_string(lcc_token_t *self)
{
    switch (self->type)
    {
        case LCC_TK_EOF      : return lcc_string_from("{EOF}");
        case LCC_TK_IDENT    : return lcc_string_from_format("{ID:%s}", self->ident->buf);
        case LCC_TK_KEYWORD  : return lcc_string_from_format("{KW:%s}", lcc_token_kw_name(self->keyword));
        case LCC_TK_OPERATOR : return lcc_string_from_format("{OP:%s}", lcc_token_op_name(self->operator));
        case LCC_TK_LITERAL  :
        {
            switch (self->literal.type)
            {
                case LCC_LT_INT        : return lcc_string_from_format("{INT:%s}"       , self->literal.raw->buf);
                case LCC_LT_LONG       : return lcc_string_from_format("{LONG:%s}"      , self->literal.raw->buf);
                case LCC_LT_LONGLONG   : return lcc_string_from_format("{LONGLONG:%s}"  , self->literal.raw->buf);
                case LCC_LT_UINT       : return lcc_string_from_format("{UINT:%s}"      , self->literal.raw->buf);
                case LCC_LT_ULONG      : return lcc_string_from_format("{ULONG:%s}"     , self->literal.raw->buf);
                case LCC_LT_ULONGLONG  : return lcc_string_from_format("{ULONGLONG:%s}" , self->literal.raw->buf);
                case LCC_LT_FLOAT      : return lcc_string_from_format("{FLOAT:%s}"     , self->literal.raw->buf);
                case LCC_LT_DOUBLE     : return lcc_string_from_format("{DOUBLE:%s}"    , self->literal.raw->buf);
                case LCC_LT_LONGDOUBLE : return lcc_string_from_format("{LONGDOUBLE:%s}", self->literal.raw->buf);
                case LCC_LT_CHAR       : return lcc_string_from_format("{CHARS:'%s'}"   , self->literal.raw->buf);
                case LCC_LT_STRING     : return lcc_string_from_format("{STRING:\"%s\"}", self->literal.raw->buf);
            }
        }
    }

    abort();
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

typedef struct __lcc_keyword_item_t
{
    const char *name;
    lcc_keyword_t keyword;
} _lcc_keyword_item_t;

typedef struct __lcc_directive_bits_t
{
    long bits;
    const char *name;
} _lcc_directive_bits_t;

static const _lcc_keyword_item_t KEYWORDS[] = {
    { "auto"       , LCC_KW_AUTO      },
    { "_Bool"      , LCC_KW_BOOL      },
    { "break"      , LCC_KW_BREAK     },
    { "case"       , LCC_KW_CASE      },
    { "char"       , LCC_KW_CHAR      },
    { "_Complex"   , LCC_KW_COMPLEX   },
    { "const"      , LCC_KW_CONST     },
    { "continue"   , LCC_KW_CONTINUE  },
    { "default"    , LCC_KW_DEFAULT   },
    { "do"         , LCC_KW_DO        },
    { "double"     , LCC_KW_DOUBLE    },
    { "else"       , LCC_KW_ELSE      },
    { "enum"       , LCC_KW_ENUM      },
    { "extern"     , LCC_KW_EXTERN    },
    { "float"      , LCC_KW_FLOAT     },
    { "for"        , LCC_KW_FOR       },
    { "goto"       , LCC_KW_GOTO      },
    { "if"         , LCC_KW_IF        },
    { "_Imaginary" , LCC_KW_IMAGINARY },
    { "inline"     , LCC_KW_INLINE    },
    { "int"        , LCC_KW_INT       },
    { "long"       , LCC_KW_LONG      },
    { "register"   , LCC_KW_REGISTER  },
    { "restrict"   , LCC_KW_RESTRICT  },
    { "return"     , LCC_KW_RETURN    },
    { "short"      , LCC_KW_SHORT     },
    { "signed"     , LCC_KW_SIGNED    },
    { "sizeof"     , LCC_KW_SIZEOF    },
    { "static"     , LCC_KW_STATIC    },
    { "struct"     , LCC_KW_STRUCT    },
    { "switch"     , LCC_KW_SWITCH    },
    { "typedef"    , LCC_KW_TYPEDEF   },
    { "union"      , LCC_KW_UNION     },
    { "unsigned"   , LCC_KW_UNSIGNED  },
    { "void"       , LCC_KW_VOID      },
    { "volatile"   , LCC_KW_VOLATILE  },
    { "while"      , LCC_KW_WHILE     },
    { NULL }
};

static const _lcc_directive_bits_t DIRECTIVES[] = {
    { LCC_LXDN_INCLUDE      , "include"         },
    { LCC_LXDN_INCLUDE |
      LCC_LXDF_INCLUDE_NEXT , "include_next"    },
    { LCC_LXDN_DEFINE       , "define"          },
    { LCC_LXDN_UNDEF        , "undef"           },
    { LCC_LXDN_IF           , "if"              },
    { LCC_LXDN_IFDEF        , "ifdef"           },
    { LCC_LXDN_IFNDEF       , "ifndef"          },
    { LCC_LXDN_ELIF         , "elif"            },
    { LCC_LXDN_ELSE         , "else"            },
    { LCC_LXDN_ENDIF        , "endif"           },
    { LCC_LXDN_PRAGMA       , "pragma"          },
    { LCC_LXDN_ERROR        , "error"           },
    { LCC_LXDN_WARNING      , "warning"         },
    { LCC_LXDN_LINE         , "line"            },
    { LCC_LXDN_SCCS         , "sccs"            },
    { 0 },
};

static void _lcc_lexer_error(lcc_lexer_t *self, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
static void _lcc_lexer_error(lcc_lexer_t *self, const char *fmt, ...)
{
    /* invoke the error handler if any */
    if (self->error_fn)
    {
        /* format the error message */
        va_list args;
        va_start(args, fmt);
        lcc_string_t *message = lcc_string_from_format_va(fmt, args);

        /* invoke the error handler */
        self->error_fn(
            self,
            self->fname,
            self->row,
            self->col,
            message,
            LCC_LXET_ERROR,
            self->error_data
        );

        /* release the message string */
        va_end(args);
        lcc_string_unref(message);
    }

    /* advance state machine to REJECT */
    self->state = LCC_LX_STATE_REJECT;
}

static void _lcc_lexer_warning(lcc_lexer_t *self, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
static void _lcc_lexer_warning(lcc_lexer_t *self, const char *fmt, ...)
{
    /* invoke the error handler if any */
    if (self->error_fn)
    {
        /* format the warning message */
        va_list args;
        va_start(args, fmt);
        lcc_string_t *message = lcc_string_from_format_va(fmt, args);

        /* invoke the error handler */
        self->error_fn(
            self,
            self->fname,
            self->row,
            self->col,
            message,
            LCC_LXET_WARNING,
            self->error_data
        );

        /* release the message string */
        va_end(args);
        lcc_string_unref(message);
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

#define _LCC_WRONG_CHAR(msg)                                            \
{                                                                       \
    if (isprint(self->ch))                                              \
        _lcc_lexer_error(self, msg " '%c'", self->ch);                  \
    else                                                                \
        _lcc_lexer_error(self, msg " '\\x%02x'", (uint8_t)(self->ch));  \
}

static inline lcc_string_t *_lcc_dump_token(lcc_lexer_t *self)
{
    char *p = self->token_buffer.buf;
    size_t n = self->token_buffer.len;
    return lcc_string_from_buffer(p, n);
}

static inline lcc_string_t *_lcc_swap_source(lcc_lexer_t *self, char keep_tail)
{
    /* old and new strings */
    lcc_string_t *old = self->source;
    lcc_string_t *new = lcc_string_new(0);

    /* remove last char as needed */
    if (old->len && !keep_tail)
        old->buf[--old->len] = 0;

    /* reset the source buffer */
    self->source = new;
    return old;
}

static inline char _lcc_commit_ident(lcc_lexer_t *self, char keep_tail)
{
    /* check for preprocessor reserved identifiers */
    if ((!(self->flags & LCC_LXDF_DEFINE_VAR) ||                /* must be variadic macro */
          (self->flags & LCC_LXDF_DEFINE_NVAR) ||               /* but must not be named variadic macro */
         !(self->flags & LCC_LXDF_DEFINE_FINE)) &&              /* must finish parsing the declaration */
        (!(strcmp(self->token_buffer.buf, "__VA_OPT__")) ||     /* must not be "__VA_OPT__" */
         !(strcmp(self->token_buffer.buf, "__VA_ARGS__"))))     /* must not be "__VA_ARGS__" */
    {
        _lcc_lexer_error(self, "'%s' is not allowed here", self->token_buffer.buf);
        return 0;
    }

    /* keyword index */
    ssize_t id = -1;
    lcc_token_t *token;
    lcc_string_t *source = _lcc_swap_source(self, keep_tail);

    /* search for keyworkds */
    for (size_t i = 0; KEYWORDS[i].name; i++)
    {
        if (!(strcmp(KEYWORDS[i].name, self->token_buffer.buf)))
        {
            id = i;
            break;
        }
    }

    /* create a new token */
    if (id < 0)
        token = lcc_token_from_ident(source, _lcc_dump_token(self));
    else
        token = lcc_token_from_keyword(source, KEYWORDS[id].keyword);

    /* attach to token chain */
    lcc_token_attach(&(self->tokens), token);
    lcc_token_buffer_reset(&(self->token_buffer));
    return 1;
}

static inline void _lcc_commit_chars(lcc_lexer_t *self, char keep_tail)
{
    /* create a new token */
    lcc_token_t *token = lcc_token_from_char(
        _lcc_swap_source(self, keep_tail),
        _lcc_dump_token(self),
        (self->gnuext & LCC_LX_GNUX_ESCAPE_CHAR) != 0
    );

    /* attach to token chain */
    lcc_token_attach(&(self->tokens), token);
    lcc_token_buffer_reset(&(self->token_buffer));
}

static inline void _lcc_commit_string(lcc_lexer_t *self, char keep_tail)
{
    /* create a new token */
    lcc_token_t *token = lcc_token_from_string(
        _lcc_swap_source(self, keep_tail),
        _lcc_dump_token(self),
        (self->gnuext & LCC_LX_GNUX_ESCAPE_CHAR) != 0
    );

    /* attach to token chain */
    lcc_token_attach(&(self->tokens), token);
    lcc_token_buffer_reset(&(self->token_buffer));
}

static inline void _lcc_commit_number(lcc_lexer_t *self, lcc_literal_type_t type, char keep_tail)
{
    /* create new tokens */
    lcc_token_t *token = lcc_token_from_number(
        _lcc_swap_source(self, keep_tail),
        _lcc_dump_token(self),
        type
    );

    /* check for overflow */
    if (errno == ERANGE)
        _lcc_lexer_warning(self, "Literal %s is out of range", self->token_buffer.buf);

    /* attach to token chain */
    lcc_token_attach(&(self->tokens), token);
    lcc_token_buffer_reset(&(self->token_buffer));
}

static inline void _lcc_commit_operator(lcc_lexer_t *self, lcc_operator_t operator, char keep_tail)
{
    lcc_string_t *src = _lcc_swap_source(self, keep_tail);
    lcc_token_attach(&(self->tokens), lcc_token_from_operator(src, operator));
}

static void _lcc_handle_substate(lcc_lexer_t *self)
{
    /* check for EOF and EOL flags */
    if ((self->flags & LCC_LXF_EOF) ||
        (self->flags & LCC_LXF_EOL))
    {
        /* check for sub-state */
        switch (self->substate)
        {
            /* in the middle of nothing (or comment), just ignore it */
            case LCC_LX_SUBSTATE_NULL:
            case LCC_LX_SUBSTATE_COMMENT_LINE:
                break;

            /* block comment */
            case LCC_LX_SUBSTATE_COMMENT_BLOCK:
            case LCC_LX_SUBSTATE_COMMENT_BLOCK_END:
            {
                /* fire a warning when meets EOF */
                if (self->flags & LCC_LXF_EOF)
                {
                    _lcc_lexer_warning(self, "EOF when parsing block comment");
                    break;
                }

                /* otherwise shift pass the line ending */
                self->state = LCC_LX_STATE_SHIFT;
                self->substate = LCC_LX_SUBSTATE_COMMENT_BLOCK;
                return;
            }

            /* in the middle of parsing identifiers, accept or commit it */
            case LCC_LX_SUBSTATE_NAME:
            {
                if (_lcc_commit_ident(self, 1))
                    break;
                else
                    return;
            }

            /* in the middle of parsing tokens */
            case LCC_LX_SUBSTATE_STRING:
            case LCC_LX_SUBSTATE_STRING_ESCAPE:
            case LCC_LX_SUBSTATE_STRING_ESCAPE_HEX:
            case LCC_LX_SUBSTATE_STRING_ESCAPE_HEX_D:
            case LCC_LX_SUBSTATE_STRING_ESCAPE_HEX_E:
            case LCC_LX_SUBSTATE_STRING_ESCAPE_OCT_2:
            case LCC_LX_SUBSTATE_STRING_ESCAPE_OCT_3:
            case LCC_LX_SUBSTATE_INCLUDE_FILE:
            case LCC_LX_SUBSTATE_NUMBER_INTEGER_HEX:
            case LCC_LX_SUBSTATE_NUMBER_DECIMAL_SCI:
            case LCC_LX_SUBSTATE_NUMBER_DECIMAL_SCI_EXP_SIGN:
            case LCC_LX_SUBSTATE_OPERATOR_ELLIPSIS:
            {
                /* not a directive, it's an error */
                if (!(self->flags & LCC_LXF_DIRECTIVE))
                {
                    _lcc_lexer_error(self, "Unexpected %s", (self->flags & LCC_LXF_EOF) ? "EOF" : "EOL");
                    return;
                }

                /* commit as string, but give a warning */
                _lcc_commit_string(self, 1);
                _lcc_lexer_warning(self, "Invalid preprocessor token");
                break;
            }

            /* in the middle of parsing integers */
            case LCC_LX_SUBSTATE_NUMBER:
            case LCC_LX_SUBSTATE_NUMBER_ZERO:
            case LCC_LX_SUBSTATE_NUMBER_INTEGER_HEX_D:
            case LCC_LX_SUBSTATE_NUMBER_INTEGER_OCT:
            {
                _lcc_commit_number(self, LCC_LT_INT, 1);
                break;
            }

            /* in the middle of parsing integers (unsigned) */
            case LCC_LX_SUBSTATE_NUMBER_INTEGER_U:
            {
                _lcc_commit_number(self, LCC_LT_UINT, 1);
                break;
            }

            /* in the middle of parsing integers (long) */
            case LCC_LX_SUBSTATE_NUMBER_INTEGER_L:
            {
                _lcc_commit_number(self, LCC_LT_LONG, 1);
                break;
            }

            /* in the middle of parsing integers (unsigned long) */
            case LCC_LX_SUBSTATE_NUMBER_INTEGER_UL:
            {
                _lcc_commit_number(self, LCC_LT_ULONG, 1);
                break;
            }

            /* in the middle of parsing floats (double) */
            case LCC_LX_SUBSTATE_NUMBER_DECIMAL:
            case LCC_LX_SUBSTATE_NUMBER_DECIMAL_SCI_EXP:
            {
                _lcc_commit_number(self, LCC_LT_DOUBLE, 1);
                break;
            }

            /* in the middle of parsing operators, accept or commit what already got */
            case LCC_LX_SUBSTATE_NUMBER_OR_OP     : _lcc_commit_operator(self, LCC_OP_POINT     , 1); break;
            case LCC_LX_SUBSTATE_OPERATOR_PLUS    : _lcc_commit_operator(self, LCC_OP_PLUS      , 1); break;
            case LCC_LX_SUBSTATE_OPERATOR_MINUS   : _lcc_commit_operator(self, LCC_OP_MINUS     , 1); break;
            case LCC_LX_SUBSTATE_OPERATOR_STAR    : _lcc_commit_operator(self, LCC_OP_STAR      , 1); break;
            case LCC_LX_SUBSTATE_OPERATOR_SLASH   : _lcc_commit_operator(self, LCC_OP_SLASH     , 1); break;
            case LCC_LX_SUBSTATE_OPERATOR_PERCENT : _lcc_commit_operator(self, LCC_OP_PERCENT   , 1); break;
            case LCC_LX_SUBSTATE_OPERATOR_EQU     : _lcc_commit_operator(self, LCC_OP_ASSIGN    , 1); break;
            case LCC_LX_SUBSTATE_OPERATOR_GT      : _lcc_commit_operator(self, LCC_OP_GT        , 1); break;
            case LCC_LX_SUBSTATE_OPERATOR_GT_GT   : _lcc_commit_operator(self, LCC_OP_BSHR      , 1); break;
            case LCC_LX_SUBSTATE_OPERATOR_LT      : _lcc_commit_operator(self, LCC_OP_LT        , 1); break;
            case LCC_LX_SUBSTATE_OPERATOR_LT_LT   : _lcc_commit_operator(self, LCC_OP_BSHL      , 1); break;
            case LCC_LX_SUBSTATE_OPERATOR_EXCL    : _lcc_commit_operator(self, LCC_OP_LNOT      , 1); break;
            case LCC_LX_SUBSTATE_OPERATOR_AMP     : _lcc_commit_operator(self, LCC_OP_BAND      , 1); break;
            case LCC_LX_SUBSTATE_OPERATOR_BAR     : _lcc_commit_operator(self, LCC_OP_BOR       , 1); break;
            case LCC_LX_SUBSTATE_OPERATOR_CARET   : _lcc_commit_operator(self, LCC_OP_BXOR      , 1); break;
            case LCC_LX_SUBSTATE_OPERATOR_HASH    : _lcc_commit_operator(self, LCC_OP_STRINGIZE , 1); break;
        }

        /* EOF or EOL when parsing compiler directive, commit it */
        if (self->flags & LCC_LXF_DIRECTIVE)
            self->state = LCC_LX_STATE_COMMIT;
        else
            self->state = LCC_LX_STATE_ACCEPT;

        /* clear those flags */
        self->flags &= ~LCC_LXF_EOF;
        self->flags &= ~LCC_LXF_EOL;
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
                    self->flags |= LCC_LXF_CHAR_SEQ;
                    self->substate = LCC_LX_SUBSTATE_STRING;
                    break;
                }

                /* strings */
                case '"':
                {
                    self->flags &= ~LCC_LXF_CHAR_SEQ;
                    self->substate = LCC_LX_SUBSTATE_STRING;
                    break;
                }

                /* zero, maybe decimal, hexadecimal or octal integer */
                case '0':
                {
                    self->substate = LCC_LX_SUBSTATE_NUMBER_ZERO;
                    lcc_token_buffer_append(&(self->token_buffer), self->ch);
                    break;
                }

                /* numbers */
                case '1' ... '9':
                {
                    self->substate = LCC_LX_SUBSTATE_NUMBER;
                    lcc_token_buffer_append(&(self->token_buffer), self->ch);
                    break;
                }

                /* maybe a decimal number, or a simple point */
                case '.':
                {
                    self->substate = LCC_LX_SUBSTATE_NUMBER_OR_OP;
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
                        case '~': _lcc_commit_operator(self, LCC_OP_BINV     , 1); break;
                        case '(': _lcc_commit_operator(self, LCC_OP_LBRACKET , 1); break;
                        case ')': _lcc_commit_operator(self, LCC_OP_RBRACKET , 1); break;
                        case '[': _lcc_commit_operator(self, LCC_OP_LINDEX   , 1); break;
                        case ']': _lcc_commit_operator(self, LCC_OP_RINDEX   , 1); break;
                        case '{': _lcc_commit_operator(self, LCC_OP_LBLOCK   , 1); break;
                        case '}': _lcc_commit_operator(self, LCC_OP_RBLOCK   , 1); break;
                        case ':': _lcc_commit_operator(self, LCC_OP_COLON    , 1); break;
                        case ',': _lcc_commit_operator(self, LCC_OP_COMMA    , 1); break;
                        case ';': _lcc_commit_operator(self, LCC_OP_SEMICOLON, 1); break;
                        case '?': _lcc_commit_operator(self, LCC_OP_QUESTION , 1); break;
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

                /* token stringize */
                case '#':
                {
                    /* preprocessor operators */
                    if (self->flags & LCC_LXF_DIRECTIVE)
                    {
                        self->state = LCC_LX_STATE_SHIFT;
                        self->substate = LCC_LX_SUBSTATE_OPERATOR_HASH;
                        break;
                    }

                    /* directives allowed */
                    if (!(self->file->flags & LCC_FF_LNODIR))
                    {
                        self->state = LCC_LX_STATE_GOT_DIRECTIVE;
                        self->substate = LCC_LX_SUBSTATE_NULL;
                        return;
                    }

                    /* otherwise it's an error */
                    _lcc_lexer_error(self, "Invalid character '#'");
                    return;
                }

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
                    _LCC_WRONG_CHAR("Invalid character")
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

            /* commit the identifier */
            if (!(_lcc_commit_ident(self, 0)))
                return;

            /* keep the character */
            self->state = LCC_LX_STATE_ACCEPT_KEEP;
            break;
        }

        /* chars or strings */
        case LCC_LX_SUBSTATE_STRING:
        {
            switch (self->ch)
            {
                /* end of string */
                case '"':
                {
                    /* ignore when parsing chars */
                    if (self->flags & LCC_LXF_CHAR_SEQ)
                        goto _lcc_label_string_generic_char;

                    /* accept as strings */
                    _lcc_commit_string(self, 1);
                    self->state = LCC_LX_STATE_ACCEPT;
                    break;
                }

                /* end of chars */
                case '\'':
                {
                    /* ignore when parsing strings */
                    if (!(self->flags & LCC_LXF_CHAR_SEQ))
                        goto _lcc_label_string_generic_char;

                    /* chars cannot be empty */
                    if (!(self->token_buffer.len))
                    {
                        _lcc_lexer_error(self, "Empty character constant");
                        return;
                    }

                    /* accept as chars */
                    _lcc_commit_chars(self, 1);
                    self->state = LCC_LX_STATE_ACCEPT;
                    break;
                }

                /* escape characters */
                case '\\':
                {
                    self->state = LCC_LX_STATE_SHIFT;
                    self->substate = LCC_LX_SUBSTATE_STRING_ESCAPE;
                    lcc_token_buffer_append(&(self->token_buffer), self->ch);
                    break;
                }

                /* generic characters */
                default:
                _lcc_label_string_generic_char:
                {
                    self->state = LCC_LX_STATE_SHIFT;
                    lcc_token_buffer_append(&(self->token_buffer), self->ch);
                    break;
                }
            }

            break;
        }

        /* escape characters */
        case LCC_LX_SUBSTATE_STRING_ESCAPE:
        {
            switch (self->ch)
            {
                /* normal escape characters */
                case 'a':
                case 'b':
                case 'e':
                case 'f':
                case 'n':
                case 'r':
                case 't':
                case 'v':
                case '?':
                case '"':
                case '\'':
                case '\\':
                {
                    /* "\e" escape character (should have GNU extension enabled) */
                    if ((self->ch == 'e') && !(self->gnuext & LCC_LX_GNUX_ESCAPE_CHAR))
                        goto _lcc_string_escape_unknown;

                    /* end of escape sequence */
                    self->substate = LCC_LX_SUBSTATE_STRING;
                    break;
                }

                /* hexadecimal escape characters */
                case 'x':
                case 'X':
                {
                    self->substate = LCC_LX_SUBSTATE_STRING_ESCAPE_HEX;
                    break;
                }

                /* octal escape characters */
                case '0' ... '3':
                {
                    self->substate = LCC_LX_SUBSTATE_STRING_ESCAPE_OCT_2;
                    break;
                }

                /* octal escape characters (two digits only) */
                case '4' ... '7':
                {
                    self->substate = LCC_LX_SUBSTATE_STRING_ESCAPE_OCT_3;
                    break;
                }

                /* other unknown characters */
                default:
                _lcc_string_escape_unknown:
                {
                    self->substate = LCC_LX_SUBSTATE_STRING;
                    _LCC_WRONG_CHAR("Unknown escape character")
                    break;
                }
            }

            /* append to token buffer */
            self->state = LCC_LX_STATE_SHIFT;
            lcc_token_buffer_append(&(self->token_buffer), self->ch);
            break;
        }

        /* hexadecimal escape characters (first and second digit) */
        case LCC_LX_SUBSTATE_STRING_ESCAPE_HEX:
        case LCC_LX_SUBSTATE_STRING_ESCAPE_HEX_D:
        {
            switch (self->ch)
            {
                /* hexadecimal digits */
                case '0' ... '9':
                case 'a' ... 'f':
                case 'A' ... 'F':
                {
                    /* move to next substate */
                    if (self->substate == LCC_LX_SUBSTATE_STRING_ESCAPE_HEX)
                        self->substate = LCC_LX_SUBSTATE_STRING_ESCAPE_HEX_D;
                    else
                        self->substate = LCC_LX_SUBSTATE_STRING_ESCAPE_HEX_E;

                    /* shift more characters */
                    self->state = LCC_LX_STATE_SHIFT;
                    lcc_token_buffer_append(&(self->token_buffer), self->ch);
                    break;
                }

                /* other unknown characters */
                default:
                {
                    _LCC_WRONG_CHAR("Invalid hexadecimal digit")
                    return;
                }
            }

            break;
        }

        /* hexadecimal escape characters (end of digits) */
        case LCC_LX_SUBSTATE_STRING_ESCAPE_HEX_E:
        {
            switch (self->ch)
            {
                /* hexadecimal digits */
                case '0' ... '9':
                case 'a' ... 'f':
                case 'A' ... 'F':
                {
                    _lcc_lexer_error(self, "Hex escape sequence out of range");
                    break;
                }

                /* other unknown characters */
                default:
                {
                    self->state = LCC_LX_STATE_GOT_CHAR;
                    self->substate = LCC_LX_SUBSTATE_STRING;
                    break;
                }
            }

            break;
        }

        /* octal escape characters (second and third digit) */
        case LCC_LX_SUBSTATE_STRING_ESCAPE_OCT_2:
        case LCC_LX_SUBSTATE_STRING_ESCAPE_OCT_3:
        {
            if ((self->ch < '0') || (self->ch > '7'))
            {
                self->state = LCC_LX_STATE_GOT_CHAR;
                self->substate = LCC_LX_SUBSTATE_STRING;
            }
            else if (self->substate == LCC_LX_SUBSTATE_STRING_ESCAPE_OCT_3)
            {
                self->state = LCC_LX_STATE_SHIFT;
                self->substate = LCC_LX_SUBSTATE_STRING;
                lcc_token_buffer_append(&(self->token_buffer), self->ch);
            }
            else
            {
                self->state = LCC_LX_STATE_SHIFT;
                self->substate = LCC_LX_SUBSTATE_STRING_ESCAPE_OCT_3;
                lcc_token_buffer_append(&(self->token_buffer), self->ch);
            }

            break;
        }

        /* numbers */
        case LCC_LX_SUBSTATE_NUMBER:
        case LCC_LX_SUBSTATE_NUMBER_DECIMAL:
        {
            switch (self->ch)
            {
                /* decimal point */
                case '.':
                {
                    /* already decimal number, stop here */
                    if (self->substate == LCC_LX_SUBSTATE_NUMBER_DECIMAL)
                        goto _lcc_label_number_end;

                    /* otherwise mark as decimal number */
                    self->state = LCC_LX_STATE_SHIFT;
                    self->substate = LCC_LX_SUBSTATE_NUMBER_DECIMAL;
                    lcc_token_buffer_append(&(self->token_buffer), self->ch);
                    break;
                }

                /* decimal number (scientific notation) */
                case 'e':
                case 'E':
                {
                    self->state = LCC_LX_STATE_SHIFT;
                    self->substate = LCC_LX_SUBSTATE_NUMBER_DECIMAL_SCI;
                    lcc_token_buffer_append(&(self->token_buffer), self->ch);
                    break;
                }

                /* digits */
                case '0' ... '9':
                {
                    self->state = LCC_LX_STATE_SHIFT;
                    lcc_token_buffer_append(&(self->token_buffer), self->ch);
                    break;
                }

                /* float specifier */
                case 'f':
                case 'F':
                {
                    self->state = LCC_LX_STATE_ACCEPT;
                    lcc_token_buffer_append(&(self->token_buffer), self->ch);
                    _lcc_commit_number(self, LCC_LT_FLOAT, 1);
                    break;
                }

                /* unsigned specifier */
                case 'u':
                case 'U':
                {
                    /* doesn't make sense with floating points */
                    if (self->substate == LCC_LX_SUBSTATE_NUMBER_DECIMAL)
                        goto _lcc_label_number_end;

                    /* mark as unsigned integer */
                    self->state = LCC_LX_STATE_SHIFT;
                    self->substate = LCC_LX_SUBSTATE_NUMBER_INTEGER_U;
                    lcc_token_buffer_append(&(self->token_buffer), self->ch);
                    break;
                }

                /* long specifier */
                case 'l':
                case 'L':
                {
                    /* long double, that's it, stop here */
                    if (self->substate == LCC_LX_SUBSTATE_NUMBER_DECIMAL)
                    {
                        self->state = LCC_LX_STATE_ACCEPT;
                        lcc_token_buffer_append(&(self->token_buffer), self->ch);
                        _lcc_commit_number(self, LCC_LT_LONGDOUBLE, 1);
                    }
                    else
                    {
                        self->state = LCC_LX_STATE_SHIFT;
                        self->substate = LCC_LX_SUBSTATE_NUMBER_INTEGER_L;
                        lcc_token_buffer_append(&(self->token_buffer), self->ch);
                    }

                    break;
                }

                /* other characters, commit the number */
                default:
                _lcc_label_number_end:
                {
                    /* move to next character */
                    if (self->substate == LCC_LX_SUBSTATE_NUMBER)
                        _lcc_commit_number(self, LCC_LT_INT, 0);
                    else
                        _lcc_commit_number(self, LCC_LT_DOUBLE, 0);

                    /* accept but keep this character */
                    self->state = LCC_LX_STATE_ACCEPT_KEEP;
                    break;
                }
            }

            break;
        }

        /* zero, maybe decimal, hexadecimal or octal integer */
        case LCC_LX_SUBSTATE_NUMBER_ZERO:
        {
            switch (self->ch)
            {
                /* decimal point */
                case '.':
                {
                    self->state = LCC_LX_STATE_SHIFT;
                    self->substate = LCC_LX_SUBSTATE_NUMBER_DECIMAL;
                    lcc_token_buffer_append(&(self->token_buffer), self->ch);
                    break;
                }

                /* decimal number (scientific notation) */
                case 'e':
                case 'E':
                {
                    self->state = LCC_LX_STATE_SHIFT;
                    self->substate = LCC_LX_SUBSTATE_NUMBER_DECIMAL_SCI;
                    lcc_token_buffer_append(&(self->token_buffer), self->ch);
                    break;
                }

                /* hexadecimal specifier */
                case 'x':
                case 'X':
                {
                    self->state = LCC_LX_STATE_SHIFT;
                    self->substate = LCC_LX_SUBSTATE_NUMBER_INTEGER_HEX;
                    lcc_token_buffer_append(&(self->token_buffer), self->ch);
                    break;
                }

                /* octal digits */
                case '0' ... '7':
                {
                    self->state = LCC_LX_STATE_SHIFT;
                    self->substate = LCC_LX_SUBSTATE_NUMBER_INTEGER_OCT;
                    lcc_token_buffer_append(&(self->token_buffer), self->ch);
                    break;
                }

                /* '8' and '9' are not valid octal digits */
                case '8':
                case '9':
                {
                    _lcc_lexer_error(self, "Invalid octal digit '%c'", self->ch);
                    break;
                }

                /* float specifier */
                case 'f':
                case 'F':
                {
                    self->state = LCC_LX_STATE_ACCEPT;
                    lcc_token_buffer_append(&(self->token_buffer), self->ch);
                    _lcc_commit_number(self, LCC_LT_FLOAT, 1);
                    break;
                }

                /* unsigned specifier */
                case 'u':
                case 'U':
                {
                    self->state = LCC_LX_STATE_SHIFT;
                    self->substate = LCC_LX_SUBSTATE_NUMBER_INTEGER_U;
                    lcc_token_buffer_append(&(self->token_buffer), self->ch);
                    break;
                }

                /* long specifier */
                case 'l':
                case 'L':
                {
                    self->state = LCC_LX_STATE_SHIFT;
                    self->substate = LCC_LX_SUBSTATE_NUMBER_INTEGER_L;
                    lcc_token_buffer_append(&(self->token_buffer), self->ch);
                    break;
                }

                /* other characters, commit a single zero */
                default:
                {
                    _lcc_commit_number(self, LCC_LT_INT, 0);
                    self->state = LCC_LX_STATE_ACCEPT_KEEP;
                    break;
                }
            }

            break;
        }

        /* decimal number or a simple point */
        case LCC_LX_SUBSTATE_NUMBER_OR_OP:
        {
            /* maybe ellipsis operator */
            if (self->ch == '.')
            {
                self->state = LCC_LX_STATE_SHIFT;
                self->substate = LCC_LX_SUBSTATE_OPERATOR_ELLIPSIS;
                break;
            }

            /* not a valid digit, accept as a "." operator */
            if (!(isdigit(self->ch)))
            {
                self->state = LCC_LX_STATE_ACCEPT_KEEP;
                _lcc_commit_operator(self, LCC_OP_POINT, 0);
                break;
            }

            /* also add the missing point */
            self->state = LCC_LX_STATE_SHIFT;
            self->substate = LCC_LX_SUBSTATE_NUMBER_DECIMAL;
            lcc_token_buffer_append(&(self->token_buffer), '.');
            lcc_token_buffer_append(&(self->token_buffer), self->ch);
            break;
        }

        /* hexadecimal integers */
        case LCC_LX_SUBSTATE_NUMBER_INTEGER_HEX:
        case LCC_LX_SUBSTATE_NUMBER_INTEGER_HEX_D:
        {
            if (((self->ch >= '0') && (self->ch <= '9')) ||
                ((self->ch >= 'a') && (self->ch <= 'f')) ||
                ((self->ch >= 'A') && (self->ch <= 'F')))
            {
                self->state = LCC_LX_STATE_SHIFT;
                self->substate = LCC_LX_SUBSTATE_NUMBER_INTEGER_HEX_D;
                lcc_token_buffer_append(&(self->token_buffer), self->ch);
                break;
            }
            else if (self->substate == LCC_LX_SUBSTATE_NUMBER_INTEGER_HEX_D)
            {
                switch (self->ch)
                {
                    /* unsigned specifier */
                    case 'u':
                    case 'U':
                    {
                        self->state = LCC_LX_STATE_SHIFT;
                        self->substate = LCC_LX_SUBSTATE_NUMBER_INTEGER_U;
                        lcc_token_buffer_append(&(self->token_buffer), self->ch);
                        break;
                    }

                    /* long specifier */
                    case 'l':
                    case 'L':
                    {
                        self->state = LCC_LX_STATE_SHIFT;
                        self->substate = LCC_LX_SUBSTATE_NUMBER_INTEGER_L;
                        lcc_token_buffer_append(&(self->token_buffer), self->ch);
                        break;
                    }

                    /* not a specifier */
                    default:
                    {
                        _lcc_commit_number(self, LCC_LT_INT, 0);
                        self->state = LCC_LX_STATE_ACCEPT_KEEP;
                        break;
                    }
                }

                break;
            }
            else
            {
                _LCC_WRONG_CHAR("Invalid hexadecimal digit")
                return;
            }
        }

        /* octal integers */
        case LCC_LX_SUBSTATE_NUMBER_INTEGER_OCT:
        {
            switch (self->ch)
            {
                /* unsigned specifier */
                case 'u':
                case 'U':
                {
                    self->state = LCC_LX_STATE_SHIFT;
                    self->substate = LCC_LX_SUBSTATE_NUMBER_INTEGER_U;
                    lcc_token_buffer_append(&(self->token_buffer), self->ch);
                    break;
                }

                /* long specifier */
                case 'l':
                case 'L':
                {
                    self->state = LCC_LX_STATE_SHIFT;
                    self->substate = LCC_LX_SUBSTATE_NUMBER_INTEGER_L;
                    lcc_token_buffer_append(&(self->token_buffer), self->ch);
                    break;
                }

                /* octal digits */
                case '0' ... '7':
                {
                    self->state = LCC_LX_STATE_SHIFT;
                    self->substate = LCC_LX_SUBSTATE_NUMBER_INTEGER_OCT;
                    lcc_token_buffer_append(&(self->token_buffer), self->ch);
                    break;
                }

                /* other characters */
                default:
                {
                    _lcc_commit_number(self, LCC_LT_INT, 0);
                    self->state = LCC_LX_STATE_ACCEPT_KEEP;
                    break;
                }
            }

            break;
        }

        /* unsigned specifier */
        case LCC_LX_SUBSTATE_NUMBER_INTEGER_U:
        {
            if ((self->ch != 'l') && (self->ch != 'L'))
            {
                _lcc_commit_number(self, LCC_LT_UINT, 0);
                self->state = LCC_LX_STATE_ACCEPT_KEEP;
            }
            else
            {
                self->state = LCC_LX_STATE_SHIFT;
                self->substate = LCC_LX_SUBSTATE_NUMBER_INTEGER_UL;
                lcc_token_buffer_append(&(self->token_buffer), self->ch);
            }

            break;
        }

        /* long specifier */
        case LCC_LX_SUBSTATE_NUMBER_INTEGER_L:
        {
            if ((self->ch != 'l') && (self->ch != 'L'))
            {
                _lcc_commit_number(self, LCC_LT_LONG, 0);
                self->state = LCC_LX_STATE_ACCEPT_KEEP;
            }
            else
            {
                self->state = LCC_LX_STATE_ACCEPT;
                lcc_token_buffer_append(&(self->token_buffer), self->ch);
                _lcc_commit_number(self, LCC_LT_LONGLONG, 1);
            }

            break;
        }

        /* unsigned long specifier */
        case LCC_LX_SUBSTATE_NUMBER_INTEGER_UL:
        {
            if ((self->ch != 'l') && (self->ch != 'L'))
            {
                _lcc_commit_number(self, LCC_LT_ULONG, 0);
                self->state = LCC_LX_STATE_ACCEPT_KEEP;
            }
            else
            {
                self->state = LCC_LX_STATE_ACCEPT;
                lcc_token_buffer_append(&(self->token_buffer), self->ch);
                _lcc_commit_number(self, LCC_LT_ULONGLONG, 1);
            }

            break;
        }

        /* scientific notation of decimal numbers */
        case LCC_LX_SUBSTATE_NUMBER_DECIMAL_SCI:
        {
            switch (self->ch)
            {
                /* "signed" exponent */
                case '+':
                case '-':
                {
                    self->state = LCC_LX_STATE_SHIFT;
                    self->substate = LCC_LX_SUBSTATE_NUMBER_DECIMAL_SCI_EXP_SIGN;
                    lcc_token_buffer_append(&(self->token_buffer), self->ch);
                    break;
                }

                /* "unsigned" exponent */
                case '0' ... '9':
                {
                    self->state = LCC_LX_STATE_SHIFT;
                    self->substate = LCC_LX_SUBSTATE_NUMBER_DECIMAL_SCI_EXP;
                    lcc_token_buffer_append(&(self->token_buffer), self->ch);
                    break;
                }

                /* other characters are prohibited */
                default:
                {
                    _lcc_lexer_error(self, "'+', '-' or digits expected");
                    break;
                }
            }

            break;
        }

        /* scientific notation of decimal numbers - exponent part */
        case LCC_LX_SUBSTATE_NUMBER_DECIMAL_SCI_EXP:
        {
            switch (self->ch)
            {
                /* float specifier */
                case 'f':
                case 'F':
                {
                    self->state = LCC_LX_STATE_ACCEPT;
                    lcc_token_buffer_append(&(self->token_buffer), self->ch);
                    _lcc_commit_number(self, LCC_LT_FLOAT, 1);
                    break;
                }

                /* long double specifier */
                case 'l':
                case 'L':
                {
                    self->state = LCC_LX_STATE_ACCEPT;
                    lcc_token_buffer_append(&(self->token_buffer), self->ch);
                    _lcc_commit_number(self, LCC_LT_LONGDOUBLE, 1);
                    break;
                }

                /* digits */
                case '0' ... '9':
                {
                    self->state = LCC_LX_STATE_SHIFT;
                    self->substate = LCC_LX_SUBSTATE_NUMBER_DECIMAL_SCI_EXP;
                    lcc_token_buffer_append(&(self->token_buffer), self->ch);
                    break;
                }

                /* other characters */
                default:
                {
                    _lcc_commit_number(self, LCC_LT_DOUBLE, 0);
                    self->state = LCC_LX_STATE_ACCEPT_KEEP;
                    break;
                }
            }

            break;
        }

        /* scientific notation of decimal numbers - sign of exponent part */
        case LCC_LX_SUBSTATE_NUMBER_DECIMAL_SCI_EXP_SIGN:
        {
            /* must be digits */
            if (!(isdigit(self->ch)))
            {
                _lcc_lexer_error(self, "Digits expected");
                break;
            }

            /* move to exponent state */
            self->state = LCC_LX_STATE_SHIFT;
            self->substate = LCC_LX_SUBSTATE_NUMBER_DECIMAL_SCI_EXP;
            lcc_token_buffer_append(&(self->token_buffer), self->ch);
            break;
        }

        /** very complex operator logic **/

        #define ACCEPT1_OR_KEEP(expect, ac_token, keep_token)                           \
        {                                                                               \
            if (self->ch == (expect))                                                   \
            {                                                                           \
                self->state = LCC_LX_STATE_ACCEPT;                                      \
                _lcc_commit_operator(self, ac_token, 1);                                \
            }                                                                           \
            else                                                                        \
            {                                                                           \
                self->state = LCC_LX_STATE_ACCEPT_KEEP;                                 \
                _lcc_commit_operator(self, keep_token, 0);                              \
            }                                                                           \
                                                                                        \
            break;                                                                      \
        }

        #define ACCEPT2_OR_KEEP(expect1, ac_token1, expect2, ac_token2, keep_token)     \
        {                                                                               \
            switch (self->ch)                                                           \
            {                                                                           \
                case expect1:                                                           \
                {                                                                       \
                    self->state = LCC_LX_STATE_ACCEPT;                                  \
                    _lcc_commit_operator(self, ac_token1, 1);                           \
                    break;                                                              \
                }                                                                       \
                                                                                        \
                case expect2:                                                           \
                {                                                                       \
                    self->state = LCC_LX_STATE_ACCEPT;                                  \
                    _lcc_commit_operator(self, ac_token2, 1);                           \
                    break;                                                              \
                }                                                                       \
                                                                                        \
                default:                                                                \
                {                                                                       \
                    self->state = LCC_LX_STATE_ACCEPT_KEEP;                             \
                    _lcc_commit_operator(self, keep_token, 0);                          \
                    break;                                                              \
                }                                                                       \
            }                                                                           \
                                                                                        \
            break;                                                                      \
        }

        #define SHIFT_ACCEPT_OR_KEEP(expect1, shift, expect2, ac_token, keep_token)     \
        {                                                                               \
            switch (self->ch)                                                           \
            {                                                                           \
                case expect1:                                                           \
                {                                                                       \
                    self->state = LCC_LX_STATE_SHIFT;                                   \
                    self->substate = shift;                                             \
                    break;                                                              \
                }                                                                       \
                                                                                        \
                case expect2:                                                           \
                {                                                                       \
                    self->state = LCC_LX_STATE_ACCEPT;                                  \
                    _lcc_commit_operator(self, ac_token, 1);                            \
                    break;                                                              \
                }                                                                       \
                                                                                        \
                default:                                                                \
                {                                                                       \
                    self->state = LCC_LX_STATE_ACCEPT_KEEP;                             \
                    _lcc_commit_operator(self, keep_token, 0);                          \
                    break;                                                              \
                }                                                                       \
            }                                                                           \
        }

        /* one- or two-character operators */
        case LCC_LX_SUBSTATE_OPERATOR_STAR    : ACCEPT1_OR_KEEP('=', LCC_OP_IMUL, LCC_OP_STAR   )   /* * *= */
        case LCC_LX_SUBSTATE_OPERATOR_PERCENT : ACCEPT1_OR_KEEP('=', LCC_OP_IMOD, LCC_OP_PERCENT)   /* % %= */
        case LCC_LX_SUBSTATE_OPERATOR_EQU     : ACCEPT1_OR_KEEP('=', LCC_OP_EQ  , LCC_OP_ASSIGN )   /* = == */
        case LCC_LX_SUBSTATE_OPERATOR_EXCL    : ACCEPT1_OR_KEEP('=', LCC_OP_NEQ , LCC_OP_LNOT   )   /* ! != */
        case LCC_LX_SUBSTATE_OPERATOR_AMP     : ACCEPT1_OR_KEEP('=', LCC_OP_IAND, LCC_OP_BAND   )   /* & &= */
        case LCC_LX_SUBSTATE_OPERATOR_BAR     : ACCEPT1_OR_KEEP('=', LCC_OP_IOR , LCC_OP_BOR    )   /* | |= */
        case LCC_LX_SUBSTATE_OPERATOR_CARET   : ACCEPT1_OR_KEEP('=', LCC_OP_IXOR, LCC_OP_BXOR   )   /* ^ ^= */

        /* one- or two-character and incremental operators */
        case LCC_LX_SUBSTATE_OPERATOR_PLUS    : ACCEPT2_OR_KEEP('+', LCC_OP_INCR, '=', LCC_OP_IADD, LCC_OP_PLUS )   /* + ++ += */
        case LCC_LX_SUBSTATE_OPERATOR_MINUS   : ACCEPT2_OR_KEEP('-', LCC_OP_DECR, '=', LCC_OP_ISUB, LCC_OP_MINUS)   /* - -- -= */

        /* bit-shifting operators */
        case LCC_LX_SUBSTATE_OPERATOR_GT_GT   : ACCEPT1_OR_KEEP('=', LCC_OP_ISHR, LCC_OP_BSHR)  /* >> >>= */
        case LCC_LX_SUBSTATE_OPERATOR_LT_LT   : ACCEPT1_OR_KEEP('=', LCC_OP_ISHL, LCC_OP_BSHL)  /* << <<= */

        /* > >> >= >>= */
        case LCC_LX_SUBSTATE_OPERATOR_GT:
        {
            SHIFT_ACCEPT_OR_KEEP('>', LCC_LX_SUBSTATE_OPERATOR_GT_GT, '=', LCC_OP_GEQ, LCC_OP_GT)
            break;
        }

        /* < << <= <<=, or #include <...> file name */
        case LCC_LX_SUBSTATE_OPERATOR_LT:
        {
            /* not parsing #include directive */
            if (!(self->flags & LCC_LXDN_INCLUDE))
            {
                SHIFT_ACCEPT_OR_KEEP('<', LCC_LX_SUBSTATE_OPERATOR_LT_LT, '=', LCC_OP_LEQ, LCC_OP_LT)
                break;
            }

            /* otherwise parse as file name */
            self->state = LCC_LX_STATE_SHIFT;
            self->flags |= LCC_LXDF_INCLUDE_SYS;
            self->substate = LCC_LX_SUBSTATE_INCLUDE_FILE;
            lcc_token_buffer_append(&(self->token_buffer), self->ch);
            break;
        }

        #undef ACCEPT1_OR_KEEP
        #undef ACCEPT2_OR_KEEP
        #undef SHIFT_ACCEPT_OR_KEEP

        /* include file name (#include <...> only) */
        case LCC_LX_SUBSTATE_INCLUDE_FILE:
        {
            if (self->ch == '>')
            {
                _lcc_commit_string(self, 1);
                self->state = LCC_LX_STATE_ACCEPT;
            }
            else
            {
                self->state = LCC_LX_STATE_SHIFT;
                lcc_token_buffer_append(&(self->token_buffer), self->ch);
            }

            break;
        }

        /* block comment, line comment or "/", "/=" */
        case LCC_LX_SUBSTATE_OPERATOR_SLASH:
        {
            switch (self->ch)
            {
                /* /= */
                case '=':
                {
                    self->state = LCC_LX_STATE_ACCEPT;
                    _lcc_commit_operator(self, LCC_OP_IDIV, 1);
                    break;
                }

                /* line comment */
                case '/':
                {
                    self->state = LCC_LX_STATE_SHIFT;
                    self->substate = LCC_LX_SUBSTATE_COMMENT_LINE;
                    break;
                }

                /* block comment */
                case '*':
                {
                    self->state = LCC_LX_STATE_SHIFT;
                    self->substate = LCC_LX_SUBSTATE_COMMENT_BLOCK;
                    break;
                }

                /* / */
                default:
                {
                    self->state = LCC_LX_STATE_ACCEPT_KEEP;
                    _lcc_commit_operator(self, LCC_OP_SLASH, 0);
                    break;
                }
            }

            break;
        }

        /* macro operators */
        case LCC_LX_SUBSTATE_OPERATOR_HASH:
        {
            if (self->ch == '#')
            {
                self->state = LCC_LX_STATE_ACCEPT;
                _lcc_commit_operator(self, LCC_OP_CONCAT, 1);
            }
            else
            {
                self->state = LCC_LX_STATE_ACCEPT_KEEP;
                _lcc_commit_operator(self, LCC_OP_STRINGIZE, 0);
            }

            break;
        }

        /* ellipsis operator */
        case LCC_LX_SUBSTATE_OPERATOR_ELLIPSIS:
        {
            /* the third character must be "." */
            if (self->ch != '.')
            {
                _LCC_WRONG_CHAR("Invalid character")
                return;
            }

            /* accept as ellipsis operator */
            self->state = LCC_LX_STATE_ACCEPT;
            _lcc_commit_operator(self, LCC_OP_ELLIPSIS, 1);
            break;
        }

        /* line comment, ignore every character on this line from now on */
        case LCC_LX_SUBSTATE_COMMENT_LINE:
        {
            self->state = LCC_LX_STATE_SHIFT;
            self->substate = LCC_LX_SUBSTATE_COMMENT_LINE;
            break;
        }

        /* block comment */
        case LCC_LX_SUBSTATE_COMMENT_BLOCK:
        {
            self->state = LCC_LX_STATE_SHIFT;
            self->substate = (self->ch == '*') ? LCC_LX_SUBSTATE_COMMENT_BLOCK_END : LCC_LX_SUBSTATE_COMMENT_BLOCK;
            break;
        }

        /* block comment end */
        case LCC_LX_SUBSTATE_COMMENT_BLOCK_END:
        {
            self->state = LCC_LX_STATE_SHIFT;
            self->substate = (self->ch == '/') ? LCC_LX_SUBSTATE_NULL : LCC_LX_SUBSTATE_COMMENT_BLOCK;
            break;
        }
    }
}

#undef _LCC_WRONG_CHAR

#define _LCC_FETCH_TOKEN(self, efmt, ...)                   \
({                                                          \
    /* should have at least 1 argument */                   \
    if (self->tokens.next == &(self->tokens))               \
    {                                                       \
        _lcc_lexer_error(self, efmt, ## __VA_ARGS__);       \
        return;                                             \
    }                                                       \
                                                            \
    /* extract the token */                                 \
    self->tokens.next;                                      \
})

#define _LCC_ENSURE_IDENT(self, token, efmt, ...)           \
({                                                          \
    /* must be an identifier */                             \
    if (token->type != LCC_TK_IDENT)                        \
    {                                                       \
        _lcc_lexer_error(self, efmt, ## __VA_ARGS__);       \
        return;                                             \
    }                                                       \
                                                            \
    /* extract the token */                                 \
    token->ident;                                           \
})

#define _LCC_ENSURE_RAWSTR(self, token, efmt, ...)          \
({                                                          \
    /* must be a string literal */                          \
    if ((token->type != LCC_TK_LITERAL) ||                  \
        (token->literal.type != LCC_LT_STRING))             \
    {                                                       \
        _lcc_lexer_error(self, efmt, ## __VA_ARGS__);       \
        return;                                             \
    }                                                       \
                                                            \
    /* extract the token */                                 \
    token->literal.raw;                                     \
})

#define _LCC_ENSURE_STRING(self, token, efmt, ...)          \
({                                                          \
    /* must be a string literal */                          \
    if ((token->type != LCC_TK_LITERAL) ||                  \
        (token->literal.type != LCC_LT_STRING))             \
    {                                                       \
        _lcc_lexer_error(self, efmt, ## __VA_ARGS__);       \
        return;                                             \
    }                                                       \
                                                            \
    /* extract the token */                                 \
    token->literal.v_string;                                \
})

#define _LCC_ENSURE_OPERATOR(self, token, op, efmt, ...)    \
({                                                          \
    /* must be an operator */                               \
    if ((token->type != LCC_TK_OPERATOR) ||                 \
        (token->operator != op))                            \
    {                                                       \
        _lcc_lexer_error(self, efmt, ## __VA_ARGS__);       \
        return;                                             \
    }                                                       \
})

static inline char _lcc_push_file(lcc_lexer_t *self, lcc_string_t *path)
{
    /* try load the file */
    char *name = path->buf;
    lcc_file_t file = lcc_file_open(name);

    /* check if it is loaded */
    if (file.flags & LCC_FF_INVALID)
        return 0;

    /* push to file stack */
    lcc_array_append(&(self->files), &file);
    self->file = lcc_array_top(&(self->files));
    return 1;
}

static inline char _lcc_file_exists(lcc_string_t *path)
{
    struct stat st;
    return stat(path->buf, &st) == 0;
}

static inline lcc_string_t *_lcc_path_concat(lcc_string_t *base, lcc_string_t *name)
{
    /* `name` is an absolute path */
    if (name->buf[0] == '/')
        return lcc_string_ref(name);

    /* otherwise make a copy of base first */
    char last = base->buf[base->len - 1];
    lcc_string_t *ret = lcc_string_copy(base);

    /* append path delimiter if needed */
    if (last != '/')
        lcc_string_append_from(ret, "/");

    /* then append name */
    lcc_string_append(ret, name);
    return ret;
}

static inline lcc_string_t *_lcc_make_message(lcc_lexer_t *self)
{
    /* no tokens */
    if (self->tokens.next == &(self->tokens))
        return lcc_string_from("");

    /* a single string, use it's content directly */
    if ((self->tokens.next->next == &(self->tokens)) &&
        (self->tokens.next->type == LCC_TK_LITERAL) &&
        (self->tokens.next->literal.type == LCC_LT_STRING))
    {
        /* make a reference to the content */
        lcc_token_t *t = self->tokens.next;
        lcc_string_t *s = lcc_string_ref(t->literal.v_string);

        /* clear the token */
        lcc_token_free(t);
        return s;
    }

    /* message string */
    lcc_string_t *p;
    lcc_string_t *s = lcc_string_new(0);

    /* concat each token */
    while (self->tokens.next != &(self->tokens))
    {
        lcc_string_append(s, (p = lcc_token_as_string(self->tokens.next)));
        lcc_string_append_from(s, " ");
        lcc_string_unref(p);
        lcc_token_free(self->tokens.next);
    }

    /* remove the last space */
    s->buf[--s->len] = 0;
    return s;
}

static inline lcc_string_t *_lcc_path_dirname(lcc_string_t *name)
{
    /* get it's directory name */
    char *buf = strndup(name->buf, name->len);
    lcc_string_t *ret = lcc_string_from(dirname(buf));

    /* free the string buffer */
    free(buf);
    return ret;
}

static char _lcc_load_include(lcc_lexer_t *self, lcc_string_t *fname)
{
    /* for "#include_next" support */
    char load = 1;
    char next = 0;
    char found = 0;
    dev_t fdev = 0;
    ino_t fino = 0;

    /* check for "#include_next" */
    if (self->flags & LCC_LXDF_INCLUDE_NEXT)
    {
        /* calculate include path from current file */
        struct stat st = {};
        lcc_string_t *dir = _lcc_path_dirname(self->file->name);

        /* try to read it's info */
        if (stat(dir->buf, &st))
        {
            _lcc_lexer_error(self, "Cannot read directory \"%s\": [%d] %s", dir->buf, errno, strerror(errno));
            lcc_string_unref(dir);
            return 0;
        }

        /* don't load immediately if it's a system include file */
        if (self->flags & LCC_LXDF_INCLUDE_SYS)
            load = 0;

        /* mark as "include_next" */
        next = 1;
        fdev = st.st_dev;
        fino = st.st_ino;
        lcc_string_unref(dir);
    }

    /* search for user directory if needed */
    if (!next && !(self->flags & LCC_LXDF_INCLUDE_SYS))
    {
        /* calculate include path from current file */
        char loaded = 0;
        lcc_string_t *dir = _lcc_path_dirname(self->file->name);
        lcc_string_t *path = _lcc_path_concat(dir, fname);

        /* push to file stack if exists */
        if (_lcc_file_exists(path))
        {
            found = 1;
            loaded = _lcc_push_file(self, path);
        }

        /* release the strings */
        lcc_string_unref(dir);
        lcc_string_unref(path);

        /* found, but not loaded, it's an error */
        if (found && !loaded)
        {
            _lcc_lexer_error(self, "Cannot open include file \"%s\": [%d] %s", fname->buf, errno, strerror(errno));
            return 0;
        }
    }

    /* search in system include directories */
    for (size_t i = 0; !found && (i < self->include_paths.array.count); i++)
    {
        char loaded = 0;
        char doload = load;
        struct stat st = {};

        /* make the full path */
        lcc_string_t *dir = lcc_string_array_get(&(self->include_paths), i);
        lcc_string_t *path = _lcc_path_concat(dir, fname);

        /* it's "include_next" */
        if (next)
        {
            /* try to read it's info */
            if (stat(dir->buf, &st))
            {
                lcc_string_unref(path);
                continue;
            }

            /* check for the same file */
            if ((st.st_dev == fdev) &&
                (st.st_ino == fino))
            {
                load = 0;
                doload = 1;
            }
        }

        /* check for load flag */
        if (load)
        {
            /* push to file stack if exists */
            if (_lcc_file_exists(path))
            {
                found = 1;
                loaded = _lcc_push_file(self, path);
            }
        }

        /* update load flag after loading this file */
        load = doload;
        lcc_string_unref(path);

        /* found, but not loaded, it's an error */
        if (found && !loaded)
        {
            _lcc_lexer_error(self, "Cannot open include file \"%s\": [%d] %s", fname->buf, errno, strerror(errno));
            return 0;
        }
    }

    /* found, and loaded successfully */
    if (found)
        return 1;

    /* still not found, it's an error */
    _lcc_lexer_error(self, "Cannot open include file \"%s\": [%d] %s", fname->buf, ENOENT, strerror(ENOENT));
    return 0;
}

static lcc_token_t *_lcc_next_arg(lcc_token_t *begin, lcc_token_t *end)
{
    /* parentheses nesting level */
    size_t level = 0;
    lcc_token_t *last = begin;

    /* find the last token of this argument */
    while (last != end)
    {
        /* maybe "," "(" or ")" operator */
        if (last->type == LCC_TK_OPERATOR)
        {
            /* checking for matched parentheses */
            if ((level == 0) &&
                ((last->operator == LCC_OP_COMMA) ||
                 (last->operator == LCC_OP_RBRACKET)))
                return last;

            /* parentheses matching */
            if (last->operator == LCC_OP_LBRACKET) level++;
            if (last->operator == LCC_OP_RBRACKET) level--;
        }

        /* move to next token */
        last = last->next;
    }

    /* encountered end of tokens, but still not found */
    return NULL;
}

static char _lcc_macro_cat(lcc_lexer_t *self, lcc_token_t *begin, lcc_token_t *end)
{
    /* reset token pointers */
    lcc_token_t *a;
    lcc_token_t *b;
    lcc_token_t *t = begin;

    /* pass 2: apply concatenation */
    while (t != end)
    {
        /* only care about "##" operator */
        if ((t->type != LCC_TK_OPERATOR) ||
            (t->operator != LCC_OP_CONCAT))
        {
            t = t->next;
            continue;
        }

        /* detach the previous and next token */
        a = lcc_token_detach(t->prev);
        b = lcc_token_detach(t->next);

        /* remove the "##" operator */
        t = t->next;
        lcc_token_free(t->prev);

        /* both identifiers, may form a keyword */
        if ((a->type == LCC_TK_IDENT) &&
            (b->type == LCC_TK_IDENT))
        {
            /* append with the next token */
            lcc_string_append(a->src, b->src);
            lcc_string_append(a->ident, b->ident);

            /* search for keyworkds */
            for (size_t i = 0; KEYWORDS[i].name; i++)
            {
                /* convert to keywords as needed */
                if (!(strcmp(KEYWORDS[i].name, a->ident->buf)))
                {
                    lcc_string_unref(a->ident);
                    a->type = LCC_TK_KEYWORD;
                    a->keyword = KEYWORDS[i].keyword;
                    break;
                }
            }

            /* attach the new token */
            lcc_token_free(b);
            lcc_token_attach(t, a);
            continue;
        }

        /* identifier with integer, never forms a keyword */
        if ((a->type == LCC_TK_IDENT) &&
            (b->type == LCC_TK_LITERAL) &&
            ((b->literal.type == LCC_LT_INT) ||
             (b->literal.type == LCC_LT_LONG) ||
             (b->literal.type == LCC_LT_LONGLONG) ||
             (b->literal.type == LCC_LT_UINT) ||
             (b->literal.type == LCC_LT_ULONG) ||
             (b->literal.type == LCC_LT_ULONGLONG)))
        {
            /* append with the next token */
            lcc_string_append(a->src, b->src);
            lcc_string_append(a->ident, b->literal.raw);

            /* attach the new token */
            lcc_token_free(b);
            lcc_token_attach(t, a);
            continue;
        }

        /* both keywords, always form an identifier */
        if ((a->type == LCC_TK_KEYWORD) &&
            (b->type == LCC_TK_KEYWORD))
        {
            /* both keywords */
            const char *kw1 = lcc_token_kw_name(a->keyword);
            const char *kw2 = lcc_token_kw_name(b->keyword);

            /* convert to identifier */
            a->type = LCC_TK_IDENT;
            a->ident = lcc_string_from_format("%s%s", kw1, kw2);

            /* attach the new token */
            lcc_token_free(b);
            lcc_token_attach(t, a);
            continue;
        }

        /* identifier and keyword, may form a keyword */
        if ((a->type == LCC_TK_IDENT) &&
            (b->type == LCC_TK_KEYWORD))
        {
            /* only "un" and "signed" can form "unsigned" */
            if (!(strcmp(a->ident->buf, "un")) &&
                (b->keyword == LCC_KW_SIGNED))
            {
                lcc_string_unref(a->ident);
                a->type = LCC_TK_KEYWORD;
                a->keyword = LCC_KW_UNSIGNED;
            }

            /* otherwise build an identifier */
            else
            {
                a->type = LCC_TK_IDENT;
                lcc_string_append_from(a->ident, lcc_token_kw_name(b->keyword));
            }

            /* attach the new token */
            lcc_token_free(b);
            lcc_token_attach(t, a);
            continue;
        }

        /* keyword and identifier, may form a keyword */
        if ((a->type == LCC_TK_KEYWORD) &&
            (b->type == LCC_TK_IDENT))
        {
            /* only "do" and "uble" can form "double" */
            if ((a->keyword == LCC_KW_DO) &&
                !(strcmp(b->ident->buf, "uble")))
            {
                a->type = LCC_TK_KEYWORD;
                a->keyword = LCC_KW_DOUBLE;
            }

            /* otherwise build an identifier */
            else
            {
                a->type = LCC_TK_IDENT;
                a->ident = lcc_string_from_format(
                    "%s%s",
                    lcc_token_kw_name(a->keyword),
                    b->ident->buf
                );
            }

            /* attach the new token */
            lcc_token_free(b);
            lcc_token_attach(t, a);
            continue;
        }

        /* keyword and integer, always form an identifier */
        if ((a->type == LCC_TK_KEYWORD) &&
            (b->type == LCC_TK_LITERAL) &&
            ((b->literal.type == LCC_LT_INT) ||
             (b->literal.type == LCC_LT_LONG) ||
             (b->literal.type == LCC_LT_LONGLONG) ||
             (b->literal.type == LCC_LT_UINT) ||
             (b->literal.type == LCC_LT_ULONG) ||
             (b->literal.type == LCC_LT_ULONGLONG)))
        {
            /* both keywords */
            const char *kw = lcc_token_kw_name(a->keyword);
            const char *num = b->literal.raw->buf;

            /* convert to identifier */
            a->type = LCC_TK_IDENT;
            a->ident = lcc_string_from_format("%s%s", kw, num);

            /* attach the new token */
            lcc_token_free(b);
            lcc_token_attach(t, a);
            continue;
        }

        /* both operators */
        if ((a->type == LCC_TK_OPERATOR) &&
            (b->type == LCC_TK_OPERATOR))
        {

#define _OP(a, b)                   (((uint32_t)a << 16u) | (uint32_t)b)
#define _DEL(tk)                    lcc_token_free(tk)
#define _ADD(list, tk)              lcc_token_attach(list, tk)
#define _NEW(name, op)              lcc_token_from_operator(lcc_string_from(name), (op))
#define _MAP(op1, op2, name, rop)   case _OP(op1, op2): { _DEL(a); _DEL(b); _ADD(t, _NEW(name, rop)); continue; }

            /* switch on compond operators */
            switch (_OP(a->operator, b->operator))
            {
                _MAP(LCC_OP_MINUS   , LCC_OP_GT     , "->"  , LCC_OP_DEREF  )   /* "-" ## ">" -> "->" */
                _MAP(LCC_OP_PLUS    , LCC_OP_PLUS   , "++"  , LCC_OP_INCR   )   /* "+" ## "+" -> "++" */
                _MAP(LCC_OP_MINUS   , LCC_OP_MINUS  , "--"  , LCC_OP_DECR   )   /* "-" ## "-" -> "--" */
                _MAP(LCC_OP_GT      , LCC_OP_GT     , ">>"  , LCC_OP_BSHR   )   /* "<" ## "<" -> "<<" */
                _MAP(LCC_OP_LT      , LCC_OP_LT     , "<<"  , LCC_OP_BSHL   )   /* ">" ## ">" -> ">>" */
                _MAP(LCC_OP_BAND    , LCC_OP_BAND   , "&&"  , LCC_OP_LAND   )   /* "&" ## "&" -> "&&" */
                _MAP(LCC_OP_BOR     , LCC_OP_BOR    , "||"  , LCC_OP_LOR    )   /* "|" ## "|" -> "||" */
                _MAP(LCC_OP_PLUS    , LCC_OP_ASSIGN , "+="  , LCC_OP_IADD   )   /* "+" ## "=" -> "+=" */
                _MAP(LCC_OP_MINUS   , LCC_OP_ASSIGN , "-="  , LCC_OP_ISUB   )   /* "-" ## "=" -> "-=" */
                _MAP(LCC_OP_STAR    , LCC_OP_ASSIGN , "*="  , LCC_OP_IMUL   )   /* "*" ## "=" -> "*=" */
                _MAP(LCC_OP_SLASH   , LCC_OP_ASSIGN , "/="  , LCC_OP_IDIV   )   /* "/" ## "=" -> "/=" */
                _MAP(LCC_OP_PERCENT , LCC_OP_ASSIGN , "%="  , LCC_OP_IMOD   )   /* "/" ## "=" -> "/=" */
                _MAP(LCC_OP_BAND    , LCC_OP_ASSIGN , "&="  , LCC_OP_IAND   )   /* "&" ## "=" -> "&=" */
                _MAP(LCC_OP_BOR     , LCC_OP_ASSIGN , "|="  , LCC_OP_IOR    )   /* "|" ## "=" -> "|=" */
                _MAP(LCC_OP_BXOR    , LCC_OP_ASSIGN , "^="  , LCC_OP_IXOR   )   /* "^" ## "=" -> "^=" */
                _MAP(LCC_OP_GT      , LCC_OP_ASSIGN , ">="  , LCC_OP_GEQ    )   /* ">" ## "=" -> ">=" */
                _MAP(LCC_OP_LT      , LCC_OP_ASSIGN , "<="  , LCC_OP_LEQ    )   /* "<" ## "=" -> "<=" */
                _MAP(LCC_OP_ASSIGN  , LCC_OP_ASSIGN , "=="  , LCC_OP_EQ     )   /* "=" ## "=" -> "==" */
                _MAP(LCC_OP_LNOT    , LCC_OP_ASSIGN , "!="  , LCC_OP_NEQ    )   /* "!" ## "=" -> "!=" */
                _MAP(LCC_OP_LT      , LCC_OP_LEQ    , "<<=" , LCC_OP_ISHL   )   /* "<" ## "<=" -> "<<=" */
                _MAP(LCC_OP_GT      , LCC_OP_GEQ    , ">>=" , LCC_OP_ISHR   )   /* ">" ## ">=" -> ">>=" */
                _MAP(LCC_OP_BSHL    , LCC_OP_ASSIGN , "<<=" , LCC_OP_ISHL   )   /* "<<" ## "=" -> "<<=" */
                _MAP(LCC_OP_BSHR    , LCC_OP_ASSIGN , ">>=" , LCC_OP_ISHR   )   /* ">>" ## "=" -> ">>=" */

                /* other unknown operators */
                default:
                {
                    const char *op1 = lcc_token_op_name(a->operator);
                    const char *op2 = lcc_token_op_name(b->operator);
                    _lcc_lexer_error(self, "\"%s%s\" is an invalid preprocessor token", op1, op2);
                    return 0;
                }
            }

#undef _OP
#undef _DEL
#undef _ADD
#undef _NEW
#undef _MAP

        }

        /* otherwise it's an error, dump the token */
        lcc_string_t *tk1 = lcc_token_as_string(a);
        lcc_string_t *tk2 = lcc_token_as_string(b);

        /* throw the error */
        _lcc_lexer_error(self, "\"%s%s\" is an invalid preprocessor token", tk1->buf, tk2->buf);
        lcc_string_unref(tk1);
        lcc_string_unref(tk2);
        return 0;
    }

    /* concatenation finished */
    return 1;
}

static void _lcc_macro_disp(lcc_token_t **pos, lcc_token_t **next, lcc_token_t *begin, lcc_token_t *end)
{
    /* first token */
    lcc_token_t *h = *pos;
    lcc_token_t *p = begin;
    lcc_token_t *q = h->next;

    /* make a copy of each token, then attach to anchor */
    while (p != end)
    {
        lcc_token_attach(q, lcc_token_copy(p));
        p = p->next;
    }

    /* replace the old anchor */
    *pos = h->next;
    *next = q;
    lcc_token_free(h);
}

static char _lcc_macro_func(lcc_lexer_t *self, lcc_token_t *head, lcc_psym_t *sym, size_t argc, lcc_token_t **argv)
{
    /* substitution result */
    ssize_t n;
    lcc_token_t *p = sym->body->next;

    /* pass 1: apply parameter substitution and stringize */
    while (p != sym->body)
    {
        /* argument subtitution */
        if ((p->type == LCC_TK_IDENT) &&
            ((n = lcc_string_array_index(&(sym->args), p->ident)) >= 0))
        {
            /* special case of "<arg> ## ..." where <arg> is nothing
             * skip the argument identifier, along with the "##" operator */
            if ((p->next->type == LCC_TK_OPERATOR) &&
                (p->next->operator == LCC_OP_CONCAT) &&
                (argv[n]->next == argv[n + 1]))
            {
                p = p->next->next;
                continue;
            }

            /* perform substitution */
            for (lcc_token_t *t = argv[n]->next; t != argv[n + 1]; t = t->next)
                lcc_token_attach(head, lcc_token_copy(t));

            /* skip the token */
            p = p->next;
            continue;
        }

        /* variadic argument substitution */
        if ((p->type == LCC_TK_IDENT) &&
            (p->ident->len == sym->vaname->len) &&
            !(strcmp(p->ident->buf, sym->vaname->buf)))
        {
            /* special case of "<varg> ## ..." where <varg> is nothing,
             * skip the variadic argument identifier, along with the "##" operator */
            if ((p->next->type == LCC_TK_OPERATOR) &&
                (p->next->operator == LCC_OP_CONCAT) &&
                ((argc <= sym->args.array.count) ||
                 (argv[sym->args.array.count]->next == argv[argc])))
            {
                p = p->next->next;
                continue;
            }

            /* attach all variadic arguments, including comma */
            if (argc > sym->args.array.count)
                for (lcc_token_t *t = argv[sym->args.array.count]->next; t != argv[argc]; t = t->next)
                    lcc_token_attach(head, lcc_token_copy(t));

            /* skip the token */
            p = p->next;
            continue;
        }

        /* special case of concatenating with empty arguments */
        if ((p->type == LCC_TK_OPERATOR) &&                                         /* first token must be an operator */
            (p->operator == LCC_OP_CONCAT) &&                                       /* which must be a concatenation operator */
            (p->next->type == LCC_TK_IDENT) &&                                      /* second token must be an identifier */
            ((((n = lcc_string_array_index(&(sym->args), p->next->ident)) >= 0) &&  /* which must be an argument name */
              (argv[n]->next == argv[n + 1])) ||                                    /* and this argument expands to nothing */
             ((p->next->ident->len == sym->vaname->len) &&                          /* or the length of this name equals to varg name */
              !(strcmp(p->next->ident->buf, sym->vaname->buf)) &&                   /* so does the characters */
              ((argc <= sym->args.array.count) ||                                   /* and we don't have excess arguments to deal with */
               (argv[sym->args.array.count]->next == argv[argc])))))                /* or we just have an empty variadic list */
        {
            p = p->next->next;
            continue;
        }

        /* special case of ", ## <vargs>" */
        if ((p->type == LCC_TK_OPERATOR) &&                             /* first token must be an operator */
            (p->operator == LCC_OP_COMMA) &&                            /* which must be "," operator */
            (p->next->type == LCC_TK_OPERATOR) &&                       /* second token must also be an operator */
            (p->next->operator == LCC_OP_CONCAT) &&                     /* which must be "##" operator */
            (p->next->next->type == LCC_TK_IDENT) &&                    /* third token must be an identifier */
            (p->next->next->ident->len == sym->vaname->len) &&          /* which must have the same length */
            !(strcmp(p->next->next->ident->buf, sym->vaname->buf)))     /* and same characters with variadic argument name */
        {
            /* remove the "," if <vargs> is empty */
            if (argc == sym->args.array.count)
            {
                p = p->next->next->next;
                continue;
            }
            else
            {
                lcc_token_attach(head, lcc_token_copy(p));
                p = p->next->next;
                continue;
            }
        }

        /* apply stringnize */
        if ((p->type == LCC_TK_OPERATOR) &&
            (p->operator == LCC_OP_STRINGIZE))
        {
            /* must be an argument after "#" */
            if ((p->next->type != LCC_TK_IDENT) ||
                ((n = lcc_string_array_index(&(sym->args), p->next->ident)) < 0))
            {
                _lcc_lexer_error(self, "'#' is not followed by a macro parameter");
                return 0;
            }

            /* create a new string */
            lcc_token_t *t = argv[n]->next;
            lcc_string_t *v = lcc_string_new(0);
            lcc_string_t *s;

            /* concat each part of token strings */
            while (t != argv[n + 1])
            {
                lcc_string_append(v, t->src);
                t = t->next;
            }

            /* remove whitespaces */
            s = v;
            v = lcc_string_trim(s);
            lcc_string_unref(s);

            /* skip to next token, and add a new token */
            p = p->next->next;
            lcc_token_attach(head, lcc_token_from_raw(v, lcc_string_ref(v)));
            continue;
        }

        /* copy directly */
        lcc_token_attach(head, lcc_token_copy(p));
        p = p->next;
    }

    /* pass 2: handle concatenation */
    return _lcc_macro_cat(self, head->next, head);
}

static void _lcc_macro_subst(lcc_lexer_t *self, lcc_token_t *begin, lcc_token_t *end)
{
    /* first token */
    lcc_psym_t *sym;
    lcc_token_t *next;
    lcc_token_t *token = begin;

    /* scan every token */
    while (token != end)
    {
        /* must be a valid macro name, otherwise ignore it */
        if ((token->type != LCC_TK_IDENT) ||
            !(lcc_map_get(&(self->psyms), token->ident, (void **)&sym)))
        {
            token = token->next;
            continue;
        }

        /* object-like macro */
        if (sym->flags & LCC_LXDF_DEFINE_O)
        {
            /* self-ref macros */
            if (token->ref || (sym->flags & LCC_LXDF_DEFINE_USING))
            {
                token->ref = 1;
                token = token->next;
                continue;
            }

            /* replace the name */
            _lcc_macro_disp(
                &token,
                &next,
                sym->body->next,
                sym->body
            );

            /* handle concatenation */
            if (!(_lcc_macro_cat(self, token, next)))
                return;
        }
        else
        {
            /* function-like macro, must follows a "(" operator */
            if ((token->next->type != LCC_TK_OPERATOR) ||
                (token->next->operator != LCC_OP_LBRACKET))
            {
                token = token->next;
                continue;
            }

            /* argument begin */
            lcc_token_t *head;
            lcc_token_t *start = token;

            /* skip the "(" operator */
            for (int i = 0; i < 2; i++)
            {
                /* check for EOF */
                if ((start = start->next) == end)
                {
                    _lcc_lexer_error(self, "Unterminated function-like macro invocation");
                    return;
                }
            }

            /* formal argument pointers */
            size_t argp = 0;
            size_t argcap = sym->args.array.count + 1;

            /* argument buffer */
            lcc_token_t *delim;
            lcc_token_t **argvp = malloc(argcap * sizeof(lcc_token_t *));

            /* make a stub delimiter */
            argvp[0] = token->next;
            memset(argvp + 1, 0, (argcap - 1) * sizeof(lcc_token_t *));

            /* macro pre-scan */
            do
            {
                /* find next end of argument */
                if (!(delim = _lcc_next_arg(start, end)))
                {
                    free(argvp);
                    _lcc_lexer_error(self, "Unterminated function-like macro invocation");
                    return;
                }

                /* expand argument buffer as needed */
                if (argp >= argcap - 1)
                {
                    argcap *= 2;
                    argvp = realloc(argvp, argcap * sizeof(lcc_token_t *));
                }

                /* expand as needed */
                if (!(start->ref) &&                                /* must not be self-ref macro */
                    !(sym->flags & LCC_LXDF_DEFINE_USING) &&        /* must not be recursive-ref macro */
                    ((argp >= sym->args.array.count) ||             /* could be an argument in variadic arguments */
                     !(sym->nx[argp / 64] & (1 << (argp % 64)))))   /* or "not-expand" bit not set for this argument */
                    _lcc_macro_subst(self, start, delim);

                /* skip the comma */
                start = delim->next;
                argvp[++argp] = delim;

            /* until encounter ")" */
            } while ((delim->type != LCC_TK_OPERATOR) ||
                     (delim->operator != LCC_OP_RBRACKET));

            /* no arguments when calling empty function-like macros */
            if ((argp == 1) &&
                (argvp[0]->next == argvp[1]) &&
                (sym->args.array.count == 0))
                argp = 0;

            /* not enough arguments */
            if (argp < sym->args.array.count)
            {
                free(argvp);
                _lcc_lexer_error(self, "Too few arguments provided to function-like macro invocation");
                return;
            }

            /* too many arguments */
            if ((argp > sym->args.array.count) &&
                !(sym->flags & LCC_LXDF_DEFINE_VAR))
            {
                free(argvp);
                _lcc_lexer_error(self, "Too many arguments provided to function-like macro invocation");
                return;
            }

            /* don't expand self-ref macros */
            if (token->ref || (sym->flags & LCC_LXDF_DEFINE_USING))
            {
                free(argvp);
                token->ref = 1;
                token = delim->next;
                continue;
            }

            /* perform function-like macro expansion */
            if (!(_lcc_macro_func(self, (head = lcc_token_new()), sym, argp, argvp)))
            {
                free(argvp);
                lcc_token_clear(head);
                return;
            }

            /* out with the original tokens */
            while (token->next != delim)
                lcc_token_free(token->next);

            /* in with the substitution */
            lcc_token_free(delim);
            _lcc_macro_disp(&token, &next, head->next, head);

            /* release bitmap and endpoint pointers */
            free(argvp);
            lcc_token_clear(head);
        }

        /* scan again for nested macros */
        sym->flags |= LCC_LXDF_DEFINE_USING;
        _lcc_macro_subst(self, token, next);
        sym->flags &= ~LCC_LXDF_DEFINE_USING;
        token = next;
    }
}

static void _lcc_handle_define(lcc_lexer_t *self)
{
    /* directive name not yet set */
    if (!(self->flags & LCC_LXDF_DEFINE_NS))
    {
        /* must be an identifier */
        if (self->tokens.next->type != LCC_TK_IDENT)
        {
            _lcc_lexer_error(self, "Macro name must be an identifier");
            return;
        }

        /* which must not be "defined" (which is
         * the only reserved word for preprocessor) */
        if (!(strcmp(self->tokens.next->ident->buf, "defined")))
        {
            _lcc_lexer_error(self, "Cannot redefine \"defined\"");
            return;
        }

        /* set a special flag after macro name parsed successfully
         * required for identifying object-like or function-like macro */
        self->flags |= LCC_LXDF_DEFINE_NS;
        self->defstate = LCC_LX_DEFSTATE_INIT;
        self->macro_name = lcc_string_ref(self->tokens.next->ident);
        self->macro_vaname = lcc_string_from("__VA_ARGS__");

        /* remove the identifier from tokens */
        lcc_token_free(self->tokens.next);
        lcc_string_array_init(&(self->macro_args));
        return;
    }

    /* only respond to function-like macros that are not checked */
    if ((self->flags & LCC_LXDF_DEFINE_O) ||
        (self->flags & LCC_LXDF_DEFINE_FINE))
        return;

    /* transfer the state machine */
    switch (self->defstate)
    {
        /* initial state */
        case LCC_LX_DEFSTATE_INIT:
        {
            _LCC_ENSURE_OPERATOR(self, self->tokens.next, LCC_OP_LBRACKET, "'(' expected");
            self->defstate = LCC_LX_DEFSTATE_PUSH_ARG;
            lcc_token_free(self->tokens.next);
            break;
        }

        /* push argument */
        case LCC_LX_DEFSTATE_PUSH_ARG:
        {
            /* may be identifier or "..." variadic specifier */
            switch (self->tokens.next->type)
            {
                /* named argument */
                case LCC_TK_IDENT:
                {
                    /* can have at most 65536 macro arguments */
                    if (self->macro_args.array.count >= 65536)
                    {
                        _lcc_lexer_error(self, "Too many macro arguments");
                        return;
                    }

                    /* check for existing names */
                    if (lcc_string_array_index(&(self->macro_args), self->tokens.next->ident) >= 0)
                    {
                        _lcc_lexer_error(self, "Duplicated macro argument: %s", self->tokens.next->ident->buf);
                        return;
                    }

                    /* move to next state */
                    self->defstate = LCC_LX_DEFSTATE_DELIM_OR_END;
                    lcc_string_array_append(&(self->macro_args), lcc_string_ref(self->tokens.next->ident));
                    break;
                }

                /* maybe variadic specifier */
                case LCC_TK_OPERATOR:
                {
                    /* maybe a macro with no arguments */
                    if (self->tokens.next->operator == LCC_OP_RBRACKET)
                    {
                        self->flags |= LCC_LXDF_DEFINE_FINE;
                        break;
                    }

                    /* otherwise can only be ellipsis */
                    if (self->tokens.next->operator != LCC_OP_ELLIPSIS)
                    {
                        _lcc_lexer_error(self, "Identifier or '...' expected");
                        return;
                    }

                    /* cannot have more arguments */
                    self->flags |= LCC_LXDF_DEFINE_VAR;
                    self->defstate = LCC_LX_DEFSTATE_END;
                    break;
                }

                /* otherwise it's an error */
                default:
                {
                    _lcc_lexer_error(self, "Identifier or '...' expected");
                    return;
                }
            }

            /* release the token */
            lcc_token_free(self->tokens.next);
            break;
        }

        /* delimiter or end of argument */
        case LCC_LX_DEFSTATE_DELIM_OR_END:
        {
            /* either way, it must be an operator */
            if (self->tokens.next->type != LCC_TK_OPERATOR)
            {
                _lcc_lexer_error(self, "')', ',' or '...' expected");
                return;
            }

            /* check for the operators */
            switch (self->tokens.next->operator)
            {
                /* more arguments */
                case LCC_OP_COMMA:
                {
                    self->defstate = LCC_LX_DEFSTATE_PUSH_ARG;
                    break;
                }

                /* end of arguments */
                case LCC_OP_RBRACKET:
                {
                    self->flags |= LCC_LXDF_DEFINE_FINE;
                    break;
                }

                /* named variadic arguments */
                case LCC_OP_ELLIPSIS:
                {
                    /* replace the variadic argument name */
                    lcc_string_unref(self->macro_vaname);
                    self->macro_vaname = lcc_string_array_pop(&(self->macro_args));

                    /* cannot have more arguments */
                    self->flags |= LCC_LXDF_DEFINE_VAR;
                    self->flags |= LCC_LXDF_DEFINE_NVAR;
                    self->defstate = LCC_LX_DEFSTATE_END;
                    break;
                }

                /* otherwise it's an error */
                default:
                {
                    _lcc_lexer_error(self, "')', ',' or '...' expected");
                    return;
                }
            }

            /* release the token */
            lcc_token_free(self->tokens.next);
            break;
        }

        /* dead end */
        case LCC_LX_DEFSTATE_END:
        {
            _LCC_ENSURE_OPERATOR(self, self->tokens.next, LCC_OP_RBRACKET, "')' expected");
            self->flags |= LCC_LXDF_DEFINE_FINE;
            lcc_token_free(self->tokens.next);
            break;
        }
    }
}

static void _lcc_handle_directive(lcc_lexer_t *self)
{
    switch (self->flags & LCC_LXDN_MASK)
    {
        /* update directive name if it's the first token */
        case 0:
        {
            /* maybe a null directive */
            if (self->tokens.prev == &(self->tokens))
            {
                self->flags |= LCC_LXDN_NULL;
                break;
            }

            /* get the directive name token */
            lcc_string_t *name = _LCC_ENSURE_IDENT(self, self->tokens.next, "Directive name expected");
            const _lcc_directive_bits_t *pb;

            /* find directive by name */
            for (pb = DIRECTIVES; pb->name; pb++)
                if (!(strcmp(pb->name, name->buf)))
                    break;

            /* no such directive */
            if (!(pb->name))
            {
                _lcc_lexer_error(self, "Unknown compiler directive \"%s\"", name->buf);
                return;
            }

            /* set the name bit, and remove the token */
            self->flags |= pb->bits;
            lcc_token_free(self->tokens.prev);
            break;
        }

        /* these directives are not handled here */
        case LCC_LXDN_NULL:
        case LCC_LXDN_INCLUDE:
        case LCC_LXDN_UNDEF:
        case LCC_LXDN_IF:
        case LCC_LXDN_IFDEF:
        case LCC_LXDN_IFNDEF:
        case LCC_LXDN_ELIF:
        case LCC_LXDN_ELSE:
        case LCC_LXDN_ENDIF:
        case LCC_LXDN_PRAGMA:
        case LCC_LXDN_ERROR:
        case LCC_LXDN_WARNING:
        case LCC_LXDN_LINE:
        case LCC_LXDN_SCCS:
            break;

        /* "#define" directive */
        case LCC_LXDN_DEFINE:
        {
            _lcc_handle_define(self);
            break;
        }

        /* unknown bit-mask, should not happen */
        default:
        {
            fprintf(stderr, "*** FATAL: unknown directive flags %.16lx\n", self->flags);
            abort();
        }
    }
}

static void _lcc_commit_directive(lcc_lexer_t *self)
{
    /* check directive type */
    switch (self->flags & LCC_LXDN_MASK)
    {
        /* "#" directive (null directive, does nothing) */
        case LCC_LXDN_NULL:
            break;

        /* "#include" directive */
        case LCC_LXDN_INCLUDE:
        {
            /* extract the file name */
            lcc_token_t *token = _LCC_FETCH_TOKEN(self, "Missing include file name");
            lcc_string_t *fname = _LCC_ENSURE_RAWSTR(self, token, "Include file name must be a string");

            /* load the include file */
            if (_lcc_load_include(self, fname))
            {
                lcc_token_free(token);
                break;
            }
            else
            {
                lcc_token_free(token);
                return;
            }
        }

        /* "#define" directive */
        case LCC_LXDN_DEFINE:
        {
            /* identifier must be set */
            if (!(self->flags & LCC_LXDF_DEFINE_NS))
            {
                _lcc_lexer_error(self, "Missing macro name");
                return;
            }

            /* not-expand (NX) bits (at most 65536 arguments) */
            ssize_t n;
            lcc_psym_t psym;
            lcc_token_t *token = self->tokens.next;

            /* create a new symbol */
            lcc_psym_init(
                &psym,
                self->macro_name,
                &(self->macro_args),
                self->flags,
                self->macro_vaname
            );

            /* move everything remaining to macro body */
            if (self->tokens.next != &(self->tokens))
            {
                psym.body->prev = self->tokens.prev;
                psym.body->next = self->tokens.next;
                self->tokens.prev->next = psym.body;
                self->tokens.next->prev = psym.body;
                self->tokens.prev = &(self->tokens);
                self->tokens.next = &(self->tokens);
            }

            /* search for arguments that don't need expansion */
            while (token != psym.body)
            {
                /* special operators */
                if (token->type == LCC_TK_OPERATOR)
                {
                    /* mark identifier after "#" as not-expand (NX) */
                    if (token->operator == LCC_OP_STRINGIZE)
                    {
                        /* must be an argument after "#" */
                        if ((token->next->type != LCC_TK_IDENT) ||
                            ((n = lcc_string_array_index(&(psym.args), token->next->ident)) < 0))
                        {
                            lcc_psym_free(&psym);
                            _lcc_lexer_error(self, "'#' is not followed by a macro parameter");
                            return;
                        }

                        /* mark as not-expand */
                        psym.nx[n / 64] |= (1 << (n % 64));
                    }

                    /* mark identifier before and after "##" as not-expand (NX) */
                    else if (token->operator == LCC_OP_CONCAT)
                    {
                        /* can't be placed in either side */
                        if ((token->prev == psym.body) ||
                            (token->next == psym.body))
                        {
                            lcc_psym_free(&psym);
                            _lcc_lexer_error(self, "'##' cannot appear at either side of macro expansion");
                            return;
                        }

                        /* before "##" */
                        if ((token->prev->type == LCC_TK_IDENT) &&
                            ((n = lcc_string_array_index(&(psym.args), token->prev->ident)) >= 0))
                            psym.nx[n / 64] |= (1 << (n % 64));

                        /* after "##" */
                        if ((token->next->type == LCC_TK_IDENT) &&
                            ((n = lcc_string_array_index(&(psym.args), token->next->ident)) >= 0))
                            psym.nx[n / 64] |= (1 << (n % 64));
                    }
                }

                /* move to next token */
                token = token->next;
            }

            /* add to predefined symbols */
            if (lcc_map_set(&(self->psyms), self->macro_name, NULL, &psym))
                _lcc_lexer_warning(self, "Symbol \"%s\" redefined", self->macro_name->buf);

            break;
        }

        /* "#undef" directive */
        case LCC_LXDN_UNDEF:
        {
            /* extract the file name */
            lcc_token_t *token = _LCC_FETCH_TOKEN(self, "Missing macro name");
            lcc_string_t *macro = _LCC_ENSURE_IDENT(self, token, "Macro name must be an identifier");

            /* undefine the macro */
            if (!(lcc_map_pop(&(self->psyms), macro, NULL)))
                _lcc_lexer_warning(self, "Macro \"%s\" was not defined", macro->buf);

            /* release the token */
            lcc_token_free(token);
            break;
        }

        /* "#if" directive */
        case LCC_LXDN_IF:
        {
            // TODO: handle if
            printf("IF:\n");
            while (self->tokens.next != &(self->tokens))
            {
                lcc_string_t *s = lcc_token_to_string(self->tokens.next);
                printf(">> %s\n", s->buf);
                lcc_string_unref(s);
                lcc_token_free(self->tokens.next);
            }
            break;
        }

        /* "#ifdef" directive */
        case LCC_LXDN_IFDEF:
        {
            /* extract the file name */
            lcc_token_t *token = _LCC_FETCH_TOKEN(self, "Missing macro name");
            lcc_string_t *macro = _LCC_ENSURE_IDENT(self, token, "Macro name must be an identifier");

            // TODO: handle ifdef
            printf("IFDEF: %s\n", macro->buf);

            /* release the token */
            lcc_token_free(token);
            break;
        }

        /* "#ifndef" directive */
        case LCC_LXDN_IFNDEF:
        {
            /* extract the file name */
            lcc_token_t *token = _LCC_FETCH_TOKEN(self, "Missing macro name");
            lcc_string_t *macro = _LCC_ENSURE_IDENT(self, token, "Macro name must be an identifier");

            // TODO: handle ifndef
            printf("IFNDEF: %s\n", macro->buf);

            /* release the token */
            lcc_token_free(token);
            break;
        }

        /* "#elif" directive */
        case LCC_LXDN_ELIF:
        {
            // TODO: handle elif
            printf("ELIF:\n");
            while (self->tokens.next != &(self->tokens))
            {
                lcc_string_t *s = lcc_token_to_string(self->tokens.next);
                printf(">> %s\n", s->buf);
                lcc_string_unref(s);
                lcc_token_free(self->tokens.next);
            }
            break;
        }

        /* "#else" directive */
        case LCC_LXDN_ELSE:
        {
            // TODO: handle else
            printf("ELSE\n");
            break;
        }

        /* "#endif" directive */
        case LCC_LXDN_ENDIF:
        {
            // TODO: handle endif
            printf("ENDIF\n");
            break;
        }

        /* "#pragma" directive */
        case LCC_LXDN_PRAGMA:
        {
            /* doesn't care what tokens given */
            while (self->tokens.next != &(self->tokens))
                lcc_token_free(self->tokens.next);

            /* not supported */
            _lcc_lexer_warning(self, "#pragma directive is ignored");
            break;
        }

        /* "#error" directive */
        case LCC_LXDN_ERROR:
        {
            lcc_string_t *s = _lcc_make_message(self);
            _lcc_lexer_error(self, "%s", s->buf);
            lcc_string_unref(s);
            return;
        }

        /* "#warning" directive */
        case LCC_LXDN_WARNING:
        {
            lcc_string_t *s = _lcc_make_message(self);
            _lcc_lexer_warning(self, "%s", s->buf);
            lcc_string_unref(s);
            break;
        }

        /* "#line" directive */
        case LCC_LXDN_LINE:
        {
            /* doesn't care what tokens given */
            while (self->tokens.next != &(self->tokens))
                lcc_token_free(self->tokens.next);

            /* not supported */
            _lcc_lexer_warning(self, "#line directive is ignored");
            break;
        }

        /* "#sccs" directive */
        case LCC_LXDN_SCCS:
        {
            /* extract the SCCS message */
            lcc_token_t *token = _LCC_FETCH_TOKEN(self, "Missing SCCS message");
            lcc_string_t *message = _LCC_ENSURE_STRING(self, token, "SCCS message must be a string");

            /* add to SCCS messages and release the token */
            lcc_string_array_append(&(self->sccs_msgs), lcc_string_ref(message));
            lcc_token_free(token);
            break;
        }

        /* unknown bit-mask, should not happen */
        default:
        {
            fprintf(stderr, "*** FATAL: unknown directive flags %.16lx\n", self->flags);
            abort();
        }
    }

    /* clear directive flags */
    self->flags &= LCC_LXF_MASK;
    self->flags &= ~LCC_LXF_DIRECTIVE;

    /* should be no more tokens left */
    if (self->tokens.next != &(self->tokens))
    {
        _lcc_lexer_error(self, "Redundent directive parameter");
        return;
    }

    /* reset state and sub-state */
    self->state = LCC_LX_STATE_SHIFT;
    self->substate = LCC_LX_SUBSTATE_NULL;
}

#undef _LCC_FETCH_TOKEN
#undef _LCC_ENSURE_IDENT
#undef _LCC_ENSURE_RAWSTR
#undef _LCC_ENSURE_STRING
#undef _LCC_ENSURE_OPERATOR

static inline char _lcc_check_line_cont(lcc_file_t *fp, lcc_string_t *line)
{
    /* check for every character after this */
    for (size_t i = fp->col; i < line->len; i++)
        if (!(isspace(line->buf[i])))
            return 0;

    /* it is a line continuation, but with whitespaces */
    return 1;
}

static void _lcc_psym_dtor(lcc_map_t *self, void *value, void *data)
{
    lcc_psym_t *sym = value;
    lcc_psym_free(sym);
}

static void _lcc_file_dtor(lcc_array_t *self, void *item, void *data)
{
    lcc_file_t *fp = item;
    lcc_string_unref(fp->name);
    lcc_string_array_free(&(fp->lines));
}

void lcc_psym_free(lcc_psym_t *self)
{
    free(self->nx);
    lcc_token_clear(self->body);
    lcc_string_unref(self->name);
    lcc_string_unref(self->vaname);
    lcc_string_array_free(&(self->args));
}

void lcc_psym_init(
    lcc_psym_t *self,
    lcc_string_t *name,
    lcc_string_array_t *args,
    long flags,
    lcc_string_t *vaname)
{
    self->nx = calloc(1024, sizeof(uint64_t));
    self->body = lcc_token_new();
    self->name = name;
    self->args = *args;
    self->flags = flags;
    self->vaname = vaname;
    memset(args, 0, sizeof(lcc_string_array_t));
}

void lcc_lexer_free(lcc_lexer_t *self)
{
    /* clear old file name if any */
    if (self->fname)
        lcc_string_unref(self->fname);

    /* release all chained tokens */
    while (self->tokens.next != &(self->tokens))
        lcc_token_free(self->tokens.next);

    /* clear directive related tables */
    lcc_map_free(&(self->psyms));
    lcc_string_array_free(&(self->sccs_msgs));

    /* clear other tables */
    lcc_array_free(&(self->files));
    lcc_string_unref(self->source);
    lcc_token_buffer_free(&(self->token_buffer));
    lcc_string_array_free(&(self->include_paths));
    lcc_string_array_free(&(self->library_paths));
}

char lcc_lexer_init(lcc_lexer_t *self, lcc_file_t file)
{
    /* check for file flags */
    if (file.flags & LCC_FF_INVALID)
        return 0;

    /* macro sources */
    lcc_file_t psrc = {
        .col = 0,
        .row = 0,
        .name = lcc_string_from("<define>"),
        .flags = 0,
        .lines = LCC_STRING_ARRAY_STATIC_INIT,
    };

    /* complex structures */
    lcc_map_init(&(self->psyms), sizeof(lcc_psym_t), _lcc_psym_dtor, NULL);
    lcc_array_init(&(self->files), sizeof(lcc_file_t), _lcc_file_dtor, NULL);

    /* token list and token buffer */
    lcc_token_init(&(self->tokens));
    lcc_token_buffer_init(&(self->token_buffer));

    /* other tables */
    lcc_string_array_init(&(self->sccs_msgs));
    lcc_string_array_init(&(self->include_paths));
    lcc_string_array_init(&(self->library_paths));

    /* add initial source files */
    lcc_array_append(&(self->files), &file);
    lcc_array_append(&(self->files), &psrc);

    /* initial file info */
    self->col = 0;
    self->row = 0;
    self->fname = lcc_string_ref(psrc.name);
    self->source = lcc_string_new(0);

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

    /* version symbols */
    lcc_lexer_add_define(self, "__LCC__", "1");
    lcc_lexer_add_define(self, "__GNUC__", "4");

    /* standard defines */
    lcc_lexer_add_define(self, "__STDC__", "1");
    lcc_lexer_add_define(self, "__STDC_HOSTED__", "1");
    lcc_lexer_add_define(self, "__STDC_VERSION__", "199901L");

    /* platform specific symbols */
    lcc_lexer_add_define(self, "unix", "1");
    lcc_lexer_add_define(self, "__unix", "1");
    lcc_lexer_add_define(self, "__unix__", "1");
    lcc_lexer_add_define(self, "__x86_64__", "1");

    /* data model */
    lcc_lexer_add_define(self, "__LP64__", "1");
    lcc_lexer_add_define(self, "__SIZE_TYPE__", "unsigned long");
    lcc_lexer_add_define(self, "__PTRDIFF_TYPE__", "long");

    /* other built-in types */
    lcc_lexer_add_define(self, "__WINT_TYPE__", "unsigned int");
    lcc_lexer_add_define(self, "__WCHAR_TYPE__", "int");
    return 1;
}

lcc_token_t *lcc_lexer_next(lcc_lexer_t *self)
{
    /* advance lexer if no tokens remaining */
    if (self->tokens.next == &(self->tokens))
        if (!(lcc_lexer_advance(self)))
            return NULL;

    /* shift one token from lexer token list */
    return lcc_token_detach(self->tokens.next);
}

lcc_token_t *lcc_lexer_advance(lcc_lexer_t *self)
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
                    _lcc_lexer_error(self, "Line too long");
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

                /* clear old file name if any */
                if (self->fname)
                    lcc_string_unref(self->fname);

                /* save the current file info */
                self->col = file->col;
                self->row = file->row;
                self->fname = lcc_string_ref(file->name);

                /* check current character */
                if (self->ch != '\\')
                {
                    self->state = LCC_LX_STATE_GOT_CHAR;
                    break;
                }

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
                _lcc_lexer_warning(self, "Whitespaces after line continuation");
                break;
            }

            /* pop file from file stack */
            case LCC_LX_STATE_POP_FILE:
            {
                /* check for substitution status */
                if (self->flags & LCC_LXF_SUBST)
                {
                    _lcc_lexer_error(self, "Unterminated function-like macro invocation");
                    break;
                }

                /* check for EOS (End-Of-Source) */
                if (self->flags & LCC_LXF_EOS)
                {
                    self->state = LCC_LX_STATE_END;
                    self->substate = LCC_LX_SUBSTATE_NULL;
                    break;
                }

                /* set EOF flags to flush the preprocessor */
                self->flags |= LCC_LXF_EOF;
                _lcc_handle_substate(self);

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
                self->file->flags &= ~LCC_FF_LNODIR;
                break;
            }

            /* move to next line */
            case LCC_LX_STATE_NEXT_LINE:
            {
                /* set EOL flags to flush the preprocessor */
                self->flags |= LCC_LXF_EOL;
                lcc_string_append_from(self->source, "\n");
                _lcc_handle_substate(self);

                /* move to next line */
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

            /* we got a new character */
            case LCC_LX_STATE_GOT_CHAR:
            {
                /* Special Case :: "#define" compiler directive
                 * distingush object-like and function-like macros */
                if ((self->flags & LCC_LXDN_DEFINE) &&
                    (self->flags & LCC_LXDF_DEFINE_MASK) == LCC_LXDF_DEFINE_NS)
                {
                    if (self->ch == '(')
                        self->flags |= LCC_LXDF_DEFINE_F;
                    else
                        self->flags |= LCC_LXDF_DEFINE_O;
                }

                /* append last character if not EOF or EOL */
                if (!(self->flags & (LCC_LXF_EOF | LCC_LXF_EOL)))
                    lcc_string_append_from_size(self->source, &(self->ch), 1);

                /* handle all sub-states */
                self->flags &= ~LCC_LXF_EOF;
                self->flags &= ~LCC_LXF_EOL;
                _lcc_handle_substate(self);

                /* not a space, prohibit compiler directive on this line */
                if (!(isspace(self->ch)))
                    self->file->flags |= LCC_FF_LNODIR;

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
                /* handle the token first */
                _lcc_handle_directive(self);

                /* not rejected */
                if (self->state != LCC_LX_STATE_REJECT)
                    _lcc_commit_directive(self);

                /* allow directives from now on */
                self->file->flags &= ~LCC_FF_LNODIR;
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

                /* nothing to accept, equivalent to SHIFT or GOT_CHAR */
                if (self->tokens.next == &(self->tokens))
                    break;

                /* get the newly accepted token */
                lcc_psym_t *sym;
                lcc_token_t *token = self->tokens.prev;

                /* check for macro substitution */
                if (!(self->flags & LCC_LXF_SUBST) &&                               /* not substituting */
                     (token->type == LCC_TK_IDENT) &&                               /* is a valid identifier */
                     (lcc_map_get(&(self->psyms), token->ident, (void **)&sym)))    /* which is also a macro name */
                {
                    /* function-like macro, expect a "(" operator */
                    if (sym->flags & LCC_LXDF_DEFINE_F)
                    {
                        self->flags |= LCC_LXF_SUBST;
                        self->subst_level = 0;
                        break;
                    }

                    /* object-like macros */
                    self->flags &= ~LCC_LXF_SUBST;
                    _lcc_macro_subst(self, self->tokens.next, &(self->tokens));

                    /* check substitution status */
                    if (self->state == LCC_LX_STATE_REJECT)
                        break;
                    else
                        return &(self->tokens);
                }

                /* still not substiting */
                if (!(self->flags & LCC_LXF_SUBST))
                    return &(self->tokens);

                /* looking for the first bracket */
                if (!(self->subst_level) &&
                    !((token->type == LCC_TK_OPERATOR) &&
                      (token->operator == LCC_OP_LBRACKET)))
                {
                    self->flags &= ~LCC_LXF_SUBST;
                    return &(self->tokens);
                }

                /* not an operator, just append to token chain */
                if (token->type != LCC_TK_OPERATOR)
                    break;

                /* "(" operator */
                if (token->operator == LCC_OP_LBRACKET)
                    self->subst_level++;

                /* ")" operator */
                if (token->operator == LCC_OP_RBRACKET)
                    self->subst_level--;

                /* still in macro invocation */
                if (self->subst_level)
                    break;

                /* perform the substitution */
                self->flags &= ~LCC_LXF_SUBST;
                _lcc_macro_subst(self, self->tokens.next, &(self->tokens));

                /* check substitution status */
                if (self->state == LCC_LX_STATE_REJECT)
                    break;
                else
                    return &(self->tokens);
            }

            /* token rejected */
            case LCC_LX_STATE_REJECT:
            {
                self->state = LCC_LX_STATE_END;
                self->substate = LCC_LX_SUBSTATE_NULL;
                return NULL;
            }

            /* end of lexing, emit an EOF token */
            case LCC_LX_STATE_END:
                return &(self->tokens);
        }
    }
}

void lcc_lexer_add_define(lcc_lexer_t *self, const char *name, const char *value)
{
    /* must be in initial state */
    if (self->state != LCC_LX_STATE_INIT)
    {
        fprintf(stderr, "*** FATAL: cannot add symbols in the middle of parsing");
        abort();
    }

    /* add to predefined sources */
    if (!value)
    {
        lcc_string_array_append(
            &(self->file->lines),
            lcc_string_from_format("#define %s", name)
        );
    }
    else
    {
        lcc_string_array_append(
            &(self->file->lines),
            lcc_string_from_format("#define %s %s", name, value)
        );
    }
}

void lcc_lexer_add_include_path(lcc_lexer_t *self, const char *path)
{
    lcc_string_t *s = lcc_string_from(path);
    lcc_string_array_append(&(self->include_paths), s);
}

void lcc_lexer_add_library_path(lcc_lexer_t *self, const char *path)
{
    lcc_string_t *s = lcc_string_from(path);
    lcc_string_array_append(&(self->library_paths), s);
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