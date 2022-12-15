#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sgcllc.h"

#define write(c) fputc(c, out)
#define writei16(c) fwrite(&c, sizeof(short), 1, out);
#define writestr(str) impl_writestr(str, out)

#define read impl_read(in)
#define readi16 impl_readi16(in)
#define readstr impl_readstr(in)

void impl_writestr(char* str, FILE* out)
{
    fwrite(str, sizeof(char), strlen(str), out);
    fputc('\0', out);
}

void writetype(FILE* out, ast_node_t* node)
{
    if (node->datatype->name)
        write(0x01);
    else
        write(0x00);
    if (node->datatype->name)
        writestr(node->datatype->name);
    else
        write(node->datatype->type);
    write(node->datatype->visibility);
    write(node->datatype->usign);
}

char impl_read(FILE* in)
{
    if (feof(in))
        errorc("corrupted header file: %i", __LINE__);
    char c;
    fread(&c, sizeof(char), 1, in);
    return c;
}

short impl_readi16(FILE* in)
{
    if (feof(in))
        errorc("corrupted header file: %i", __LINE__);
    short s;
    fread(&s, sizeof(short), 1, in);
    return s;
}

char* impl_readstr(FILE* in)
{
    if (feof(in))
        errorc("corrupted header file: %i", __LINE__);
    buffer_t* buffer = buffer_init(1024, 64);
    for (char c = fgetc(in); c; c = fgetc(in))
    {
        buffer_append(buffer, c);
        if (feof(in)) errorc("corrupted header file: %i", __LINE__);
    }
    buffer_append(buffer, '\0');
    return buffer_export(buffer);
}

// wb mode on fopen for this function
void write_header(FILE* out, map_t* genv)
{
    for (int i = 0; i < genv->capacity; i++)
    {
        char* key = genv->key[i];
        if (key == NULL)
            continue;
        ast_node_t* node = genv->value[i];
        if (node->type == AST_GVAR)
            write(0x00);
        else if (node->type == AST_FUNC_DEFINITION)
        {
            if (node->extrn)
                continue;
            write(0x01);
        }
        else
            errorc("expected global variable or function definition for header output, got %i", node->type);
        writetype(out, node);
        if (node->type == AST_GVAR)
            writestr(node->var_name);
        else
            writestr(node->func_name);
        if (node->type == AST_FUNC_DEFINITION)
        {
            writei16(node->params->size);
            for (int i = 0; i < node->params->size; i++)
            {
                ast_node_t* arg = vector_get(node->params, i);
                writetype(out, arg);
                writestr(arg->var_name);
            }
        }
    }
}

vector_t* read_header(FILE* in)
{
    #define make_datatype \
        datatype_t* dt = calloc(1, sizeof(datatype_t)); \
        dt->visibility = vis; \
        dt->usign = usign; \
        if (type_storage == 0x00) \
            dt->type = dtt; \
        else \
        { \
            errorc("objects don't exist, errored on: %i", type_storage); \
        } \
        switch (dt->type) \
        { \
            case DTT_VOID: dt->size = 0; break; \
            case DTT_I8: \
            case DTT_BOOL: \
                dt->size = 1; \
                break; \
            case DTT_I16: dt->size = 2; break; \
            case DTT_I32: \
            case DTT_F32: \
            case DTT_LET: \
                dt->size = 4; \
                break; \
            default: \
                dt->size = 8; \
                break; \
        }
    vector_t* vec = vector_init(5, 5);
    for (;;)
    {
        ast_node_type decl_type = fgetc(in);
        if (decl_type == EOF)
            break;
        char type_storage = read;
        char dtt, * nametype;
        if (type_storage == 0x00)
            dtt = read;
        else
            nametype = readstr;
        visibility_type vis = read;
        bool usign = read;
        char* name = readstr;
        if (decl_type == 0x00)
            errorc("global variables do not exist yet");
        else if (decl_type == 0x01)
        {
            make_datatype;
            ast_node_t* func_node = vector_push(vec, ast_builtin_init(dt, name, vector_init(5, 5)));
            short param_count = readi16;
            for (int i = 0; i < param_count; i++)
            {
                char type_storage = read;
                char dtt, * nametype;
                if (type_storage == 0x00)
                    dtt = read;
                else
                    nametype = readstr;
                visibility_type vis = read;
                bool usign = read;
                char* name = readstr;
                make_datatype;
                vector_push(func_node->params, ast_lvar_init(dt, NULL, name));
            }
        }
    }
    return vec;
}