#ifndef LCC_LEXER_H
#define LCC_LEXER_H

#include "lcc_string.h"

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
    LCC_OP_RBRACKET,        /* Right-bracket    (   */
    LCC_OP_LINDEX,          /* Left-index       [   */
    LCC_OP_RINDEX,          /* Right-index      ]   */
    LCC_OP_LBLOCK,          /* Left-block       {   */
    LCC_OP_RBLOCK,          /* Right-block      }   */
    LCC_OP_COLON,           /* Colon            :   */
    LCC_OP_COMMA,           /* Comma            ,   */
    LCC_OP_POINT,           /* Point            .   */
    LCC_OP_SEMICOLON,       /* Semicolon        ;   */
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
    lcc_token_type_t type;

    union
    {
        lcc_string_t   *ident;
        lcc_literal_t   literal;
        lcc_keyword_t   keyword;
        lcc_operator_t  operator;
    };
} lcc_token_t;

#endif /* LCC_LEXER_H */
