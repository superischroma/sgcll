#include <stdio.h>
#include <stdlib.h>

#include "sgcllc.h"

void write_options(options_t* options, FILE* out)
{
    writei32(options->import_search_paths->size);
    for (int i = 0; i < options->import_search_paths->size; i++)
        writestr(vector_get(options->import_search_paths, i));
    writestr(options->fdlibm_path);
}

options_t* read_options(FILE* in)
{
    options_t* options = calloc(1, sizeof(options_t));
    if (!in || feof(in) || ferror(in))
    {
        options->import_search_paths = vector_qinit(1, "libsgcll");
        options->fdlibm_path = "fdlibm/libm.a";
        return options;
    }
    int sp_count = readi32;
    vector_t* import_search_paths = vector_init(sp_count, DEFAULT_ALLOC_DELTA);
    for (int i = 0; i < sp_count; i++)
        vector_push(import_search_paths, readstr);
    options->import_search_paths = import_search_paths;
    options->fdlibm_path = readstr;
    return options;
}