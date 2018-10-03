#ifndef LCC_LEXER_H
#define LCC_LEXER_H

#include <stdio.h>

#include "lcc_map.h"
#include "lcc_array.h"
#include "lcc_string.h"
#include "lcc_string_array.h"

/*** Tokens ***/

typedef enum _lcc_keyword_t
{
    LCC_KW_AUTO,            /* auto         */
    LCC_KW_BOOL,            /* _Bool        */
    LCC_KW_BREAK,           /* break        */
    LCC_KW_CASE,            /* case         */
    LCC_KW_CHAR,            /* char         */
    LCC_KW_COMPLEX,         /* Complex      */
    LCC_KW_CONST,           /* const        */
    LCC_KW_CONTINUE,        /* continue     */
    LCC_KW_DEFAULT,         /* default      */
    LCC_KW_DO,              /* do           */
    LCC_KW_DOUBLE,          /* double       */
    LCC_KW_ELSE,            /* else         */
    LCC_KW_ENUM,            /* enum         */
    LCC_KW_EXTERN,          /* extern       */
    LCC_KW_FLOAT,           /* float        */
    LCC_KW_FOR,             /* for          */
    LCC_KW_GOTO,            /* goto         */
    LCC_KW_IF,              /* if           */
    LCC_KW_IMAGINARY,       /* _Imaginary   */
    LCC_KW_INLINE,          /* inline       */
    LCC_KW_INT,             /* int          */
    LCC_KW_LONG,            /* long         */
    LCC_KW_REGISTER,        /* register     */
    LCC_KW_RESTRICT,        /* restrict     */
    LCC_KW_RETURN,          /* return       */
    LCC_KW_SHORT,           /* short        */
    LCC_KW_SIGNED,          /* signed       */
    LCC_KW_SIZEOF,          /* sizeof       */
    LCC_KW_STATIC,          /* static       */
    LCC_KW_STRUCT,          /* struct       */
    LCC_KW_SWITCH,          /* switch       */
    LCC_KW_TYPEDEF,         /* typedef      */
    LCC_KW_UNION,           /* union        */
    LCC_KW_UNSIGNED,        /* unsigned     */
    LCC_KW_VOID,            /* void         */
    LCC_KW_VOLATILE,        /* volatile     */
    LCC_KW_WHILE,           /* while        */
} lcc_keyword_t;

typedef enum _lcc_operator_t
{
    LCC_OP_PLUS,            /* Plus             +   */
    LCC_OP_MINUS,           /* Munis            -   */
    LCC_OP_STAR,            /* Star             *   */
    LCC_OP_SLASH,           /* Slash            /   */
    LCC_OP_PERCENT,         /* Percent          %   */
    LCC_OP_INCR,            /* Increment        ++  */
    LCC_OP_DECR,            /* Decrement        --  */
    LCC_OP_EQ,              /* Equals           ==  */
    LCC_OP_GT,              /* Greater-than     >   */
    LCC_OP_LT,              /* Less-than        <   */
    LCC_OP_NEQ,             /* Not-equals       !=  */
    LCC_OP_GEQ,             /* GT-or-equals     >=  */
    LCC_OP_LEQ,             /* LT-or-equals     <=  */
    LCC_OP_LAND,            /* Logical-AND      &&  */
    LCC_OP_LOR,             /* Logical-OR       ||  */
    LCC_OP_LNOT,            /* Logical-NOT      !   */
    LCC_OP_BAND,            /* Bitwise-AND      &   */
    LCC_OP_BOR,             /* Bitwise-OR       |   */
    LCC_OP_BXOR,            /* Bitwise-XOR      ^   */
    LCC_OP_BINV,            /* Inversion        ~   */
    LCC_OP_BSHL,            /* Left-shifting    <<  */
    LCC_OP_BSHR,            /* Right-shiftint   >>  */
    LCC_OP_ASSIGN,          /* Assignment       =   */
    LCC_OP_IADD,            /* Inplace-ADD      +=  */
    LCC_OP_ISUB,            /* Inplace-SUB      -=  */
    LCC_OP_IMUL,            /* Inplace-MUL      *=  */
    LCC_OP_IDIV,            /* Inplace-DIV      /=  */
    LCC_OP_IMOD,            /* Inplace-MOD      %=  */
    LCC_OP_ISHL,            /* Inplace-BSHL     <<= */
    LCC_OP_ISHR,            /* Inplace-BSHR     >>= */
    LCC_OP_IAND,            /* Inplace-BAND     &=  */
    LCC_OP_IXOR,            /* Inplace-BXOR     ^=  */
    LCC_OP_IOR,             /* Inplace-BOR      |=  */
    LCC_OP_QUESTION,        /* Question-mark    ?   */
    LCC_OP_LBRACKET,        /* Left-bracket     (   */
    LCC_OP_RBRACKET,        /* Right-bracket    )   */
    LCC_OP_LINDEX,          /* Left-index       [   */
    LCC_OP_RINDEX,          /* Right-index      ]   */
    LCC_OP_LBLOCK,          /* Left-block       {   */
    LCC_OP_RBLOCK,          /* Right-block      }   */
    LCC_OP_COLON,           /* Colon            :   */
    LCC_OP_COMMA,           /* Comma            ,   */
    LCC_OP_POINT,           /* Point            .   */
    LCC_OP_SEMICOLON,       /* Semicolon        ;   */
    LCC_OP_DEREF,           /* Dereferencing    ->  */
} lcc_operator_t;

typedef enum _lcc_literal_type_t
{
    LCC_LT_INT,
    LCC_LT_LONG,
    LCC_LT_LONGLONG,
    LCC_LT_UINT,
    LCC_LT_ULONG,
    LCC_LT_ULONGLONG,
    LCC_LT_FLOAT,
    LCC_LT_DOUBLE,
    LCC_LT_LONGDOUBLE,
    LCC_LT_CHAR,
    LCC_LT_STRING,
} lcc_literal_type_t;

typedef enum _lcc_token_type_t
{
    LCC_TK_EOF,
    LCC_TK_IDENT,
    LCC_TK_LITERAL,
    LCC_TK_KEYWORD,
    LCC_TK_OPERATOR,
} lcc_token_type_t;

typedef struct _lcc_literal_t
{
    lcc_literal_type_t type;

    union
    {
        int                 v_int;
        long                v_long;
        long long           v_longlong;
        unsigned int        v_uint;
        unsigned long       v_ulong;
        unsigned long long  v_ulonglong;
        float               v_float;
        double              v_double;
        long double         v_longdouble;
        lcc_string_t       *v_char;
        lcc_string_t       *v_string;
    };
} lcc_literal_t;

typedef struct _lcc_token_t
{
    struct _lcc_token_t *prev;
    struct _lcc_token_t *next;
    lcc_token_type_t     type;

    union
    {
        lcc_string_t   *ident;
        lcc_literal_t   literal;
        lcc_keyword_t   keyword;
        lcc_operator_t  operator;
    };
} lcc_token_t;

void lcc_token_free(lcc_token_t *self);
void lcc_token_init(lcc_token_t *self);
void lcc_token_attach(lcc_token_t *self, lcc_token_t *next);

lcc_token_t *lcc_token_from_eof          (void);
lcc_token_t *lcc_token_from_ident        (lcc_string_t       *ident);
lcc_token_t *lcc_token_from_keyword      (lcc_keyword_t       keyword);
lcc_token_t *lcc_token_from_operator     (lcc_operator_t      operator);

lcc_token_t *lcc_token_from_int          (int                 value);
lcc_token_t *lcc_token_from_long         (long                value);
lcc_token_t *lcc_token_from_longlong     (long long           value);
lcc_token_t *lcc_token_from_uint         (unsigned int        value);
lcc_token_t *lcc_token_from_ulong        (unsigned long       value);
lcc_token_t *lcc_token_from_ulonglong    (unsigned long long  value);
lcc_token_t *lcc_token_from_float        (float               value);
lcc_token_t *lcc_token_from_double       (double              value);
lcc_token_t *lcc_token_from_longdouble   (long double         value);
lcc_token_t *lcc_token_from_char         (lcc_string_t       *value);
lcc_token_t *lcc_token_from_string       (lcc_string_t       *value);

/*** Source File ***/

#define LCC_FF_LNODIR           0x00000001      /* no compiler directive is allowed on this line */
#define LCC_FF_INVALID          0x80000000      /* this file object is invalid */

typedef struct _lcc_file_t
{
    int flags;
    size_t col;
    size_t row;
    lcc_string_t *name;
    lcc_string_array_t lines;
} lcc_file_t;

lcc_file_t lcc_file_open(const char *fname);
lcc_file_t lcc_file_from_file(const char *fname, FILE *fp);
lcc_file_t lcc_file_from_string(const char *fname, const char *data, size_t size);

/*** Token Buffer ***/

typedef struct _lcc_token_buffer_t
{
    char *buf;
    size_t len;
    size_t cap;
} lcc_token_buffer_t;

void lcc_token_buffer_free(lcc_token_buffer_t *self);
void lcc_token_buffer_init(lcc_token_buffer_t *self);
void lcc_token_buffer_reset(lcc_token_buffer_t *self);
void lcc_token_buffer_append(lcc_token_buffer_t *self, char ch);

/*** Lexer Object ***/

#define LCC_LEXER_MAX_LINE_LEN      4096

typedef enum _lcc_lexer_state_t
{
    LCC_LX_STATE_INIT,
    LCC_LX_STATE_SHIFT,
    LCC_LX_STATE_POP_FILE,
    LCC_LX_STATE_NEXT_LINE,
    LCC_LX_STATE_NEXT_LINE_CONT,
    LCC_LX_STATE_GOT_CHAR,
    LCC_LX_STATE_GOT_DIRECTIVE,
    LCC_LX_STATE_COMMIT,
    LCC_LX_STATE_ACCEPT,
    LCC_LX_STATE_ACCEPT_KEEP,
    LCC_LX_STATE_REJECT,
} lcc_lexer_state_t;

typedef enum _lcc_lexer_substate_t
{
    LCC_LX_SUBSTATE_NULL,
    LCC_LX_SUBSTATE_NAME,
    LCC_LX_SUBSTATE_CHARS,
    LCC_LX_SUBSTATE_NUMBER,
    LCC_LX_SUBSTATE_STRING,
    LCC_LX_SUBSTATE_OPERATOR_PLUS,
    LCC_LX_SUBSTATE_OPERATOR_MINUS,
    LCC_LX_SUBSTATE_OPERATOR_STAR,
    LCC_LX_SUBSTATE_OPERATOR_SLASH,
    LCC_LX_SUBSTATE_OPERATOR_PERCENT,
    LCC_LX_SUBSTATE_OPERATOR_EQU,
    LCC_LX_SUBSTATE_OPERATOR_GT,
    LCC_LX_SUBSTATE_OPERATOR_GT_GT,
    LCC_LX_SUBSTATE_OPERATOR_LT,
    LCC_LX_SUBSTATE_OPERATOR_LT_LT,
    LCC_LX_SUBSTATE_OPERATOR_EXCL,
    LCC_LX_SUBSTATE_OPERATOR_AMP,
    LCC_LX_SUBSTATE_OPERATOR_BAR,
    LCC_LX_SUBSTATE_OPERATOR_CARET,
} lcc_lexer_substate_t;

typedef enum _lcc_lexer_error_type_t
{
    LCC_LXET_ERROR,
    LCC_LXET_WARNING,
} lcc_lexer_error_type_t;

struct _lcc_lexer_t;
typedef char (*lcc_lexer_on_error_fn)(
    struct _lcc_lexer_t     *self,
    lcc_string_t            *file,
    ssize_t                  row,
    ssize_t                  col,
    lcc_string_t            *message,
    lcc_lexer_error_type_t   type,
    void                    *data
);

#define LCC_LXF_EOL         0x0000000000000001      /* End-Of-Line encountered */
#define LCC_LXF_EOF         0x0000000000000002      /* End-Of-File encountered */
#define LCC_LXF_END         0x0000000000000003      /* EOF or EOL encountered */

#define LCC_LXF_EOS         0x0000000000000004      /* End-Of-Source encountered */
#define LCC_LXF_DIRECTIVE   0x0000000000000008      /* parsing compiler directive */
#define LCC_LXF_MASK        0x00000000ffffffff      /* lexer flags mask */

#define LCC_LXDN_INCLUDE    0x0000000100000000      /* #include directive */
#define LCC_LXDN_DEFINE     0x0000000200000000      /* #define directive */
#define LCC_LXDN_UNDEF      0x0000000800000000      /* #undef directive */
#define LCC_LXDN_IF         0x0000001000000000      /* #if directive */
#define LCC_LXDN_IFDEF      0x0000002000000000      /* #ifdef directive */
#define LCC_LXDN_IFNDEF     0x0000004000000000      /* #ifndef directive */
#define LCC_LXDN_ELSE       0x0000008000000000      /* #else directive */
#define LCC_LXDN_ENDIF      0x0000010000000000      /* #endif directive */
#define LCC_LXDN_PRAGMA     0x0000020000000000      /* #pragma directive */
#define LCC_LXDN_MASK       0x00000fff00000000      /* compiler directive name mask */

typedef struct _lcc_lexer_t
{
    /* lexer tables */
    int gnuext;
    lcc_map_t psyms;
    lcc_array_t files;
    lcc_string_array_t include_paths;
    lcc_string_array_t library_paths;

    /* state machine */
    long flags;
    lcc_lexer_state_t state;
    lcc_lexer_substate_t substate;

    /* current file and token */
    char ch;
    lcc_file_t *file;
    lcc_token_t tokens;
    lcc_token_buffer_t token_buffer;

    /* error handling */
    void *error_data;
    lcc_lexer_on_error_fn error_fn;
} lcc_lexer_t;

typedef enum _lcc_lexer_gnu_ext_t
{
    LCC_LX_GNUX_DOLLAR_IDENT = 0x00000001,      /* allow the dollar sign character '$' in identifiers */
} lcc_lexer_gnu_ext_t;

void lcc_lexer_free(lcc_lexer_t *self);
char lcc_lexer_init(lcc_lexer_t *self, lcc_file_t file);
char lcc_lexer_next(lcc_lexer_t *self);

void lcc_lexer_add_define(lcc_lexer_t *self, const char *name, const char *value);
void lcc_lexer_add_include_path(lcc_lexer_t *self, const char *path);
void lcc_lexer_add_library_path(lcc_lexer_t *self, const char *path);

void lcc_lexer_set_gnu_ext(lcc_lexer_t *self, lcc_lexer_gnu_ext_t name, char enabled);
void lcc_lexer_set_error_handler(lcc_lexer_t *self, lcc_lexer_on_error_fn error_fn, void *data);

#endif /* LCC_LEXER_H */
