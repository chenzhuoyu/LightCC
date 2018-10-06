#include <stdio.h>

#include "lcc_lexer.h"
#include "lcc_string_array.h"

int main()
{
    lcc_lexer_t lexer;
    lcc_lexer_init(&lexer, lcc_file_open("/Users/Oxygen/Sources/tests/lcc_test.c"));
    lcc_lexer_add_include_path(&lexer, "/Users/Oxygen/Sources/tests");
    lcc_lexer_add_include_path(&lexer, "/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/usr/include");
    lcc_lexer_add_include_path(&lexer, "/usr/local/include");

    while (lexer.state != LCC_LX_STATE_END)
    {
        printf("-------------------\n");
        if (!(lcc_lexer_advance(&lexer))) break;
    }

    printf("{END}\n");
    lcc_lexer_free(&lexer);
    return 0;
}