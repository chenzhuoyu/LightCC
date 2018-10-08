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

    while ((token = lcc_lexer_next(&lexer)) && (token->type != LCC_TK_EOF))
    {
        lcc_string_t *s = lcc_token_to_string(token);
        printf("%s\n", s->buf);
        lcc_string_unref(s);
        lcc_token_free(token);
    }

    printf("{END}\n");
    lcc_token_free(token);
    lcc_lexer_free(&lexer);
    return 0;
}