#ifndef LCC_LEXER_H
#define LCC_LEXER_H

typedef enum _lcc_token_type_t {
    LCC_TK_IDENT,
    LCC_TK_PRAGMA,
    LCC_TK_LITERAL,
    LCC_TK_KEYWORD,
    LCC_TK_OPERATOR,
} lcc_token_type_t;

typedef struct _lcc_token_t {
    lcc_token_type_t type;
} lcc_token_t;

typedef struct _lcc_lexer_t {

} lcc_lexer_t;

#endif /* LCC_LEXER_H */
