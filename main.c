#include <stdio.h>

#include "lcc_lexer.h"
#include "lcc_string_array.h"

int main()
{
    lcc_lexer_t lexer;
    lcc_token_t *token;
    lcc_lexer_init(&lexer, lcc_file_open("/Users/Oxygen/Sources/tests/lcc_test.c"));
    lcc_lexer_add_include_path(&lexer, "/Users/Oxygen/Sources/tests");
    lcc_lexer_add_include_path(&lexer, "/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/usr/include");
    lcc_lexer_add_include_path(&lexer, "/usr/local/include");

    while ((token = lcc_lexer_next(&lexer)))
    {
        lcc_string_t *s = lcc_token_str(token);
        printf("%s ", s->buf);
        lcc_string_unref(s);
        lcc_token_free(token);
        if (token->type == LCC_TK_OPERATOR &&
            (token->operator == LCC_OP_SEMICOLON ||
             token->operator == LCC_OP_LBLOCK))
            printf("\n");
    }
    printf("\n{END}\n");

//    printf("\nPSYMS:\n");
//    typedef struct __sym
//    {
//        long flags;
//        uint64_t *nx;
//        lcc_token_t *body;
//        lcc_string_t *name;
//        lcc_string_t *vaname;
//        lcc_string_array_t args;
//    } sym;
//    for (size_t i = 0; i < lexer.psyms.capacity; i++)
//    {
//        if (lexer.psyms.bucket[i].flags == LCC_MAP_FLAGS_USED)
//        {
//            printf("#define %s ", lexer.psyms.bucket[i].key->buf);
//            sym *s = lexer.psyms.bucket[i].value;
//            if (!(s->body))
//                printf("<built-in>");
//            else
//            {
//                for (lcc_token_t *p = s->body->next; p != s->body; p = p->next)
//                {
//                    lcc_string_t *x = lcc_token_str(p);
//                    printf("%s ", x->buf);
//                    lcc_string_unref(x);
//                }
//            }
//            printf("\n");
//        }
//    }

    lcc_lexer_free(&lexer);
    return 0;
}