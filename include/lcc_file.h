#ifndef LCC_FILE_H
#define LCC_FILE_H

#include <stdio.h>
#include <wchar.h>
#include <sys/types.h>

typedef struct _lcc_file_t {
    int ref;
    int col;
    int row;
    long pos;
    long size;
    char *name;
    wchar_t *data;
} lcc_file_t;

lcc_file_t *lcc_file_ref(lcc_file_t *self);
lcc_file_t *lcc_file_open(const char *fname);
lcc_file_t *lcc_file_open_string(const wchar_t *content, size_t size);

void lcc_file_unref(lcc_file_t *self);
void lcc_file_return(lcc_file_t *self, wchar_t ch);

wchar_t lcc_file_next(lcc_file_t *self);
wchar_t lcc_file_peek(lcc_file_t *self);

#endif /* LCC_FILE_H */
