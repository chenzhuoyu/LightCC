#include <stdio.h>

#include "lcc_lexer.h"
#include "lcc_string_array.h"

int main()
{
    lcc_lexer_t lexer;
    lcc_token_t *token;
    lcc_lexer_init(&lexer, lcc_file_open("/Users/Oxygen/Sources/tests/lcc_test.c"));
    lcc_lexer_set_gnu_ext(&lexer, LCC_LX_GNUX_VA_OPT_MACRO, 1);
    lcc_lexer_add_include_path(&lexer, "/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/usr/include");
    lcc_lexer_add_include_path(&lexer, "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/lib/clang/10.0.0/include");
    lcc_lexer_add_include_path(&lexer, "/usr/local/include");
    lcc_lexer_add_include_path(&lexer, "/Users/Oxygen/Sources/tests");
    lcc_lexer_add_include_path(&lexer, "/Users/Oxygen/ClionProjects/LightCC/include");

    int c = 0;
    int n = 0;
    int f = 0;
    while ((token = lcc_lexer_next(&lexer)))
    {
        if (token->type == LCC_TK_PRAGMA)
        {
            if (f)
            {
                f = 0;
                n -= 4;
                printf("%*s} ", n, "");
            }

            if (c)
                printf("\n");

            c = 0;
            n = 0;
            lcc_string_t *s = lcc_token_str(token);
            printf("%s\n", s->buf);
            lcc_string_unref(s);
        }
        else if (token->type == LCC_TK_OPERATOR &&
                 token->operator == LCC_OP_LBLOCK)
        {
            if (f)
            {
                f = 0;
                n -= 4;
                printf("%*s}\n", n, "");
            }

            printf("%*s{\n", !c * n, "");
            c = 0;
            n += 4;
        }
        else if (token->type == LCC_TK_OPERATOR &&
                 token->operator == LCC_OP_RBLOCK)
        {
            if (f)
            {
                c = 0;
                n -= 4;
                printf("%*s}\n", n, "");
            }

            f = 1;
        }
        else if (token->type == LCC_TK_OPERATOR &&
                 token->operator == LCC_OP_SEMICOLON)
        {
            if (f)
            {
                f = 0;
                n -= 4;
                printf("%*s} ", n, "");
            }

            c = 0;
            printf(";\n");
        }
        else
        {
            if (f)
            {
                f = 0;
                c = 0;
                n -= 4;
                printf("%*s}\n", n, "");
            }

            lcc_string_t *s = lcc_token_str(token);
            if (!(c++))
                printf("%*s", n, "");
            printf("%s ", s->buf);
            lcc_string_unref(s);
        }

        lcc_token_free(token);
    }

    /* what's wrong with Clion DFA ?? */
    #pragma clang diagnostic push
    #pragma ide diagnostic ignored "OCDFAInspection"

    if (f)
        printf("}\n");

    #pragma clang diagnostic pop

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