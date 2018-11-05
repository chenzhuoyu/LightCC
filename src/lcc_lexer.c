#include <time.h>
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
    self->len = q - self->buf;
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

        lcc_string_unref(self->src);
        lcc_token_detach(self);
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

char lcc_token_equals(lcc_token_t *self, lcc_token_t *other)
{
    /* identity check */
    if (self == other)
        return 1;

    /* type check */
    if (self->type != other->type)
        return 0;

    /* value check */
    switch (self->type)
    {
        /* simple tokens */
        case LCC_TK_EOF     : return 1;
        case LCC_TK_KEYWORD : return self->keyword == other->keyword;
        case LCC_TK_OPERATOR: return self->operator == other->operator;
        case LCC_TK_IDENT   : return lcc_string_equals(self->ident, other->ident);

        /* literals */
        case LCC_TK_LITERAL:
        {
            /* literal type check */
            if (self->literal.type != other->literal.type)
                return 0;

            /* literal value check */
            switch (self->literal.type)
            {
                /* primitive types */
                case LCC_LT_INT         : return self->literal.v_int        == other->literal.v_int;
                case LCC_LT_LONG        : return self->literal.v_long       == other->literal.v_long;
                case LCC_LT_LONGLONG    : return self->literal.v_longlong   == other->literal.v_longlong;
                case LCC_LT_UINT        : return self->literal.v_uint       == other->literal.v_uint;
                case LCC_LT_ULONG       : return self->literal.v_ulong      == other->literal.v_ulong;
                case LCC_LT_ULONGLONG   : return self->literal.v_ulonglong  == other->literal.v_ulonglong;
                case LCC_LT_FLOAT       : return self->literal.v_float      == other->literal.v_float;
                case LCC_LT_DOUBLE      : return self->literal.v_double     == other->literal.v_double;
                case LCC_LT_LONGDOUBLE  : return self->literal.v_longdouble == other->literal.v_longdouble;

                /* sequence types */
                case LCC_LT_CHAR        : return lcc_string_equals(self->literal.v_char, other->literal.v_char);
                case LCC_LT_STRING      : return lcc_string_equals(self->literal.v_string, other->literal.v_string);
            }
        }
    }

    abort();
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

lcc_token_t *lcc_token_from_int(intmax_t value)
{
    lcc_token_t *self = malloc(sizeof(lcc_token_t));
    self->ref = 0;
    self->src = lcc_string_from_format("%li", value);
    self->prev = self;
    self->next = self;
    self->type = LCC_TK_LITERAL;
    self->literal.raw = lcc_string_from_format("%li", value);
    self->literal.type = LCC_LT_LONGLONG;
    self->literal.v_longlong = value;
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
    self->literal.raw = lcc_string_from_format("\"%s\"", value->buf);
    self->literal.type = LCC_LT_STRING;
    self->literal.v_string = value;
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
    self->literal.raw = lcc_string_from_format("'%s'", value->buf);
    self->literal.type = LCC_LT_CHAR;
    self->literal.v_char = _lcc_string_eval(value, allow_gnuext);
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
    self->literal.raw = lcc_string_from_format("\"%s\"", value->buf);
    self->literal.type = LCC_LT_STRING;
    self->literal.v_string = _lcc_string_eval(value, allow_gnuext);
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
        case LCC_OP_ISHL      : return "<<=";
        case LCC_OP_ISHR      : return ">>=";
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

lcc_string_t *lcc_token_str(lcc_token_t *self)
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

lcc_string_t *lcc_token_repr(lcc_token_t *self)
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
                case LCC_LT_CHAR       : return lcc_string_from_format("{CHARS:%s}"     , self->literal.raw->buf);
                case LCC_LT_STRING     : return lcc_string_from_format("{STRING:%s}"    , self->literal.raw->buf);
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
    .offset = 0,
    .display = NULL,
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
        .offset = 1,
        .display = lcc_string_from(fname),
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
        lcc_string_array_append(
            &(result.lines),
            lcc_string_from_buffer(line, (size_t)linelen)
        );
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
        .offset = 1,
        .display = lcc_string_from(fname),
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

typedef char (_lcc_macro_extension_fn)(
    lcc_lexer_t *self,
    lcc_token_t **begin,
    lcc_token_t *end
);

typedef struct __lcc_sym_t
{
    long flags;
    lcc_token_t *body;
    lcc_string_t *name;
    lcc_string_t *vaname;
    lcc_string_array_t args;
    _lcc_macro_extension_fn *ext;
} _lcc_sym_t;

typedef struct __lcc_val_t
{
    char discard;
    intmax_t value;
} _lcc_val_t;

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

    /* create a new token */
    lcc_token_t *token = lcc_token_from_ident(
        _lcc_swap_source(self, keep_tail),
        _lcc_dump_token(self)
    );

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

    /* check for multi-character constant */
    if (token->literal.v_char->len > 1)
        _lcc_lexer_warning(self, "Multi-character character constant");

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

static void _lcc_sym_free(_lcc_sym_t *self)
{
    /* extensions have no body */
    if (self->body)
        lcc_token_clear(self->body);

    /* extensions have no variadic argument name either */
    if (self->vaname)
        lcc_string_unref(self->vaname);

    /* clear other fields */
    lcc_string_unref(self->name);
    lcc_string_array_free(&(self->args));
}

static void _lcc_file_free(lcc_file_t *self)
{
    lcc_string_unref(self->name);
    lcc_string_unref(self->display);
    lcc_string_array_free(&(self->lines));
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
            /* in the middle of nothing, just ignore it */
            case LCC_LX_SUBSTATE_NULL:
                break;

            /* clear source buffer when line comment ends */
            case LCC_LX_SUBSTATE_COMMENT_LINE:
            {
                self->source->len = 0;
                self->source->buf[0] = 0;
                break;
            }

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
            case LCC_LX_SUBSTATE_NUMBER_INTEGER_BIN:
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
            case LCC_LX_SUBSTATE_NUMBER_INTEGER_BIN_D:
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

                /* binary specifier */
                case 'b':
                case 'B':
                {
                    self->state = LCC_LX_STATE_SHIFT;
                    self->substate = LCC_LX_SUBSTATE_NUMBER_INTEGER_BIN;
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

        /* binary or hexadecimal integers */
        case LCC_LX_SUBSTATE_NUMBER_INTEGER_BIN:
        case LCC_LX_SUBSTATE_NUMBER_INTEGER_BIN_D:
        case LCC_LX_SUBSTATE_NUMBER_INTEGER_HEX:
        case LCC_LX_SUBSTATE_NUMBER_INTEGER_HEX_D:
        {
            /* binary integers */
            if (((self->ch == '0') ||
                 (self->ch == '1')) &&
                ((self->substate == LCC_LX_SUBSTATE_NUMBER_INTEGER_BIN) ||
                 (self->substate == LCC_LX_SUBSTATE_NUMBER_INTEGER_BIN_D)))
            {
                self->state = LCC_LX_STATE_SHIFT;
                self->substate = LCC_LX_SUBSTATE_NUMBER_INTEGER_BIN_D;
                lcc_token_buffer_append(&(self->token_buffer), self->ch);
                break;
            }

            /* hexadecimal integers */
            if ((((self->ch >= '0') && (self->ch <= '9')) ||
                 ((self->ch >= 'a') && (self->ch <= 'f')) ||
                 ((self->ch >= 'A') && (self->ch <= 'F'))) &&
                ((self->substate == LCC_LX_SUBSTATE_NUMBER_INTEGER_HEX) ||
                 (self->substate == LCC_LX_SUBSTATE_NUMBER_INTEGER_HEX_D)))
            {
                self->state = LCC_LX_STATE_SHIFT;
                self->substate = LCC_LX_SUBSTATE_NUMBER_INTEGER_HEX_D;
                lcc_token_buffer_append(&(self->token_buffer), self->ch);
                break;
            }

            /* check for binary digits */
            if (self->substate == LCC_LX_SUBSTATE_NUMBER_INTEGER_BIN)
            {
                _LCC_WRONG_CHAR("Invalid binary digit")
                return;
            }

            /* check for hexadecimal digits */
            if (self->substate == LCC_LX_SUBSTATE_NUMBER_INTEGER_HEX)
            {
                _LCC_WRONG_CHAR("Invalid hexadecimal digit")
                return;
            }

            /* check for integer modifiers */
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

#define KEEP(keep_token) default:               \
{                                               \
    self->state = LCC_LX_STATE_ACCEPT_KEEP;     \
    _lcc_commit_operator(self, keep_token, 0);  \
    break;                                      \
}

#define SHIFT(expect, shift) case expect:       \
{                                               \
    self->state = LCC_LX_STATE_SHIFT;           \
    self->substate = shift;                     \
    break;                                      \
}

#define ACCEPT(expect, ac_token) case expect:   \
{                                               \
    self->state = LCC_LX_STATE_ACCEPT;          \
    _lcc_commit_operator(self, ac_token, 1);    \
    break;                                      \
}

#define AC_K(                       \
    expect, ac_token,               \
    keep_token)                     \
{                                   \
    switch (self->ch)               \
    {                               \
        KEEP(keep_token)            \
        ACCEPT(expect, ac_token)    \
    }                               \
                                    \
    break;                          \
}

#define AC2_K(                      \
    expect1, ac_token1,             \
    expect2, ac_token2,             \
    keep_token)                     \
{                                   \
    switch (self->ch)               \
    {                               \
        KEEP(keep_token)            \
        ACCEPT(expect1, ac_token1)  \
        ACCEPT(expect2, ac_token2)  \
    }                               \
                                    \
    break;                          \
}

        /* one- or two-character operators */
        case LCC_LX_SUBSTATE_OPERATOR_STAR    : AC_K('=', LCC_OP_IMUL, LCC_OP_STAR   )  /* * *= */
        case LCC_LX_SUBSTATE_OPERATOR_PERCENT : AC_K('=', LCC_OP_IMOD, LCC_OP_PERCENT)  /* % %= */
        case LCC_LX_SUBSTATE_OPERATOR_EQU     : AC_K('=', LCC_OP_EQ  , LCC_OP_ASSIGN )  /* = == */
        case LCC_LX_SUBSTATE_OPERATOR_EXCL    : AC_K('=', LCC_OP_NEQ , LCC_OP_LNOT   )  /* ! != */
        case LCC_LX_SUBSTATE_OPERATOR_CARET   : AC_K('=', LCC_OP_IXOR, LCC_OP_BXOR   )  /* ^ ^= */

        /* one- or two-character and incremental operators */
        case LCC_LX_SUBSTATE_OPERATOR_PLUS    : AC2_K('+', LCC_OP_INCR, '=', LCC_OP_IADD, LCC_OP_PLUS )     /* + ++ += */
        case LCC_LX_SUBSTATE_OPERATOR_AMP     : AC2_K('&', LCC_OP_LAND, '=', LCC_OP_IAND, LCC_OP_BAND )     /* & && &= */
        case LCC_LX_SUBSTATE_OPERATOR_BAR     : AC2_K('|', LCC_OP_LOR , '=', LCC_OP_IOR , LCC_OP_BOR  )     /* | || |= */

        /* bit-shifting operators */
        case LCC_LX_SUBSTATE_OPERATOR_GT_GT   : AC_K('=', LCC_OP_ISHR, LCC_OP_BSHR)     /* >> >>= */
        case LCC_LX_SUBSTATE_OPERATOR_LT_LT   : AC_K('=', LCC_OP_ISHL, LCC_OP_BSHL)     /* << <<= */

        /* - -- -> -= */
        case LCC_LX_SUBSTATE_OPERATOR_MINUS:
        {
            switch (self->ch)
            {
                KEEP(LCC_OP_MINUS)
                ACCEPT('=', LCC_OP_ISUB)
                ACCEPT('-', LCC_OP_DECR)
                ACCEPT('>', LCC_OP_DEREF)
            }

            break;
        }

        /* > >> >= >>= */
        case LCC_LX_SUBSTATE_OPERATOR_GT:
        {
            switch (self->ch)
            {
                KEEP(LCC_OP_GT)
                SHIFT('>', LCC_LX_SUBSTATE_OPERATOR_GT_GT)
                ACCEPT('=', LCC_OP_GEQ)
            }

            break;
        }

        /* < << <= <<=, or #include <...> file name */
        case LCC_LX_SUBSTATE_OPERATOR_LT:
        {
            /* treat as file name when parsing "#include" directive */
            if (self->flags & LCC_LXDN_INCLUDE)
            {
                self->state = LCC_LX_STATE_SHIFT;
                self->flags |= LCC_LXDF_INCLUDE_SYS;
                self->substate = LCC_LX_SUBSTATE_INCLUDE_FILE;
                lcc_token_buffer_append(&(self->token_buffer), self->ch);
            }
            else
            {
                switch (self->ch)
                {
                    KEEP(LCC_OP_LT)
                    SHIFT('<', LCC_LX_SUBSTATE_OPERATOR_LT_LT)
                    ACCEPT('=', LCC_OP_LEQ)
                }
            }

            break;
        }

#undef KEEP
#undef SHIFT
#undef ACCEPT
#undef AC_K
#undef AC2_K

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
            /* block comment ends */
            if (self->ch == '/')
            {
                /* clear source buffer */
                self->source->len = 0;
                self->source->buf[0] = 0;

                /* shift to next state */
                self->state = LCC_LX_STATE_SHIFT;
                self->substate = LCC_LX_SUBSTATE_NULL;
                break;
            }

            /* we might have multiple stars before "/" character */
            if (self->ch != '*')
            {
                self->state = LCC_LX_STATE_SHIFT;
                self->substate = LCC_LX_SUBSTATE_COMMENT_BLOCK;
                break;
            }
            else
            {
                self->state = LCC_LX_STATE_SHIFT;
                self->substate = LCC_LX_SUBSTATE_COMMENT_BLOCK_END;
                break;
            }
        }
    }
}

#undef _LCC_WRONG_CHAR

/* evaluator declarations */

static char _lcc_eval_factor  (lcc_lexer_t *self, intmax_t *result, lcc_token_t **token, lcc_token_t *end);
static char _lcc_eval_term    (lcc_lexer_t *self, intmax_t *result, lcc_token_t **token, lcc_token_t *end);
static char _lcc_eval_expr    (lcc_lexer_t *self, intmax_t *result, lcc_token_t **token, lcc_token_t *end);
static char _lcc_eval_shift   (lcc_lexer_t *self, intmax_t *result, lcc_token_t **token, lcc_token_t *end);
static char _lcc_eval_order   (lcc_lexer_t *self, intmax_t *result, lcc_token_t **token, lcc_token_t *end);
static char _lcc_eval_equal   (lcc_lexer_t *self, intmax_t *result, lcc_token_t **token, lcc_token_t *end);
static char _lcc_eval_bit_and (lcc_lexer_t *self, intmax_t *result, lcc_token_t **token, lcc_token_t *end);
static char _lcc_eval_bit_xor (lcc_lexer_t *self, intmax_t *result, lcc_token_t **token, lcc_token_t *end);
static char _lcc_eval_bit_or  (lcc_lexer_t *self, intmax_t *result, lcc_token_t **token, lcc_token_t *end);
static char _lcc_eval_bool_and(lcc_lexer_t *self, intmax_t *result, lcc_token_t **token, lcc_token_t *end);
static char _lcc_eval_bool_or (lcc_lexer_t *self, intmax_t *result, lcc_token_t **token, lcc_token_t *end);

/* evaluator implementations */

#define _LCC_OP_0(op)               _LCC_TUPLE_ITEM_AT(op, 0)
#define _LCC_OP_1(op)               _LCC_TUPLE_ITEM_AT(op, 1)

#define _LCC_OP_OR_1_I              ()
#define _LCC_OP_OR_2_I              (||,)
#define _LCC_OP_OR_3_I              (||, ||,)
#define _LCC_OP_OR_4_I              (||, ||, ||,)
#define _LCC_OP_OR_I(n, i)          _LCC_TUPLE_ITEM_AT(_LCC_OP_OR_ ## n ## _I, i)
#define _LCC_OP_OR(n, i)            _LCC_OP_OR_I(n, i)

#define _LCC_OP_CHECK_I(i, n, op)   _LCC_OP_OR(n, i) ((*token)->operator == _LCC_OP_0(op))
#define _LCC_OP_CHECK(...)          _LCC_FOR_EACH(_LCC_OP_CHECK_I, _LCC_VA_NARGS(__VA_ARGS__), __VA_ARGS__)

#define _LCC_OP_CASE_I(i, n, op)    case _LCC_OP_0(op): lhs = _LCC_OP_1(op); break;
#define _LCC_OP_CASE(...)           _LCC_FOR_EACH(_LCC_OP_CASE_I, ?, __VA_ARGS__)

#define _LCC_NZ(val, msg)           ({  \
    /* check for zero */                \
    if (!val)                           \
    {                                   \
        _lcc_lexer_error(self, #msg);   \
        return 0;                       \
    }                                   \
                                        \
    /* the verified value */            \
    rhs;                                \
})

#define _LCC_SLASH_Z                Division by zero in preprocessor expression
#define _LCC_PERCENT_Z              Remainder by zero in preprocessor expression

#define _LCC_OP_S(op)               lhs op rhs
#define _LCC_OP_Z(op, msg)          lhs op _LCC_NZ(rhs, msg)
#define _LCC_MK_OP(name, op)        (LCC_OP_ ## name, _LCC_OP_S(op))
#define _LCC_MK_NZ(name, op)        (LCC_OP_ ## name, _LCC_OP_Z(op, _LCC_ ## name ## _Z))

#define _LCC_EVAL_OP(name, super, ...)                                          \
static char _lcc_eval_ ## name(                                                 \
    lcc_lexer_t *self,                                                          \
    intmax_t *result,                                                           \
    lcc_token_t **token,                                                        \
    lcc_token_t *end)                                                           \
{                                                                               \
    intmax_t lhs;                                                               \
    intmax_t rhs;                                                               \
    lcc_operator_t op;                                                          \
                                                                                \
    /* evaluate left-hand side operand */                                       \
    if (!(_lcc_eval_ ## super(self, &lhs, token, end)))                         \
        return 0;                                                               \
                                                                                \
    /* search for every operator */                                             \
    while ((*token) != end)                                                     \
    {                                                                           \
        /* must be an operator */                                               \
        if ((*token)->type != LCC_TK_OPERATOR)                                  \
        {                                                                       \
            _lcc_lexer_error(self, "Invalid preprocessor expression token");    \
            return 0;                                                           \
        }                                                                       \
                                                                                \
        /* doesn't care other operators */                                      \
        if (!(_LCC_OP_CHECK(__VA_ARGS__)))                                      \
            break;                                                              \
                                                                                \
        /* skip the operator */                                                 \
        op = (*token)->operator;                                                \
        *token = (*token)->next;                                                \
                                                                                \
        /* evaluate right-hand side operand */                                  \
        if (!(_lcc_eval_ ## super(self, &rhs, token, end)))                     \
            return 0;                                                           \
                                                                                \
        /* apply operators */                                                   \
        switch (op)                                                             \
        {                                                                       \
            /* auto-generated cases */                                          \
            _LCC_OP_CASE(__VA_ARGS__)                                           \
        }                                                                       \
    }                                                                           \
                                                                                \
    /* evaluation successful */                                                 \
    *result = lhs;                                                              \
    return 1;                                                                   \
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wswitch"

_LCC_EVAL_OP(term    , factor  , _LCC_MK_OP(STAR, * ), _LCC_MK_NZ(SLASH, / ), _LCC_MK_NZ(PERCENT, %))
_LCC_EVAL_OP(expr    , term    , _LCC_MK_OP(PLUS, + ), _LCC_MK_OP(MINUS, - ))
_LCC_EVAL_OP(shift   , expr    , _LCC_MK_OP(BSHL, <<), _LCC_MK_OP(BSHR , >>))
_LCC_EVAL_OP(order   , shift   , _LCC_MK_OP(GT  , > ), _LCC_MK_OP(GEQ  , >=), _LCC_MK_OP(LT, <), _LCC_MK_OP(LEQ, <=))
_LCC_EVAL_OP(equal   , order   , _LCC_MK_OP(EQ  , ==), _LCC_MK_OP(NEQ  , !=))
_LCC_EVAL_OP(bit_and , equal   , _LCC_MK_OP(BAND, & ))
_LCC_EVAL_OP(bit_xor , bit_and , _LCC_MK_OP(BXOR, ^ ))
_LCC_EVAL_OP(bit_or  , bit_xor , _LCC_MK_OP(BOR , | ))
_LCC_EVAL_OP(bool_and, bit_or  , _LCC_MK_OP(LAND, &&))
_LCC_EVAL_OP(bool_or , bool_and, _LCC_MK_OP(LOR , ||))

#pragma clang diagnostic pop

#undef _LCC_OP_0
#undef _LCC_OP_1
#undef _LCC_OP_OR_1_I
#undef _LCC_OP_OR_2_I
#undef _LCC_OP_OR_3_I
#undef _LCC_OP_OR_4_I
#undef _LCC_OP_OR_I
#undef _LCC_OP_OR
#undef _LCC_OP_CHECK_I
#undef _LCC_OP_CHECK
#undef _LCC_OP_CASE_I
#undef _LCC_OP_CASE
#undef _LCC_SLASH_Z
#undef _LCC_PERCENT_Z
#undef _LCC_OP_S
#undef _LCC_OP_Z
#undef _LCC_MK_OP
#undef _LCC_MK_NZ
#undef _LCC_EVAL_OP

static char _lcc_eval_factor(lcc_lexer_t *self, intmax_t *result, lcc_token_t **token, lcc_token_t *end)
{
    /* must have at least 1 token */
    if ((*token) == end)
    {
        _lcc_lexer_error(self, "Expected value in expression");
        return 0;
    }

    /* check for token type */
    switch ((*token)->type)
    {
        /* EOF, should not happen */
        case LCC_TK_EOF:
        {
            fprintf(stderr, "*** FATAL: EOF when parsing factor\n");
            abort();
        }

        /* normal macros are been substituted beforewards,
         * no identifier but undefined one may appear here,
         * which expands to zero according to the standard */
        case LCC_TK_IDENT:
        {
            /* check for undefined function-like macros */
            if (((*token)->next != end) &&
                ((*token)->next->type == LCC_TK_OPERATOR) &&
                ((*token)->next->operator == LCC_OP_LBRACKET))
            {
                _lcc_lexer_error(self, "Function-like macro '%s' is not defined", (*token)->ident->buf);
                return 0;
            }

            /* undefined object-like macro expands to zero */
            *token = (*token)->next;
            *result = 0;
            break;
        }

        /* literal constant */
        case LCC_TK_LITERAL:
        {
            /* check for literal type */
            switch ((*token)->literal.type)
            {
                /* integer constants */
                case LCC_LT_INT       : *result = (intmax_t)((*token)->literal.v_int);       break;
                case LCC_LT_LONG      : *result = (intmax_t)((*token)->literal.v_long);      break;
                case LCC_LT_LONGLONG  : *result = (intmax_t)((*token)->literal.v_longlong);  break;
                case LCC_LT_UINT      : *result = (intmax_t)((*token)->literal.v_uint);      break;
                case LCC_LT_ULONG     : *result = (intmax_t)((*token)->literal.v_ulong);     break;
                case LCC_LT_ULONGLONG : *result = (intmax_t)((*token)->literal.v_ulonglong); break;

                /* floating-point numbers are not allowed */
                case LCC_LT_FLOAT:
                case LCC_LT_DOUBLE:
                case LCC_LT_LONGDOUBLE:
                {
                    _lcc_lexer_error(self, "Floating-point in preprocessor expression");
                    return 0;
                }

                /* character sequence */
                case LCC_LT_CHAR:
                {
                    /* final value and character pointer */
                    intmax_t val = 0;
                    lcc_string_t *s = (*token)->literal.v_char;

                    /* convert to integer */
                    if (s->len <= sizeof(intmax_t))
                        memcpy(&val, s->buf, s->len);
                    else
                        memcpy(&val, s->buf + s->len - sizeof(intmax_t), sizeof(intmax_t));

                    /* skip the literal */
                    *token = (*token)->next;
                    *result = val;
                    break;
                }

                /* string literals are not allowed either */
                case LCC_LT_STRING:
                {
                    _lcc_lexer_error(self, "Invalid token at start of a preprocessor expression");
                    return 0;
                }
            }

            /* skip the literal */
            *token = (*token)->next;
            break;
        }

        /* keywords are generated after preprocessing,
         * thus must not appear here, or something went wrong */
        case LCC_TK_KEYWORD:
        {
            fprintf(stderr, "*** FATAL: keywords in preprocessor tokens\n");
            abort();
        }

        /* maybe prefix operators */
        case LCC_TK_OPERATOR:
        {
            /* save operator first */
            intmax_t val;
            lcc_operator_t op = (*token)->operator;

            /* skip the operator first */
            val = 0;
            *token = (*token)->next;

            /* check for sub-expression */
            if (op == LCC_OP_LBRACKET)
            {
                /* evaluate sub-expression */
                if (!(_lcc_eval_bool_or(self, result, token, end)))
                    return 0;

                /* should ends with ")" */
                if (((*token) == end) ||
                    ((*token)->type != LCC_TK_OPERATOR) ||
                    ((*token)->operator != LCC_OP_RBRACKET))
                {
                    _lcc_lexer_error(self, "Expected ')' in preprocessor expression");
                    return 0;
                }

                /* skip the ")" */
                *token = (*token)->next;
                break;
            }

            /* evaluate sub-factor */
            if (!(_lcc_eval_factor(self, &val, token, end)))
                return 0;

            /* apply operators */
            switch (op)
            {
                /* four possible unary operators */
                case LCC_OP_PLUS : break;
                case LCC_OP_MINUS: val = -val; break;
                case LCC_OP_BINV : val = ~val; break;
                case LCC_OP_LNOT : val = !val; break;

                /* other operators are not allowed */
                default:
                {
                    _lcc_lexer_error(self, "Invalid token at start of a preprocessor expression");
                    return 0;
                }
            }

            /* store the result */
            *result = val;
            break;
        }
    }

    /* evaluation successful */
    return 1;
}

static char _lcc_eval_tokens(lcc_lexer_t *self, intmax_t *result)
{
    lcc_token_t *token = self->tokens.next;
    return _lcc_eval_bool_or(self, result, &token, &(self->tokens));
}

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

#define _LCC_ENSURE_RAWINT(self, token, efmt, ...)          \
({                                                          \
    /* must be an long long literal */                      \
    if ((token->type != LCC_TK_LITERAL) ||                  \
        ((token->literal.type != LCC_LT_INT) &&             \
         (token->literal.type != LCC_LT_LONG) &&            \
         (token->literal.type != LCC_LT_LONGLONG) &&        \
         (token->literal.type != LCC_LT_UINT) &&            \
         (token->literal.type != LCC_LT_ULONG) &&           \
         (token->literal.type != LCC_LT_ULONGLONG)))        \
    {                                                       \
        _lcc_lexer_error(self, efmt, ## __VA_ARGS__);       \
        return;                                             \
    }                                                       \
                                                            \
    /* extract the token */                                 \
    token->literal.raw;                                     \
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

static inline char _lcc_push_file(lcc_lexer_t *self, lcc_string_t *path, char check_only)
{
    /* try load the file */
    char *name = path->buf;
    lcc_file_t file = lcc_file_open(name);

    /* check if it is loaded */
    if (file.flags & LCC_FF_INVALID)
        return 0;

    /* don't actually load under "check only" mode */
    if (check_only)
    {
        _lcc_file_free(&file);
        return 1;
    }

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
        lcc_string_append(s, (p = lcc_token_str(self->tokens.next)));
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

static char _lcc_load_include(lcc_lexer_t *self, lcc_string_t *fname, char check_only)
{
    /* for "#include_next" support */
    char load = 1;
    char next = 0;
    char found = 0;
    dev_t fdev = 0;
    ino_t fino = 0;

    /* check file path for "#include_next" */
    if ((fname->buf[0] == '/') &&
        (self->flags & LCC_LXDF_INCLUDE_NEXT))
    {
        self->flags &= ~LCC_LXDF_INCLUDE_NEXT;
        _lcc_lexer_warning(self, "#include_next with absolute path");
    }

    /* check for "#include_next" */
    if (self->flags & LCC_LXDF_INCLUDE_NEXT)
    {
        /* check file stack */
        if (self->files.count == 1)
            _lcc_lexer_warning(self, "#include_next in primary source file");

        /* calculate include path from current file */
        struct stat st = {};
        lcc_string_t *dir = _lcc_path_dirname(self->file->name);

        /* try to read it's info */
        if (stat(dir->buf, &st))
        {
            /* errors are muted under "check only" mode */
            if (check_only)
            {
                lcc_string_unref(dir);
                return 0;
            }

            /* otherwise raise error as intended */
            _lcc_lexer_error(self, "Cannot read directory '%s': [%d] %s", dir->buf, errno, strerror(errno));
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
            loaded = _lcc_push_file(self, path, check_only);
        }

        /* release the strings */
        lcc_string_unref(dir);
        lcc_string_unref(path);

        /* found, but not loaded, it's an error */
        if (found && !loaded)
        {
            /* errors are muted under "check only" mode */
            if (check_only)
                return 0;

            /* otherwise raise error as intended */
            _lcc_lexer_error(self, "Cannot open include file '%s': [%d] %s", fname->buf, errno, strerror(errno));
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
                loaded = _lcc_push_file(self, path, check_only);
            }
        }

        /* update load flag after loading this file */
        load = doload;
        lcc_string_unref(path);

        /* found, but not loaded, it's an error */
        if (found && !loaded)
        {
            /* errors are muted under "check only" mode */
            if (check_only)
                return 0;

            /* otherwise raise error as intended */
            _lcc_lexer_error(self, "Cannot open include file '%s': [%d] %s", fname->buf, errno, strerror(errno));
            return 0;
        }
    }

    /* found, and loaded successfully */
    if (found)
        return 1;

    /* errors are muted under "check only" mode */
    if (check_only)
        return 0;

    /* still not found, it's an error */
    _lcc_lexer_error(self, "Cannot open include file '%s': [%d] %s", fname->buf, ENOENT, strerror(ENOENT));
    return 0;
}

static lcc_token_t *_lcc_next_arg(lcc_token_t *begin, lcc_token_t *end, char allow_comma)
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
                (last->operator == LCC_OP_RBRACKET))
                return last;

            /* checking for comma delimiter */
            if (allow_comma &&
                (level == 0) &&
                (last->operator == LCC_OP_COMMA))
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

        /* both identifiers */
        if ((a->type == LCC_TK_IDENT) &&
            (b->type == LCC_TK_IDENT))
        {
            /* append with the next token */
            lcc_string_append(a->src, b->src);
            lcc_string_append(a->ident, b->ident);

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
                    _lcc_lexer_error(self, "'%s%s' is an invalid preprocessor token", op1, op2);
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
        lcc_string_t *tk1 = lcc_token_str(a);
        lcc_string_t *tk2 = lcc_token_str(b);

        /* throw the error */
        _lcc_lexer_error(self, "'%s%s' is an invalid preprocessor token", tk1->buf, tk2->buf);
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

static char _lcc_macro_scan(
    lcc_lexer_t *self,
    lcc_token_t *begin,
    lcc_token_t *end,
    char        *has_defined
);

static char _lcc_macro_attach(
    lcc_lexer_t *self,
    lcc_token_t *input,
    lcc_token_t *head,
    lcc_token_t *begin,
    lcc_token_t *end,
    char        *has_defined)
{
    /* header tokens */
    lcc_token_t *t = begin;
    lcc_token_t *h = head->prev;

    /* perform substitution */
    while (t != end)
    {
        lcc_token_attach(head, lcc_token_copy(t));
        t = t->next;
    }

    /* only expand when not concatenating */
    if (((input->next->type != LCC_TK_OPERATOR) ||
         (input->next->operator != LCC_OP_CONCAT)) &&
        !(_lcc_macro_scan(self, h->next, head, has_defined)))
        return 0;

    return 1;
}

static char _lcc_macro_func(
    lcc_lexer_t  *self,
    lcc_token_t  *head,
    _lcc_sym_t   *sym,
    size_t        argc,
    lcc_token_t **argv,
    char         *has_defined)
{
    /* substitution result */
    ssize_t m;
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

            /* beginning and ending tokens */
            lcc_token_t *to = argv[n + 1];
            lcc_token_t *from = argv[n]->next;

            /* attach token sequence, then substitute as needed */
            if (!(_lcc_macro_attach(self, p, head, from, to, has_defined)))
                return 0;

            /* skip the token */
            p = p->next;
            continue;
        }

        /* variadic argument substitution */
        if ((p->type == LCC_TK_IDENT) &&
            lcc_string_equals(p->ident, sym->vaname))
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
            {
                /* beginning and ending tokens */
                lcc_token_t *to = argv[argc];
                lcc_token_t *from = argv[sym->args.array.count]->next;

                /* attach token sequence, then substitute as needed */
                if (!(_lcc_macro_attach(self, p, head, from, to, has_defined)))
                    return 0;
            }

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
             (lcc_string_equals(p->next->ident, sym->vaname) &&                     /* or the name equals to varg name */
              ((argc <= sym->args.array.count) ||                                   /* and we don't have excess arguments to deal with */
               (argv[sym->args.array.count]->next == argv[argc])))))                /* or we just have an empty variadic list */
        {
            p = p->next->next;
            continue;
        }

        /* special case of ", ## <vargs>" */
        if ((p->type == LCC_TK_OPERATOR) &&                         /* first token must be an operator */
            (p->operator == LCC_OP_COMMA) &&                        /* which must be "," operator */
            (p->next->type == LCC_TK_OPERATOR) &&                   /* second token must also be an operator */
            (p->next->operator == LCC_OP_CONCAT) &&                 /* which must be "##" operator */
            (p->next->next->type == LCC_TK_IDENT) &&                /* third token must be an identifier */
            lcc_string_equals(p->next->next->ident, sym->vaname))   /* and is same with variadic argument name */
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

        /* concatenation, don't expand the following argument */
        if ((p->type == LCC_TK_OPERATOR) &&
            (p->operator == LCC_OP_CONCAT) &&
            (p->next->type == LCC_TK_IDENT) &&
            ((n = lcc_string_array_index(&(sym->args), p->next->ident)) >= 0))
        {
            /* skip the "##" and the identifier */
            lcc_token_attach(head, lcc_token_copy(p));
            p = p->next->next;

            /* beginning and ending tokens */
            lcc_token_t *to = argv[n + 1];
            lcc_token_t *from = argv[n]->next;

            /* perform substitution */
            while (from != to)
            {
                lcc_token_attach(head, lcc_token_copy(from));
                from = from->next;
            }

            /* continue scanning */
            continue;
        }

        /* apply stringnize */
        if ((p->type == LCC_TK_OPERATOR) &&
            (p->operator == LCC_OP_STRINGIZE))
        {
            /* must be an identifier after "#" */
            if (p->next->type != LCC_TK_IDENT)
            {
                _lcc_lexer_error(self, "'#' is not followed by a macro parameter");
                return 0;
            }

            /* it's an argument */
            if ((n = lcc_string_array_index(&(sym->args), p->next->ident)) >= 0)
                m = n + 1;

            /* variadic argument */
            else if (lcc_string_equals(p->next->ident, sym->vaname))
            {
                m = argc;
                n = sym->args.array.count;
            }

            /* otherwise it's an error */
            else
            {
                _lcc_lexer_error(self, "'#' is not followed by a macro parameter");
                return 0;
            }

            /* create a new string */
            lcc_token_t *t = argv[n]->next;
            lcc_string_t *v = lcc_string_new(0);
            lcc_string_t *s;

            /* concat each part of token strings */
            while (t != argv[m])
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

static char _lcc_macro_scan(lcc_lexer_t *self, lcc_token_t *begin, lcc_token_t *end, char *has_defined)
{
    /* first token */
    _lcc_sym_t *sym;
    lcc_token_t *next;
    lcc_token_t *token = begin;

    /* scan every token */
    while (token != end)
    {
        /* must be a valid macro */
        if ((token->type != LCC_TK_IDENT) ||                                /* must be an identifier */
            !(lcc_map_get(&(self->psyms), token->ident, (void **)&sym)) ||  /* must be defined */
            ((sym->flags & LCC_LXDF_DEFINE_SYS) &&                          /* special case of builtin macros */
             !(strcmp(sym->name->buf, "defined")) &&                        /* actually, "defined" macro */
             !(self->flags & (LCC_LXDN_IF | LCC_LXDN_ELIF))))               /* only available in "#if" or "#elif" */
        {
            token = token->next;
            continue;
        }

        /* call the extension if any */
        if (sym->ext)
        {
            if (!(sym->ext(self, &token, end)))
                return 0;
            else
                continue;
        }

        /* self-ref macros */
        if (token->ref || (sym->flags & LCC_LXDF_DEFINE_USING))
        {
            token->ref = 1;
            token = token->next;
            continue;
        }

        /* object-like macro */
        if (sym->flags & LCC_LXDF_DEFINE_O)
        {
            /* replace the name */
            _lcc_macro_disp(
                &token,
                &next,
                sym->body->next,
                sym->body
            );

            /* handle concatenation */
            if (!(_lcc_macro_cat(self, token, next)))
                return 0;
        }
        else
        {
            /* function-like macro, must follows a "(" operator */
            if ((token->next == end) ||
                (token->next->type != LCC_TK_OPERATOR) ||
                (token->next->operator != LCC_OP_LBRACKET))
            {
                token = token->next;
                continue;
            }

            /* argument begin */
            lcc_token_t *head;
            lcc_token_t *start = token->next->next;

            /* check for EOF */
            if (start == end)
            {
                _lcc_lexer_error(self, "Unterminated function-like macro invocation");
                return 0;
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
                if (!(delim = _lcc_next_arg(start, end, 1)))
                {
                    free(argvp);
                    _lcc_lexer_error(self, "Unterminated function-like macro invocation");
                    return 0;
                }

                /* expand argument buffer as needed */
                if (argp >= argcap - 1)
                {
                    argcap *= 2;
                    argvp = realloc(argvp, argcap * sizeof(lcc_token_t *));
                }

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
                return 0;
            }

            /* too many arguments */
            if ((argp > sym->args.array.count) &&
                !(sym->flags & LCC_LXDF_DEFINE_VAR))
            {
                free(argvp);
                _lcc_lexer_error(self, "Too many arguments provided to function-like macro invocation");
                return 0;
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
            if (!(_lcc_macro_func(self, (head = lcc_token_new()), sym, argp, argvp, has_defined)))
            {
                free(argvp);
                lcc_token_clear(head);
                return 0;
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

        /* check the substitution result as needed */
        if (!(*has_defined))
        {
            /* scan every token */
            for (lcc_token_t *p = token; p != next; p = p->next)
            {
                /* check for expanded "defined" macro */
                if ((p->type == LCC_TK_IDENT) && !(strcmp(p->ident->buf, "defined")))
                {
                    *has_defined = 1;
                    break;
                }
            }
        }

        /* move one token backward to prevent dangling pointer */
        token = token->prev;
        sym->flags |= LCC_LXDF_DEFINE_USING;

        /* scan again for nested macros */
        if (!(_lcc_macro_scan(self, token->next, next, has_defined)))
        {
            sym->flags &= ~LCC_LXDF_DEFINE_USING;
            return 0;
        }

        /* move to next token */
        token = next;
        sym->flags &= ~LCC_LXDF_DEFINE_USING;
    }

    /* substitution successful */
    return 1;
}

static char _lcc_macro_subst(lcc_lexer_t *self, lcc_token_t *begin, lcc_token_t *end)
{
    /* evaluate the macro */
    char warn = 0;
    char result = 0;
    lcc_token_t *head = begin->prev;

    /* do a macro prescan, the scan the token
     * sequence again for macros to be expanded */
    if (_lcc_macro_scan(self, begin, end, &warn))
        if (_lcc_macro_scan(self, head->next, end, &warn))
            result = 1;

    /* check for warnings */
    if (!warn)
        return result;

    /* throw out the warning */
    _lcc_lexer_warning(self, "Macro expansion producing 'defined' has undefined behavior");
    return result;
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
            _lcc_lexer_error(self, "'defined' is not a valid macro name");
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

static void _lcc_handle_condition(lcc_lexer_t *self)
{
    /* end of file encountered */
    if (self->flags & LCC_LXF_EOF)
    {
        _lcc_lexer_error(self, "Unterminated conditional directive");
        return;
    }

    /* end of line encountered, reset state if needed */
    while (self->flags & LCC_LXF_EOL)
    {
        /* check for current status */
        switch (self->condstate)
        {
            /* idle state */
            case LCC_LX_CONDSTATE_IDLE:
            {
                self->state = LCC_LX_STATE_SHIFT;
                self->substate = LCC_LX_SUBSTATE_NULL;
                return;
            }

            /* "#else" */
            case LCC_LX_CONDSTATE_ELSE:
            {
                /* not on root level */
                if (self->cond_level != 1)
                {
                    self->condstate = LCC_LX_CONDSTATE_IDLE;
                    break;
                }

                /* insert an "#else" preprocessor directive */
                self->flags |= LCC_LXF_DIRECTIVE;
                self->flags |= LCC_LXDN_ELSE;

                /* commit the directive */
                self->state = LCC_LX_STATE_COMMIT;
                self->substate = LCC_LX_SUBSTATE_NULL;
                return;
            }

            /* "#endif" */
            case LCC_LX_CONDSTATE_ENDIF:
            {
                /* not on root level */
                if (--self->cond_level)
                {
                    self->condstate = LCC_LX_CONDSTATE_IDLE;
                    break;
                }

                /* insert an "#endif" preprocessor directive */
                self->flags |= LCC_LXF_DIRECTIVE;
                self->flags |= LCC_LXDN_ENDIF;

                /* commit the directive */
                self->state = LCC_LX_STATE_COMMIT;
                self->substate = LCC_LX_SUBSTATE_NULL;
                return;
            }

            /* "#ifdef" or "#ifndef" */
            case LCC_LX_CONDSTATE_IFDEF:
            {
                _lcc_lexer_error(self, "Missing macro name");
                return;
            }

            /* new-line is permitted in block comments */
            case LCC_LX_CONDSTATE_BLOCK_COMMENT:
            case LCC_LX_CONDSTATE_BLOCK_COMMENT_END:
            {
                self->state = LCC_LX_STATE_SHIFT;
                self->substate = LCC_LX_SUBSTATE_NULL;
                self->condstate = LCC_LX_CONDSTATE_BLOCK_COMMENT;
                return;
            }

            /* reset state for other status */
            default:
            {
                self->condstate = self->savestate;
                self->savestate = LCC_LX_CONDSTATE_IDLE;
                break;
            }
        }
    }

    /* check condition state */
    while (1)
    {
        char again = 0;
        switch (self->condstate)
        {
            /* idle state, drop characters as needed */
            case LCC_LX_CONDSTATE_IDLE:
            {
                switch (self->ch)
                {
                    /* "/" or "#" encountered */
                    case '/': self->condstate = LCC_LX_CONDSTATE_SLASH; break;
                    case '#': self->condstate = LCC_LX_CONDSTATE_DIRECTIVE; break;

                    /* other characters */
                    default:
                    {
                        /* not a space, this is a normal source line */
                        if (!(isspace(self->ch)))
                            self->condstate = LCC_LX_CONDSTATE_SOURCE;

                        break;
                    }
                }

                /* IDLE state can only be transfered to SOURCE state */
                self->savestate = LCC_LX_CONDSTATE_SOURCE;
                break;
            }

            /* "/" encountered, maybe line comment or block comment */
            case LCC_LX_CONDSTATE_SLASH:
            {
                switch (self->ch)
                {
                    /* line comments or block comments */
                    case '/': self->condstate = LCC_LX_CONDSTATE_LINE_COMMENT; break;
                    case '*': self->condstate = LCC_LX_CONDSTATE_BLOCK_COMMENT; break;

                    /* everything else */
                    default:
                    {
                        again = 1;
                        self->condstate = self->savestate;
                        break;
                    }
                }

                break;
            }

            /* normal source line */
            case LCC_LX_CONDSTATE_SOURCE:
            {
                /* "/" encountered, maybe line comment or block comment */
                if (self->ch == '/')
                {
                    self->condstate = LCC_LX_CONDSTATE_SLASH;
                    self->savestate = LCC_LX_CONDSTATE_SOURCE;
                }

                break;
            }

            /* "#" encountered, maybe a directive */
            case LCC_LX_CONDSTATE_DIRECTIVE:
            {
                switch (self->ch)
                {
                    /* "e" or "i" encountered */
                    case 'e': self->condstate = LCC_LX_CONDSTATE_E; break;
                    case 'i': self->condstate = LCC_LX_CONDSTATE_I; break;

                    /* other characters */
                    default:
                    {
                        /* not a space, this is a normal source line */
                        if (!(isspace(self->ch)))
                            self->condstate = LCC_LX_CONDSTATE_SOURCE;

                        break;
                    }
                }

                break;
            }

            /* line comment, keep this state until next line */
            case LCC_LX_CONDSTATE_LINE_COMMENT:
                break;

            /* block comment, check for "*" character */
            case LCC_LX_CONDSTATE_BLOCK_COMMENT:
            {
                /* "*" encountered, maybe end of block comment */
                if (self->ch == '*')
                    self->condstate = LCC_LX_CONDSTATE_BLOCK_COMMENT_END;

                break;
            }

            /* maybe end of block comment, only if we meet "/" */
            case LCC_LX_CONDSTATE_BLOCK_COMMENT_END:
            {
                /* "/" encountered, block comment ends here */
                if (self->ch == '/')
                    self->condstate = self->savestate;

                /* otherwise it's a false trigger, so revert
                 * back to block comment state, but we need to check
                 * such state that have multiple stars before a slash */
                else if (self->ch != '*')
                    self->condstate = LCC_LX_CONDSTATE_BLOCK_COMMENT;

                break;
            }

            /* "#e(ndif|l(se|if))" */
            case LCC_LX_CONDSTATE_E:
            {
                switch (self->ch)
                {
                    case 'n': self->condstate = LCC_LX_CONDSTATE_EN; break;
                    case 'l': self->condstate = LCC_LX_CONDSTATE_EL; break;
                    default : self->condstate = LCC_LX_CONDSTATE_SOURCE; break;
                }

                break;
            }

            /* "#el(se|if) */
            case LCC_LX_CONDSTATE_EL:
            {
                switch (self->ch)
                {
                    case 'i': self->condstate = LCC_LX_CONDSTATE_ELI; break;
                    case 's': self->condstate = LCC_LX_CONDSTATE_ELS; break;
                    default : self->condstate = LCC_LX_CONDSTATE_SOURCE; break;
                }

                break;
            }

            /* "#els(e)" */
            case LCC_LX_CONDSTATE_ELS:
            {
                self->condstate = (self->ch == 'e') ? LCC_LX_CONDSTATE_ELSE : LCC_LX_CONDSTATE_SOURCE;
                break;
            }

            /* "#eli(f)" */
            case LCC_LX_CONDSTATE_ELI:
            {
                self->condstate = (self->ch == 'f') ? LCC_LX_CONDSTATE_ELIF : LCC_LX_CONDSTATE_SOURCE;
                break;
            }

            /* "#elif" */
            case LCC_LX_CONDSTATE_ELIF:
            {
                /* should be a space character */
                if (!(isspace(self->ch)) || (self->cond_level != 1))
                {
                    self->condstate = LCC_LX_CONDSTATE_SOURCE;
                    break;
                }

                /* insert an "#elif" preprocessor directive */
                self->flags |= LCC_LXF_DIRECTIVE;
                self->flags |= LCC_LXDN_ELIF;
                break;
            }

            /* "#en(dif)" */
            case LCC_LX_CONDSTATE_EN:
            {
                self->condstate = (self->ch == 'd') ? LCC_LX_CONDSTATE_END : LCC_LX_CONDSTATE_SOURCE;
                break;
            }

            /* "#end(if)" */
            case LCC_LX_CONDSTATE_END:
            {
                self->condstate = (self->ch == 'i') ? LCC_LX_CONDSTATE_ENDI : LCC_LX_CONDSTATE_SOURCE;
                break;
            }

            /* "#endi(f)" */
            case LCC_LX_CONDSTATE_ENDI:
            {
                self->condstate = (self->ch == 'f') ? LCC_LX_CONDSTATE_ENDIF : LCC_LX_CONDSTATE_SOURCE;
                break;
            }

            /* "#else" and "#endif" */
            case LCC_LX_CONDSTATE_ELSE:
            case LCC_LX_CONDSTATE_ENDIF:
            {
                /* should be spaces */
                if (isspace(self->ch))
                    break;

                /* not even comments */
                if (self->ch != '/')
                {
                    _lcc_lexer_error(self, "Redundent directive parameter");
                    return;
                }

                /* go parsing comments */
                self->savestate = self->condstate;
                self->condstate = LCC_LX_CONDSTATE_SLASH;
                break;
            }

            /* "#i(f((n)?def)?) */
            case LCC_LX_CONDSTATE_I:
            {
                self->condstate = (self->ch == 'f') ? LCC_LX_CONDSTATE_IF : LCC_LX_CONDSTATE_SOURCE;
                break;
            }

            /* "#if((n)?def)?" */
            case LCC_LX_CONDSTATE_IF:
            {
                switch (self->ch)
                {
                    /* "d" or "n" encountered */
                    case 'd': self->condstate = LCC_LX_CONDSTATE_IFD; break;
                    case 'n': self->condstate = LCC_LX_CONDSTATE_IFN; break;

                    /* other characters */
                    default:
                    {
                        /* is a space, commit as "#if" */
                        if (isspace(self->ch))
                            self->cond_level++;

                        /* mark as source line */
                        self->condstate = LCC_LX_CONDSTATE_SOURCE;
                        break;
                    }
                }

                break;
            }

            /* "#ifn(def)" */
            case LCC_LX_CONDSTATE_IFN:
            {
                self->condstate = (self->ch == 'd') ? LCC_LX_CONDSTATE_IFD : LCC_LX_CONDSTATE_SOURCE;
                break;
            }

            /* "#ifd(ef)" or "#ifnd(ef)" */
            case LCC_LX_CONDSTATE_IFD:
            {
                self->condstate = (self->ch == 'e') ? LCC_LX_CONDSTATE_IFDE : LCC_LX_CONDSTATE_SOURCE;
                break;
            }

            /* "#ifde(f)" or "#ifnde(f)" */
            case LCC_LX_CONDSTATE_IFDE:
            {
                self->condstate = (self->ch == 'f') ? LCC_LX_CONDSTATE_IFDEF : LCC_LX_CONDSTATE_SOURCE;
                break;
            }

            /* "#ifdef" or "#ifndef" */
            case LCC_LX_CONDSTATE_IFDEF:
            {
                /* should have a space */
                if (isspace(self->ch))
                    break;

                /* maybe comments */
                if (self->ch == '/')
                {
                    self->condstate = LCC_LX_CONDSTATE_SLASH;
                    self->savestate = LCC_LX_CONDSTATE_IFDEF;
                    break;
                }

                /* mark as source line */
                self->condstate = LCC_LX_CONDSTATE_SOURCE;
                self->cond_level++;
                break;
            }
        }

        /* check for "try again" flag */
        if (again)
            continue;

        /* shift next character */
        self->state = LCC_LX_STATE_SHIFT;
        self->substate = LCC_LX_SUBSTATE_NULL;
        break;
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
                _lcc_lexer_error(self, "Unknown compiler directive '%s'", name->buf);
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

            /* remove the '"' on either side */
            fname->buf[--fname->len] = 0;
            memmove(fname->buf, fname->buf + 1, fname->len--);

            /* load the include file */
            if (_lcc_load_include(self, fname, 0))
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

            /* create a new symbol */
            _lcc_sym_t old;
            _lcc_sym_t sym = {
                .ext = NULL,
                .body = lcc_token_new(),
                .name = self->macro_name,
                .args = self->macro_args,
                .flags = self->flags,
                .vaname = self->macro_vaname,
            };

            /* make sure we have at least 1 flag set */
            if (!(sym.flags & LCC_LXDF_DEFINE_F))
                sym.flags |= LCC_LXDF_DEFINE_O;

            /* check for pre-included file */
            if (self->file->flags & LCC_FF_SYS)
                sym.flags |= LCC_LXDF_DEFINE_SYS;

            /* move everything remaining to macro body */
            if (self->tokens.next != &(self->tokens))
            {
                sym.body->prev = self->tokens.prev;
                sym.body->next = self->tokens.next;
                self->tokens.prev->next = sym.body;
                self->tokens.next->prev = sym.body;
                self->tokens.prev = &(self->tokens);
                self->tokens.next = &(self->tokens);
            }

            /* add to predefined symbols */
            if (!(lcc_map_set(&(self->psyms), self->macro_name, &old, &sym)))
                break;

            /* no warnings for system macros overriding user macros */
            if (sym.flags & LCC_LXDF_DEFINE_SYS)
            {
                _lcc_sym_free(&old);
                break;
            }

            /* check for built-in macro */
            if (old.flags & LCC_LXDF_DEFINE_SYS)
            {
                _lcc_sym_free(&old);
                _lcc_lexer_warning(self, "Redefining builtin macro '%s'", self->macro_name->buf);
                break;
            }

            /* two headers */
            lcc_token_t *a = sym.body->next;
            lcc_token_t *b = old.body->next;

            /* compare each token */
            while ((a != sym.body) && (b != old.body) && lcc_token_equals(a, b))
            {
                a = a->next;
                b = b->next;
            }

            /* redefine to same token sequence doesn't considered as "macro redefined" */
            if ((a != sym.body) || (b != old.body))
                _lcc_lexer_warning(self, "Symbol '%s' redefined", self->macro_name->buf);

            /* release the old symbol */
            _lcc_sym_free(&old);
            break;
        }

        /* "#undef" directive */
        case LCC_LXDN_UNDEF:
        {
            /* extract the macro name */
            _lcc_sym_t sym;
            lcc_token_t *token = _LCC_FETCH_TOKEN(self, "Missing macro name");
            lcc_string_t *macro = _LCC_ENSURE_IDENT(self, token, "Macro name must be an identifier");

            /* check for macro name */
            if (!(strcmp(macro->buf, "defined")))
            {
                lcc_token_free(token);
                _lcc_lexer_error(self, "'defined' is not a valid macro name");
                return;
            }

            /* undefine the macro */
            if (!(lcc_map_pop(&(self->psyms), macro, &sym)))
            {
                lcc_token_free(token);
                break;
            }

            /* check for macro type */
            if (sym.flags & LCC_LXDF_DEFINE_SYS)
                _lcc_lexer_warning(self, "Undefining builtin macro '%s'", macro->buf);

            /* release the token */
            _lcc_sym_free(&sym);
            lcc_token_free(token);
            break;
        }

        /* "#if" and "#elif" directives */
        case LCC_LXDN_IF:
        case LCC_LXDN_ELIF:
        {
            _lcc_val_t val = {};
            _lcc_val_t *pval = &val;
            lcc_array_t *stack = &(self->eval_stack);

            /* "#elif" branch */
            if ((self->flags & LCC_LXDN_MASK) == LCC_LXDN_ELIF)
            {
                /* read the stack top */
                if (!(pval = lcc_array_top(stack)))
                {
                    _lcc_lexer_error(self, "#elif without #if");
                    return;
                }

                /* discard "#elif" branch as needed */
                if (pval->discard)
                {
                    /* clear all tokens */
                    while (self->tokens.next != &(self->tokens))
                        lcc_token_free(self->tokens.next);

                    /* force an inactive branch */
                    pval->value = 0;
                    self->condstate = LCC_LX_CONDSTATE_IDLE;
                    self->cond_level = 1;
                    break;
                }
            }

            /* perform a substitution, then evaluate token sequence */
            if (!(_lcc_macro_subst(self, self->tokens.next, &(self->tokens))) ||
                !(_lcc_eval_tokens(self, &(pval->value))))
                return;

            /* clear all tokens after evaluation */
            while (self->tokens.next != &(self->tokens))
                lcc_token_free(self->tokens.next);

            /* set discard flags for remaining branches */
            if (pval->value)
                pval->discard = 1;

            /* append new if "#if" */
            if ((self->flags & LCC_LXDN_MASK) == LCC_LXDN_IF)
                lcc_array_append(&(self->eval_stack), pval);

            /* push new value, and set conditional lexer state */
            self->condstate = LCC_LX_CONDSTATE_IDLE;
            self->cond_level = 1;
            break;
        }

        /* "#ifdef" and "#ifndef" directives */
        case LCC_LXDN_IFDEF:
        case LCC_LXDN_IFNDEF:
        {
            /* extract the macro name */
            lcc_token_t *token = _LCC_FETCH_TOKEN(self, "Missing macro name");
            lcc_string_t *macro = _LCC_ENSURE_IDENT(self, token, "Macro name must be an identifier");

            /* check for macro name */
            if (!(strcmp(macro->buf, "defined")))
            {
                lcc_token_free(token);
                _lcc_lexer_error(self, "'defined' is not a valid macro name");
                return;
            }

            /* check for defination */
            char has_sym = lcc_map_get(&(self->psyms), macro, NULL);
            char flag_ifdef = ((self->flags & LCC_LXDN_MASK) == LCC_LXDN_IFDEF);

            /* build a new value */
            _lcc_val_t value = {
                .value = (has_sym == flag_ifdef) ? 1 : 0,
                .discard = (has_sym == flag_ifdef),
            };

            /* conditional lexer state */
            self->condstate = LCC_LX_CONDSTATE_IDLE;
            self->cond_level = 1;

            /* release the token, and push the new value onto eval stack */
            lcc_token_free(token);
            lcc_array_append(&(self->eval_stack), &value);
            break;
        }

        /* "#else" directive */
        case LCC_LXDN_ELSE:
        {
            /* extract the last value on stack */
            _lcc_val_t *value;
            lcc_array_t *stack = &(self->eval_stack);

            /* nothing on stack, that's an error */
            if (!(value = lcc_array_top(stack)))
            {
                _lcc_lexer_error(self, "#else without #if");
                return;
            }

            /* invert condition to emulate "else" if not discarding */
            if (value->discard)
                value->value = 0;
            else
                value->value = !(value->value);

            /* restart conditional lexer */
            self->condstate = LCC_LX_CONDSTATE_IDLE;
            self->cond_level = 1;
            break;
        }

        /* "#endif" directive */
        case LCC_LXDN_ENDIF:
        {
            /* pop the last value from eval stack */
            if (lcc_array_pop(&(self->eval_stack), NULL))
                break;

            /* nothing on stack, that's an error */
            _lcc_lexer_error(self, "#endif without #if");
            return;
        }

        /* "#pragma" directive */
        case LCC_LXDN_PRAGMA:
        {
            /* doesn't care what tokens given */
            while (self->tokens.next != &(self->tokens))
                lcc_token_free(self->tokens.next);

            // TODO: impelemnt this
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
            /* perform a macro expansion first */
            if (!(_lcc_macro_subst(self, self->tokens.next, &(self->tokens))))
                return;

            /* extract the line number */
            lcc_token_t *token = _LCC_FETCH_TOKEN(self, "Missing line number");
            lcc_string_t *lineno = _LCC_ENSURE_RAWINT(self, token, "#line directive requires a positive integer argument");

            /* also extract file name if any */
            if (token->next != &(self->tokens))
            {
                /* extract the token */
                lcc_token_t *next = token->next;
                lcc_string_t *fname = _LCC_ENSURE_RAWSTR(self, next, "Invalid filename for #line directive");

                /* extract the file name */
                fname = lcc_string_ref(fname);
                lcc_token_free(token->next);

                /* remove '"' on either side */
                fname->buf[--fname->len] = 0;
                memmove(fname->buf, fname->buf + 1, fname->len--);

                /* replace current display name */
                lcc_string_unref(self->file->display);
                self->file->display = fname;
            }

            /* check line number */
            if ((lineno->len == 1) && (lineno->buf[0] == '0'))
                _lcc_lexer_warning(self, "#line directive interprets number as decimal, not octal");

            /* try convert to integer */
            char *endp = NULL;
            long long val = strtoll(lineno->buf, &endp, 10);

            /* check for conversion result */
            if (endp && (*endp))
            {
                lcc_token_free(token);
                _lcc_lexer_error(self, "#line directive requires a simple digit sequence");
                return;
            }

            /* update line number, then release the token */
            self->file->offset = val - self->file->row - 1;
            lcc_token_free(token);
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

#define _LCC_MACRO_EXT(name)                \
    static char __lcc_macro_ext_ ## name(   \
        lcc_lexer_t *self,                  \
        lcc_token_t **begin,                \
        lcc_token_t *end                    \
    )

#define _LCC_MACRO_EXT_P(name)              \
    static char __lcc_macro_ext_ ## name(   \
        lcc_lexer_t *self,                  \
        lcc_token_t **begin,                \
        lcc_token_t *end                    \
    )                                       \
    {                                       \
        *begin = (*begin)->next;            \
        return 1;                           \
    }

#define _LCC_ADD_MACRO_EXT_F(ext_name) {                        \
    _lcc_sym_t __sym_macro_ext_ ## ext_name = {                 \
        .ext = &__lcc_macro_ext_ ## ext_name,                   \
        .body = NULL,                                           \
        .name = lcc_string_from(# ext_name),                    \
        .args = LCC_STRING_ARRAY_STATIC_INIT,                   \
        .flags = LCC_LXDF_DEFINE_SYS | LCC_LXDF_DEFINE_F,       \
        .vaname = NULL,                                         \
    };                                                          \
                                                                \
    lcc_map_set(                                                \
        &(self->psyms),                                         \
        __sym_macro_ext_ ## ext_name.name,                      \
        NULL,                                                   \
        &__sym_macro_ext_ ## ext_name                           \
    );                                                          \
}

#define _LCC_ADD_MACRO_EXT_O(ext_name) {                        \
    _lcc_sym_t __sym_macro_ext_ ## ext_name = {                 \
        .ext = &__lcc_macro_ext_ ## ext_name,                   \
        .body = NULL,                                           \
        .name = lcc_string_from(# ext_name),                    \
        .args = LCC_STRING_ARRAY_STATIC_INIT,                   \
        .flags = LCC_LXDF_DEFINE_SYS | LCC_LXDF_DEFINE_O,       \
        .vaname = NULL,                                         \
    };                                                          \
                                                                \
    lcc_map_set(                                                \
        &(self->psyms),                                         \
        __sym_macro_ext_ ## ext_name.name,                      \
        NULL,                                                   \
        &__sym_macro_ext_ ## ext_name                           \
    );                                                          \
}

static inline void _lcc_range_subst(lcc_token_t **from, lcc_token_t *to, lcc_token_t *token)
{
    /* remove old tokens */
    while ((*from)->next != to)
        lcc_token_free((*from)->next);

    /* replace the old token */
    lcc_token_free(*from);
    lcc_token_attach((*from = to), token);
}

static inline void _lcc_single_subst(lcc_token_t **at, lcc_token_t *token)
{
    /* substitute a single token (from self to next) */
    _lcc_range_subst(at, (*at)->next, token);
}

static inline lcc_string_t *_lcc_extract_ident(
    lcc_lexer_t  *self,
    lcc_token_t **begin,
    lcc_token_t  *end,
    lcc_token_t **tail,
    const char   *name)
{
    /* token sequences */
    lcc_token_t *n0 = *begin;
    lcc_token_t *n1 = n0->next;
    lcc_token_t *n2 = n1->next;
    lcc_token_t *n3 = n2->next;
    lcc_string_t *ident;

    /* the first token must be an identifier */
    if (n0->type != LCC_TK_IDENT)
    {
        fprintf(stderr, "*** FATAL: non-identifier macro\n");
        abort();
    }

    /* check for beginning "(" */
    if ((n1 == end) ||
        (n1->type != LCC_TK_OPERATOR) ||
        (n1->operator != LCC_OP_LBRACKET))
    {
        _lcc_lexer_error(self, "Missing '(' after '%s'", name);
        return NULL;
    }

    /* check for EOL */
    if ((n2 == end) || (n3 == end))
    {
        _lcc_lexer_error(self, "Unterminated function-like macro invocation");
        return NULL;
    }

    /* check for identifier type and ending ")" */
    if ((n2->type != LCC_TK_IDENT) ||
        (n3->type != LCC_TK_OPERATOR) ||
        (n3->operator != LCC_OP_RBRACKET))
    {
        _lcc_lexer_error(self, "Builtin feature check macro requires a single parenthesized identifier");
        return NULL;
    }

    /* store the tail and name */
    *tail = *begin = n3->next;
    ident = lcc_string_ref(n2->ident);

    /* release tokens */
    lcc_token_free(n0);
    lcc_token_free(n1);
    lcc_token_free(n2);
    lcc_token_free(n3);
    return ident;
}

/** Object-like built-in macros **/

_LCC_MACRO_EXT(__FILE__)
{
    /* dump file name */
    lcc_string_t *src = lcc_string_ref(self->fname);
    lcc_string_t *value = lcc_string_ref(self->fname);

    /* replace the old token */
    _lcc_single_subst(begin, lcc_token_from_raw(src, value));
    return 1;
}

_LCC_MACRO_EXT(__LINE__)
{
    _lcc_single_subst(begin, lcc_token_from_int(self->row));
    return 1;
}

_LCC_MACRO_EXT(__DATE__)
{
    char s[32] = {};
    time_t ts;
    struct tm tm;

    /* read current time */
    if ((time(&ts) < 0) || !(localtime_r(&ts, &tm)))
        strcpy(s, "??? ?? ????");
    else
        strftime(s, sizeof(s), "%b %e %Y", &tm);

    /* create date string */
    lcc_string_t *src = lcc_string_from(s);
    lcc_string_t *value = lcc_string_from(s);

    /* replace the old token */
    _lcc_single_subst(begin, lcc_token_from_raw(src, value));
    return 1;
}

_LCC_MACRO_EXT(__TIME__)
{
    char s[32] = {};
    time_t ts;
    struct tm tm;

    /* read current time */
    if ((time(&ts) < 0) || !(localtime_r(&ts, &tm)))
        strcpy(s, "??:??:??");
    else
        strftime(s, sizeof(s), "%T", &tm);

    /* create time string */
    lcc_string_t *src = lcc_string_from(s);
    lcc_string_t *value = lcc_string_from(s);

    /* replace the old token */
    _lcc_single_subst(begin, lcc_token_from_raw(src, value));
    return 1;
}

_LCC_MACRO_EXT(__TIMESTAMP__)
{
    char s[32] = {};
    struct tm tm;
    struct stat st;

    /* read current time */
    if (stat(self->fname->buf, &st) ||
        !(localtime_r(&(st.st_mtime), &tm)))
        strcpy(s, "??? ??? ?? ??:??:?? ????");
    else
        strftime(s, sizeof(s), "%a %b %e %T %Y", &tm);

    /* create timestamp string */
    lcc_string_t *src = lcc_string_from(s);
    lcc_string_t *value = lcc_string_from(s);

    /* replace the old token */
    _lcc_single_subst(begin, lcc_token_from_raw(src, value));
    return 1;
}

_LCC_MACRO_EXT(__BASE_FILE__)
{
    /* dump display name */
    lcc_file_t *fp = self->files.items;
    lcc_string_t *src = lcc_string_ref(fp->display);
    lcc_string_t *value = lcc_string_ref(fp->display);

    /* replace the old token */
    _lcc_single_subst(begin, lcc_token_from_raw(src, value));
    return 1;
}

_LCC_MACRO_EXT(__INCLUDE_LEVEL__)
{
    _lcc_single_subst(begin, lcc_token_from_int(self->files.count - 1));
    return 1;
}

/** Function-like built-in macros **/

_LCC_MACRO_EXT(defined)
{
    char brk;
    lcc_token_t *token = *begin;
    lcc_string_t *ident;

    /* check for next token */
    if ((token = token->next) == end)
    {
        _lcc_lexer_error(self, "Macro name missing");
        return 0;
    }

    /* check for operator type */
    switch (token->type)
    {
        /* "defined xxx" */
        case LCC_TK_IDENT:
        {
            brk = 0;
            token = token->next;
            ident = lcc_string_ref(token->prev->ident);
            break;
        }

        /* maybe "defined(xxx)" */
        case LCC_TK_OPERATOR:
        {
            /* must be a "(" in this case */
            if (token->operator != LCC_OP_LBRACKET)
                goto _lcc_factor_not_ident;

            /* skip the "(" */
            brk = 1;
            token = token->next;

            /* then must be an identifier */
            if (token->type != LCC_TK_IDENT)
                goto _lcc_factor_not_ident;

            /* extract the identifier */
            token = token->next;
            ident = lcc_string_ref(token->prev->ident);
            break;
        }

            /* otherwise it's an error */
        default:
        _lcc_factor_not_ident:
        {
            _lcc_lexer_error(self, "Macro name must be an identifier");
            return 0;
        }
    }

    /* close bracket required */
    if (brk)
    {
        /* check for close bracket */
        if ((token->type != LCC_TK_OPERATOR) ||
            (token->operator != LCC_OP_RBRACKET))
        {
            _lcc_lexer_error(self, "Missing ')' after 'defined'");
            return 0;
        }

        /* skip the ")" */
        token = token->next;
    }

    /* check for "defined" */
    if (!(strcmp(ident->buf, "defined")))
    {
        _lcc_lexer_error(self, "'defined' is not a valid macro name");
        lcc_string_unref(ident);
        return 0;
    }

    /* replace the old token */
    _lcc_range_subst(begin, token, lcc_token_from_int(lcc_map_get(&(self->psyms), ident, NULL)));
    lcc_string_unref(ident);
    return 1;
}

#define _LCC_FEATURE_CHECK(type)                                                                \
{                                                                                               \
    /* extract the identifier */                                                                \
    lcc_token_t *tail;                                                                          \
    lcc_string_t *ident = _lcc_extract_ident(self, begin, end, &tail, "__has_" #type);          \
                                                                                                \
    /* check for errors */                                                                      \
    if (!ident)                                                                                 \
        return 0;                                                                               \
                                                                                                \
    /* attach the result */                                                                     \
    lcc_token_attach(tail, lcc_token_from_int(lcc_set_contains(&(self->type ## s), ident)));    \
    lcc_string_unref(ident);                                                                    \
    return 1;                                                                                   \
}

_LCC_MACRO_EXT(__has_builtin  ) _LCC_FEATURE_CHECK(builtin  )
_LCC_MACRO_EXT(__has_feature  ) _LCC_FEATURE_CHECK(feature  )
_LCC_MACRO_EXT(__has_extension) _LCC_FEATURE_CHECK(extension)

#undef _LCC_FEATURE_CHECK

static char _lcc_check_include(
    lcc_lexer_t *self,
    lcc_token_t **begin,
    lcc_token_t *end,
    char is_next)
{
    /* skip "__has_include" */
    lcc_token_t *next;
    lcc_token_t *head = (*begin)->next;

    /* token source and file path */
    char is_sys = 1;
    char is_macro = 0;
    lcc_string_t *src;
    lcc_string_t *path;

    /* check for next token */
    if ((head == end) ||
        (head->type != LCC_TK_OPERATOR) ||
        (head->operator != LCC_OP_LBRACKET))
    {
        _lcc_lexer_error(self, "Missing '(' after '__has_include'");
        return 0;
    }

    /* may need macro expansion */
    if (((head->next->type != LCC_TK_LITERAL) ||
         (head->next->literal.type != LCC_LT_STRING)) &&
        ((head->next->type != LCC_TK_OPERATOR) ||
         (head->next->operator != LCC_OP_LT)))
    {
        /* could only be identifiers */
        if (head->next->type != LCC_TK_IDENT)
        {
            _lcc_lexer_error(self, "Expected \"FILENAME\" or <FILENAME>");
            return 0;
        }

        /* search for next token */
        if (!(next = _lcc_next_arg(head->next, end, 0)))
        {
            _lcc_lexer_error(self, "Expected value in expression");
            return 0;
        }

        /* perform substitution */
        if (!(is_macro = _lcc_macro_subst(self, head->next, next)))
            return 0;
    }

    /* move to next token */
    if ((next = head->next) == end)
    {
        _lcc_lexer_error(self, "Expected value in expression");
        return 0;
    }

    /* __has_include[_next]("...") */
    if ((next->type == LCC_TK_LITERAL) &&
        (next->literal.type == LCC_LT_STRING))
    {
        /* copy the raw string */
        path = lcc_string_copy(next->literal.raw);
        next = next->next;

        /* remove '"' on either side */
        is_sys = 0;
        path->buf[--path->len] = 0;
        memmove(path->buf, path->buf + 1, path->len--);
    }

    /* __has_include[_next](<...>) */
    else if ((next->type == LCC_TK_OPERATOR) &&
             (next->operator == LCC_OP_LT))
    {
        /* create a new string */
        path = lcc_string_new(0);
        next = next->next;

        /* concat every token before ">" */
        while ((next != end) &&
               ((next->type != LCC_TK_OPERATOR) ||
                (next->operator != LCC_OP_GT)))
        {
            lcc_string_append(path, next->src);
            next = next->next;
        }

        /* check for end of tokens */
        if (next == end)
        {
            lcc_string_unref(path);
            _lcc_lexer_error(self, "Expected \"FILENAME\" or <FILENAME>");
            return 0;
        }

        /* remove whitespaces when substituted from macro */
        if (is_macro)
        {
            src = path;
            path = lcc_string_trim(src);
            lcc_string_unref(src);
        }

        /* skip the ">" */
        next = next->next;
    }

    /* otherwise it's an error */
    else
    {
        _lcc_lexer_error(self, "Expected \"FILENAME\" or <FILENAME>");
        return 0;
    }

    /* must be a ")" */
    if ((next->type != LCC_TK_OPERATOR) ||
        (next->operator != LCC_OP_RBRACKET))
    {
        lcc_string_unref(path);
        _lcc_lexer_error(self, "Missing ')' after '__has_include'");
        return 0;
    }

    /* set flags */
    if (is_sys) self->flags |= LCC_LXDF_INCLUDE_SYS;
    if (is_next) self->flags |= LCC_LXDF_INCLUDE_NEXT;

    /* try load the include file */
    lcc_token_t *tail = next->next;
    lcc_token_t *token = lcc_token_from_int(_lcc_load_include(self, path, 1));

    /* reset flags */
    if (is_sys) self->flags &= ~LCC_LXDF_INCLUDE_SYS;
    if (is_next) self->flags &= ~LCC_LXDF_INCLUDE_NEXT;

    /* replace the old token */
    _lcc_range_subst(begin, tail, token);
    lcc_string_unref(path);
    return 1;
}

_LCC_MACRO_EXT(__has_include)
{
    /* check as "#include" */
    return _lcc_check_include(self, begin, end, 0);
}

_LCC_MACRO_EXT(__has_include_next)
{
    /* check as "#include_next" */
    return _lcc_check_include(self, begin, end, 1);
}

/** Pseudo-macro "__func__" and "__FUNCTION__"
 *
 * neither of them is a macro
 * the preprocessor does not know the name of the current function
 * they will be replaced by the actual function name during syntax parsing
 */
_LCC_MACRO_EXT_P(__func__);
_LCC_MACRO_EXT_P(__FUNCTION__);

static inline char _lcc_check_drop_char(lcc_lexer_t *self)
{
    /* most inner compiling section condition */
    _lcc_val_t *value;
    lcc_array_t *stack = &(self->eval_stack);

    /* outside of condition compiling section,
     * or parsing compiler directives */
    if (!(value = lcc_array_top(stack)) ||
        (self->flags & LCC_LXF_DIRECTIVE))
        return 0;
    else
        return !(value->value);
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

static void _lcc_psym_dtor(lcc_map_t *self, void *value, void *data)
{
    _lcc_sym_t *sym = value;
    _lcc_sym_free(sym);
}

static void _lcc_file_dtor(lcc_array_t *self, void *item, void *data)
{
    lcc_file_t *fp = item;
    _lcc_file_free(fp);
}

void lcc_lexer_free(lcc_lexer_t *self)
{
    /* clear old file name if any */
    if (self->fname)
        lcc_string_unref(self->fname);

    /* release all cached tokens */
    while (self->tokens.next != &(self->tokens))
        lcc_token_free(self->tokens.next);

    /* clear feature tables */
    lcc_set_free(&(self->builtins));
    lcc_set_free(&(self->features));
    lcc_set_free(&(self->extensions));

    /* clear directive related tables */
    lcc_map_free(&(self->psyms));
    lcc_string_array_free(&(self->sccs_msgs));

    /* clear complex state buffers */
    lcc_array_free(&(self->files));
    lcc_array_free(&(self->eval_stack));

    /* clear other tables */
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
        .flags = LCC_FF_SYS,
        .lines = LCC_STRING_ARRAY_STATIC_INIT,
        .offset = 1,
        .display = lcc_string_from("<define>"),
    };

    /* complex structures */
    lcc_map_init(&(self->psyms), sizeof(_lcc_sym_t), _lcc_psym_dtor, NULL);
    lcc_array_init(&(self->files), sizeof(lcc_file_t), _lcc_file_dtor, NULL);
    lcc_array_init(&(self->eval_stack), sizeof(_lcc_val_t), NULL, NULL);

    /* feature sets */
    lcc_set_init(&(self->builtins));
    lcc_set_init(&(self->features));
    lcc_set_init(&(self->extensions));

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
    lcc_lexer_define(self, "__LCC__", "1");
    lcc_lexer_define(self, "__VERSION__", "\"LightCC 1.0 (GCC 4.8.3 compatiable)\"");

    /* LightCC emulates GCC 4.8.3 */
    lcc_lexer_define(self, "__GNUC__", "4");
    lcc_lexer_define(self, "__GNUC_MINOR__", "8");
    lcc_lexer_define(self, "__GNUC_PATCHLEVEL__", "3");

    /* standard defines */
    lcc_lexer_define(self, "__STDC__", "1");
    lcc_lexer_define(self, "__STDC_HOSTED__", "1");
    lcc_lexer_define(self, "__STDC_VERSION__", "199901L");

    /* data model */
    lcc_lexer_define(self, "_LP64", "1");
    lcc_lexer_define(self, "__LP64__", "1");

    /* platform specific symbols */
    lcc_lexer_define(self, "__unix__", "1");
    lcc_lexer_define(self, "__amd64__", "1");
    lcc_lexer_define(self, "__x86_64__", "1");

    /* assembler specified macros (LightCC uses GAS syntax) */
    lcc_lexer_define(self, "__REGISTER_PREFIX__", "%");
    lcc_lexer_define(self, "__USER_LABEL_PREFIX__", "_");

#define _NAME_DEF(type)     #type
#define _COPY_NAME(type)    lcc_lexer_define(self, #type, _NAME_DEF(type))

#include "lcc_builtin_sizes.i"
#include "lcc_builtin_types.i"
#include "lcc_builtin_limits.i"
#include "lcc_builtin_endians.i"

#undef _NAME_DEF
#undef _COPY_NAME

    /* built-in object-like extensions */
    _LCC_ADD_MACRO_EXT_O(__FILE__);
    _LCC_ADD_MACRO_EXT_O(__LINE__);
    _LCC_ADD_MACRO_EXT_O(__DATE__);
    _LCC_ADD_MACRO_EXT_O(__TIME__);
    _LCC_ADD_MACRO_EXT_O(__TIMESTAMP__);
    _LCC_ADD_MACRO_EXT_O(__BASE_FILE__);
    _LCC_ADD_MACRO_EXT_O(__INCLUDE_LEVEL__);

    /* built-in function-like extensions */
    _LCC_ADD_MACRO_EXT_F(defined);
    _LCC_ADD_MACRO_EXT_F(__has_include);
    _LCC_ADD_MACRO_EXT_F(__has_include_next);

    /* built-in feature testing macros */
    _LCC_ADD_MACRO_EXT_F(__has_builtin);
    _LCC_ADD_MACRO_EXT_F(__has_feature);
    _LCC_ADD_MACRO_EXT_F(__has_extension);

    /* pseudo-macro "__func__" and "__FUNCTION__" */
    _LCC_ADD_MACRO_EXT_O(__func__);
    _LCC_ADD_MACRO_EXT_O(__FUNCTION__);
    return 1;
}

lcc_token_t *lcc_lexer_next(lcc_lexer_t *self)
{
    /* advance lexer if no tokens remaining */
    if (self->tokens.next == &(self->tokens))
        if (!(lcc_lexer_advance(self)))
            return NULL;

    /* still no more tokens */
    if (self->tokens.next == &(self->tokens))
        return NULL;

    /* shift one token from lexer token list */
    lcc_token_t *next = self->tokens.next;
    lcc_token_t *token = lcc_token_detach(next);

    /* keyword conversion */
    if (token->type == LCC_TK_IDENT)
    {
        /* search in keyword mapping table */
        for (ssize_t i = 0; KEYWORDS[i].name; i++)
        {
            /* found the keyword, upgrade to keyword token */
            if (!(strcmp(KEYWORDS[i].name, token->ident->buf)))
            {
                lcc_string_unref(token->ident);
                token->type = LCC_TK_KEYWORD;
                token->keyword = KEYWORDS[i].keyword;
                break;
            }
        }
    }

    /* token maybe converted */
    return token;
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
                self->row = file->row + file->offset;
                self->fname = lcc_string_ref(file->display);

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
                    /* check for unterminated conditional directives */
                    if (self->eval_stack.count)
                    {
                        _lcc_lexer_error(self, "Unterminated conditional directive");
                        break;
                    }

                    /* move to end of source */
                    self->state = LCC_LX_STATE_END;
                    self->substate = LCC_LX_SUBSTATE_NULL;
                    break;
                }

                /* set EOF flags to flush the preprocessor */
                if (!(_lcc_check_drop_char(self)))
                {
                    self->flags |= LCC_LXF_EOF;
                    _lcc_handle_substate(self);
                }
                else
                {
                    self->flags |= LCC_LXF_EOF;
                    _lcc_handle_condition(self);
                }

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
                /* in condition compilation section,
                 * whereas this section is not compiled */
                if (!(_lcc_check_drop_char(self)))
                {
                    self->flags |= LCC_LXF_EOL;
                    _lcc_handle_substate(self);
                }
                else
                {
                    self->flags |= LCC_LXF_EOL;
                    _lcc_handle_condition(self);
                }

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
                /* in condition compilation section,
                 * whereas this section is not compiled */
                if (_lcc_check_drop_char(self))
                {
                    self->flags &= ~LCC_LXF_EOF;
                    self->flags &= ~LCC_LXF_EOL;
                    _lcc_handle_condition(self);
                }
                else
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
                }

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
                self->file->flags &= ~LCC_FF_LNODIR;

                /* rejected, don't commit */
                if (self->state == LCC_LX_STATE_REJECT)
                    break;

                /* commit the directive */
                _lcc_commit_directive(self);
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
                _lcc_sym_t *sym;
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
                }
                else
                {
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
                }

                /* reset substitution status */
                self->flags &= ~LCC_LXF_SUBST;
                self->subst_level = 0;

                /* perform substitution */
                if (!(_lcc_macro_subst(self, self->tokens.next, &(self->tokens))) ||
                    (self->tokens.next == &(self->tokens)))
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

void lcc_lexer_undef(lcc_lexer_t *self, const char *name)
{
    /* must be in initial state */
    if (self->state != LCC_LX_STATE_INIT)
    {
        fprintf(stderr, "*** FATAL: cannot remove symbols in the middle of parsing");
        abort();
    }

    /* use "#undef" to remove the symbol */
    lcc_string_array_append(
        &(self->file->lines),
        lcc_string_from_format("#undef %s", name)
    );
}

void lcc_lexer_define(lcc_lexer_t *self, const char *name, const char *value)
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

void lcc_lexer_add_builtin(lcc_lexer_t *self, const char *name) { lcc_set_add_string(&(self->builtins), name); }
void lcc_lexer_add_feature(lcc_lexer_t *self, const char *name) { lcc_set_add_string(&(self->features), name); }
void lcc_lexer_add_extension(lcc_lexer_t *self, const char *name) { lcc_set_add_string(&(self->extensions), name); }

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