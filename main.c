#include <stdio.h>

#include "lcc_lexer.h"
#include "lcc_string_array.h"

int main()
{
    lcc_lexer_t lexer;
    lcc_lexer_init(&lexer, lcc_file_open("/Users/Oxygen/Sources/tests/lcc_test.c"));

    while (!(lexer.flags & LCC_LXF_EOS))
    {
        printf("-------------------\n");
        if (!(lcc_lexer_next(&lexer, NULL))) break;
    }

    lcc_lexer_free(&lexer);
    return 0;
}