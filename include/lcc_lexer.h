#ifndef LCC_LEXER_H
#define LCC_LEXER_H

#include <stdio.h>
#include <stdint.h>

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
    LCC_KW_COMPLEX,         /* _Complex     */
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
    LCC_OP_ELLIPSIS,        /* Ellipsis         ... */
    LCC_OP_STRINGIZE,       /* Stringize (PP)   #   */
    LCC_OP_CONCAT,          /* Concat (PP)      ##  */
} lcc_operator_t;

typedef enum _lcc_literal_type_t
{
    LCC_LT_INT,             /* int                  */
    LCC_LT_LONG,            /* long                 */
    LCC_LT_LONGLONG,        /* long long            */
    LCC_LT_UINT,            /* unsigned int         */
    LCC_LT_ULONG,           /* unsigned long        */
    LCC_LT_ULONGLONG,       /* unsigned long long   */
    LCC_LT_FLOAT,           /* float                */
    LCC_LT_DOUBLE,          /* double               */
    LCC_LT_LONGDOUBLE,      /* long double          */
    LCC_LT_CHAR,            /* '...'                */
    LCC_LT_STRING,          /* "..."                */
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
    lcc_string_t *raw;
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

    char                 ref;
    lcc_string_t        *src;
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
void lcc_token_clear(lcc_token_t *self);
void lcc_token_attach(lcc_token_t *self, lcc_token_t *next);

lcc_token_t *lcc_token_new(void);
lcc_token_t *lcc_token_copy(lcc_token_t *self);
lcc_token_t *lcc_token_detach(lcc_token_t *self);

lcc_token_t *lcc_token_from_ident(lcc_string_t *src, lcc_string_t *ident);
lcc_token_t *lcc_token_from_keyword(lcc_string_t *src, lcc_keyword_t keyword);
lcc_token_t *lcc_token_from_operator(lcc_string_t *src, lcc_operator_t operator);

lcc_token_t *lcc_token_from_raw(lcc_string_t *src, lcc_string_t *value);
lcc_token_t *lcc_token_from_char(lcc_string_t *src, lcc_string_t *value, char allow_gnuext);
lcc_token_t *lcc_token_from_string(lcc_string_t *src, lcc_string_t *value, char allow_gnuext);
lcc_token_t *lcc_token_from_number(lcc_string_t *src, lcc_string_t *value, lcc_literal_type_t type);

const char *lcc_token_kw_name(lcc_keyword_t value);
const char *lcc_token_op_name(lcc_operator_t value);

lcc_string_t *lcc_token_as_string(lcc_token_t *self);
lcc_string_t *lcc_token_to_string(lcc_token_t *self);

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
    LCC_LX_STATE_END,
} lcc_lexer_state_t;

typedef enum _lcc_lexer_substate_t
{
    LCC_LX_SUBSTATE_NULL,
    LCC_LX_SUBSTATE_NAME,
    LCC_LX_SUBSTATE_STRING,
    LCC_LX_SUBSTATE_STRING_ESCAPE,
    LCC_LX_SUBSTATE_STRING_ESCAPE_HEX,
    LCC_LX_SUBSTATE_STRING_ESCAPE_HEX_D,
    LCC_LX_SUBSTATE_STRING_ESCAPE_HEX_E,
    LCC_LX_SUBSTATE_STRING_ESCAPE_OCT_2,
    LCC_LX_SUBSTATE_STRING_ESCAPE_OCT_3,
    LCC_LX_SUBSTATE_NUMBER,
    LCC_LX_SUBSTATE_NUMBER_ZERO,
    LCC_LX_SUBSTATE_NUMBER_OR_OP,
    LCC_LX_SUBSTATE_NUMBER_INTEGER_HEX,
    LCC_LX_SUBSTATE_NUMBER_INTEGER_HEX_D,
    LCC_LX_SUBSTATE_NUMBER_INTEGER_OCT,
    LCC_LX_SUBSTATE_NUMBER_INTEGER_U,
    LCC_LX_SUBSTATE_NUMBER_INTEGER_L,
    LCC_LX_SUBSTATE_NUMBER_INTEGER_UL,
    LCC_LX_SUBSTATE_NUMBER_DECIMAL,
    LCC_LX_SUBSTATE_NUMBER_DECIMAL_SCI,
    LCC_LX_SUBSTATE_NUMBER_DECIMAL_SCI_EXP,
    LCC_LX_SUBSTATE_NUMBER_DECIMAL_SCI_EXP_SIGN,
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
    LCC_LX_SUBSTATE_OPERATOR_HASH,
    LCC_LX_SUBSTATE_OPERATOR_ELLIPSIS,
    LCC_LX_SUBSTATE_INCLUDE_FILE,
    LCC_LX_SUBSTATE_COMMENT_LINE,
    LCC_LX_SUBSTATE_COMMENT_BLOCK,
    LCC_LX_SUBSTATE_COMMENT_BLOCK_END,
} lcc_lexer_substate_t;

typedef enum _lcc_lexer_define_state_t
{
    LCC_LX_DEFSTATE_INIT,
    LCC_LX_DEFSTATE_PUSH_ARG,
    LCC_LX_DEFSTATE_DELIM_OR_END,
    LCC_LX_DEFSTATE_END,
} lcc_lexer_define_state_t;

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

#define LCC_LXF_EOL             0x0000000000000001      /* End-Of-Line encountered */
#define LCC_LXF_EOF             0x0000000000000002      /* End-Of-File encountered */
#define LCC_LXF_EOS             0x0000000000000004      /* End-Of-Source encountered */
#define LCC_LXF_DIRECTIVE       0x0000000000000008      /* parsing compiler directive */
#define LCC_LXF_CHAR_SEQ        0x0000000000000010      /* parsing character sequence rather than string */
#define LCC_LXF_SUBST           0x0000000000000020      /* substituting macros */
#define LCC_LXF_MASK            0x00000000000000ff      /* lexer flags mask */

#define LCC_LXDN_NULL           0x0000000000000100      /* # directive (null directive, does nothing) */
#define LCC_LXDN_INCLUDE        0x0000000000000200      /* #include directive */
#define LCC_LXDN_DEFINE         0x0000000000000400      /* #define directive */
#define LCC_LXDN_UNDEF          0x0000000000000800      /* #undef directive */
#define LCC_LXDN_IF             0x0000000000001000      /* #if directive */
#define LCC_LXDN_IFDEF          0x0000000000002000      /* #ifdef directive */
#define LCC_LXDN_IFNDEF         0x0000000000004000      /* #ifndef directive */
#define LCC_LXDN_ELIF           0x0000000000008000      /* #else directive */
#define LCC_LXDN_ELSE           0x0000000000010000      /* #else directive */
#define LCC_LXDN_ENDIF          0x0000000000020000      /* #endif directive */
#define LCC_LXDN_PRAGMA         0x0000000000040000      /* #pragma directive */
#define LCC_LXDN_ERROR          0x0000000000080000      /* #error directive */
#define LCC_LXDN_WARNING        0x0000000000100000      /* #warning directive */
#define LCC_LXDN_LINE           0x0000000000200000      /* #line directive */
#define LCC_LXDN_SCCS           0x0000000000400000      /* #sccs directive */
#define LCC_LXDN_MASK           0x0000000000ffff00      /* compiler directive name mask */

#define LCC_LXDF_DEFINE_O       0x0000000001000000      /* object-like macro (weak flag) */
#define LCC_LXDF_DEFINE_F       0x0000000002000000      /* function-like macro */
#define LCC_LXDF_DEFINE_NS      0x0000000004000000      /* macro name already set */
#define LCC_LXDF_DEFINE_VAR     0x0000000008000000      /* variadic function-like macro */
#define LCC_LXDF_DEFINE_NVAR    0x0000000010000000      /* named variadic arguments */
#define LCC_LXDF_DEFINE_FINE    0x0000000020000000      /* macro is been checked */
#define LCC_LXDF_DEFINE_USING   0x0000000040000000      /* macro is been using */
#define LCC_LXDF_DEFINE_MASK    0x00000000ff000000      /* #define directive flags mask */

#define LCC_LXDF_INCLUDE_SYS    0x0000000100000000      /* #include includes file from system headers */
#define LCC_LXDF_INCLUDE_NEXT   0x0000000200000000      /* #include_next directive */

typedef struct _lcc_psym_t
{
    long flags;
    uint64_t *nx;
    lcc_token_t *body;
    lcc_string_t *name;
    lcc_string_t *vaname;
    lcc_string_array_t args;
} lcc_psym_t;

void lcc_psym_free(lcc_psym_t *self);
void lcc_psym_init(
    lcc_psym_t *self,
    lcc_string_t *name,
    lcc_string_array_t *args,
    long flags,
    lcc_string_t *vaname
);

typedef struct _lcc_lexer_t
{
    /* lexer tables */
    int gnuext;
    lcc_map_t psyms;
    lcc_array_t files;
    lcc_string_array_t sccs_msgs;
    lcc_string_array_t include_paths;
    lcc_string_array_t library_paths;

    /* state machine */
    long flags;
    lcc_lexer_state_t state;
    lcc_lexer_substate_t substate;
    lcc_lexer_define_state_t defstate;

    /* complex state buffers */
    size_t subst_level;
    lcc_array_t eval_stack;
    lcc_string_t *macro_name;
    lcc_string_t *macro_vaname;
    lcc_string_array_t macro_args;

    /* current file info */
    size_t col;
    size_t row;
    lcc_string_t *fname;
    lcc_string_t *source;

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
    LCC_LX_GNUX_DOLLAR_IDENT    = 0x00000001,       /* dollar sign character '$' in identifiers */
    LCC_LX_GNUX_ESCAPE_CHAR     = 0x00000002,       /* '\e' escape character */
} lcc_lexer_gnu_ext_t;

void lcc_lexer_free(lcc_lexer_t *self);
char lcc_lexer_init(lcc_lexer_t *self, lcc_file_t file);

lcc_token_t *lcc_lexer_next(lcc_lexer_t *self);
lcc_token_t *lcc_lexer_advance(lcc_lexer_t *self);

void lcc_lexer_add_define(lcc_lexer_t *self, const char *name, const char *value);
void lcc_lexer_add_include_path(lcc_lexer_t *self, const char *path);
void lcc_lexer_add_library_path(lcc_lexer_t *self, const char *path);

void lcc_lexer_set_gnu_ext(lcc_lexer_t *self, lcc_lexer_gnu_ext_t name, char enabled);
void lcc_lexer_set_error_handler(lcc_lexer_t *self, lcc_lexer_on_error_fn error_fn, void *data);

#endif /* LCC_LEXER_H */
