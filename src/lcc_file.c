#include <stdlib.h>
#include <string.h>

#include "lcc_file.h"

lcc_file_t *lcc_file_ref(lcc_file_t *self)
{
    self->ref++;
    return self;
}

lcc_file_t *lcc_file_open(const char *fname)
{
    FILE *fp;
    lcc_file_t *self;

    /* open the file */
    if (!(fp = fopen(fname, "r")))
        return NULL;

    /* create the file object */
    if (!(self = malloc(sizeof(lcc_file_t))))
    {
        fprintf(stderr, "fatal: cannot allocate memory for files\n");
        abort();
    }

    /* build the file object */
    self->col = 1;
    self->row = 1;
    self->ref = 1;
    self->name = strdup(fname);
    return self;
}

lcc_file_t *lcc_file_open_string(const wchar_t *content, size_t size)
{
    FILE *fp;
    lcc_file_t *self;

    /* open the file */
    if (!(fp = fmemopen(content, size, "r")))
        return NULL;

    /* create the file object */
    if (!(self = malloc(sizeof(lcc_file_t))))
    {
        fprintf(stderr, "fatal: cannot allocate memory for files\n");
        abort();
    }

    /* build the file object */
    self->fp = fp;
    self->col = 1;
    self->row = 1;
    self->ref = 1;
    self->name = strdup("<memory>");
    return self;
}

void lcc_file_unref(lcc_file_t *self)
{
    if (--self->ref == 0)
    {
        free(self->data);
        free(self->name);
    }
}

void lcc_file_return(lcc_file_t *self, wchar_t ch)
{

}

wchar_t lcc_file_next(lcc_file_t *self)
{

}

wchar_t lcc_file_peek(lcc_file_t *self)
{

}
