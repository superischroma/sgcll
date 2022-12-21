#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sgcllc.h"

#define write(c) fputc(c, out)
#define writei16(c) fwrite(&c, sizeof(short), 1, out)
#define writei32(c) fwrite(&c, sizeof(int), 1, out)
#define writestr(str) impl_writestr(str, out)
#define writedt(dt) impl_writedt(dt, out)
#define writefunc(func) impl_writefunc(func, out)

#define read impl_read(in)
#define readi16 impl_readi16(in)
#define readi32 impl_readi32(in)
#define readstr impl_readstr(in)
#define readdt impl_readdt(in)
#define readfunc(bp) impl_readfunc(in, filename, ident, bp)

static void impl_writestr(char* str, FILE* out)
{
    fwrite(str, sizeof(char), strlen(str), out);
    fputc('\0', out);
}

static void impl_writedt(datatype_t* dt, FILE* out)
{
    write(dt->visibility);
    if (dt->type == DTT_ARRAY)
    {
        datatype_t* current = dt;
        while (dt->array_type)
            current = dt->array_type;
        write(current->type);
    }
    else
        write(dt->type);
    write(dt->size);
    write(dt->usign);
    if (dt->type == DTT_OBJECT && dt->name)
        writestr(dt->name);
    if (dt->type == DTT_ARRAY)
        write(dt->depth);
}

static void impl_writefunc(ast_node_t* func, FILE* out)
{
    writedt(func->datatype);
    write(func->func_type);
    write(func->lowlvl);
    writei16(func->operator);
    writei16(func->params->size);
    for (int i = 0; i < func->params->size; i++)
    {
        ast_node_t* param = vector_get(func->params, i);
        writestr(param->var_name);
        writedt(param->datatype);
    }
}

static char impl_read(FILE* in)
{
    if (feof(in))
        errorc("corrupted header file (line %i)", __LINE__);
    char c;
    fread(&c, sizeof(char), 1, in);
    return c;
}

static short impl_readi16(FILE* in)
{
    if (feof(in))
        errorc("corrupted header file (line %i)", __LINE__);
    short s;
    fread(&s, sizeof(short), 1, in);
    return s;
}

static int impl_readi32(FILE* in)
{
    if (feof(in))
        errorc("corrupted header file (line %i)", __LINE__);
    int i;
    fread(&i, sizeof(int), 1, in);
    return i;
}

static char* impl_readstr(FILE* in)
{
    if (feof(in))
        errorc("corrupted header file (line %i)", __LINE__);
    buffer_t* buffer = buffer_init(1024, 64);
    for (char c = fgetc(in); c; c = fgetc(in))
    {
        buffer_append(buffer, c);
        if (feof(in)) errorc("corrupted header file (line %i)", __LINE__);
    }
    buffer_append(buffer, '\0');
    return buffer_export(buffer);
}

static datatype_t* impl_readdt(FILE* in)
{
    char visibility = read;
    char dtt = read;
    char size = read;
    char usign = read;
    char* name = NULL;
    char depth = 0;
    if (dtt == DTT_OBJECT)
        name = readstr;
    if (dtt == DTT_ARRAY)
        depth = read;
    datatype_t* dt = calloc(1, sizeof(datatype_t));
    dt->visibility = visibility;
    dt->type = dtt;
    dt->size = size;
    dt->usign = usign;
    dt->name = name;
    dt->depth = 0;
    for (int i = 0; i < depth; i++)
    {
        datatype_t* array = calloc(1, sizeof(datatype_t));
        array->visibility = visibility;
        array->type = DTT_ARRAY;
        array->size = 8;
        array->usign = false;
        array->name = NULL;
        array->depth = i + 1;
        array->array_type = dt;
        dt = array;
    }
    return dt;
}

static ast_node_t* impl_readfunc(FILE* in, char* filename, char* ident, ast_node_t* bp)
{
    datatype_t* rettype = readdt;
    char func_type = read;
    char lowlvl = read;
    short operator = readi16;
    short arg_count = readi16;
    vector_t* args = vector_init(arg_count, 1);
    for (int i = 0; i < arg_count; i++)
    {
        char* name = readstr;
        datatype_t* dt = readdt;
        vector_push(args, ast_lvar_init(dt, NULL, name, NULL, filename));
    }
    ast_node_t* node = ast_builtin_init(rettype, ident, args, filename, 'u');
    node->func_type = func_type;
    node->lowlvl = lowlvl;
    node->operator = operator;
    node->func_label = make_func_label(filename, node, bp);
    return node;
}

// wb mode on fopen for this function
void write_header(FILE* out, map_t* genv)
{
    for (int i = 0; i < genv->capacity; i++)
    {
        char* key = genv->key[i];
        if (!key)
            continue;
        ast_node_t* node = genv->value[i];
        if (!node)
            continue;
        if (node->type == AST_FUNC_DEFINITION && node->extrn)
            continue;
        writei16(node->type);
        switch (node->type)
        {
            case AST_GVAR:
            {
                writestr(node->var_name);
                break;
            }
            case AST_FUNC_DEFINITION:
            {
                writestr(node->func_name);
                break;
            }
            case AST_BLUEPRINT:
            {
                writestr(node->bp_name);
                break;
            }
        }
        writedt(node->datatype);
        switch (node->type)
        {
            case AST_FUNC_DEFINITION:
                writefunc(node);
                break;
            case AST_BLUEPRINT:
            {
                write(node->bp_datatype->visibility);
                writei32(node->bp_size);
                writei16(node->inst_variables->size);
                for (int i = 0; i < node->inst_variables->size; i++)
                {
                    ast_node_t* inst_var = vector_get(node->inst_variables, i);
                    writestr(inst_var->var_name);
                    writedt(inst_var->datatype);
                }
                writei16(node->methods->size);
                for (int i = 0; i < node->methods->size; i++)
                {
                    ast_node_t* method = vector_get(node->methods, i);
                    writestr(method->func_name);
                    writefunc(method);
                }
            }
        }
    }
}

vector_t* read_header(FILE* in, char* filename)
{
    vector_t* decls = vector_init(25, 10);
    for (;;)
    {
        short decl_type;
        if (fread(&decl_type, sizeof(short), 1, in) < 1)
            break;
        char* ident = readstr;
        datatype_t* dt = readdt;
        switch (decl_type)
        {
            case AST_FUNC_DEFINITION:
            {
                vector_push(decls, readfunc(NULL));
                break;
            }
            case AST_BLUEPRINT:
            {
                char visibility = read;
                int size = readi32;
                short inst_var_count = readi16;
                ast_node_t* bp = vector_push(decls, ast_blueprint_init(NULL, ident, NULL));
                vector_t* inst_vars = vector_init(10, 5);
                for (int i = 0; i < inst_var_count; i++)
                {
                    char* ident = readstr;
                    datatype_t* dt = readdt;
                    vector_push(inst_vars, ast_lvar_init(dt, NULL, ident, NULL, filename));
                }
                short method_count = readi16;
                vector_t* methods = vector_init(10, 5);
                for (int i = 0; i < methods->size; i++)
                {
                    char* ident = readstr;
                    vector_push(methods, readfunc(bp));
                }
                datatype_t* dt = calloc(1, sizeof(datatype_t));
                dt->array_type = NULL;
                dt->depth = 0;
                dt->length = NULL;
                dt->name = ident;
                dt->size = 8;
                dt->type = DTT_OBJECT;
                dt->usign = false;
                dt->visibility = visibility;
                bp->bp_datatype = dt;
                bp->inst_variables = inst_vars;
                bp->methods = methods;
                bp->bp_size = size;
                vector_push(decls, bp);
                break;
            }
            case AST_GVAR:
            {
                errorc(0, 0, "tell dev to add global variables lol");
                break;
            }
        }
    }
    return decls;
}