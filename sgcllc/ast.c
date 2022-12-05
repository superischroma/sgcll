#include <stdio.h>
#include <stdlib.h>

#include "sgcllc.h"

static ast_node_t* ast_init(ast_node_type type, datatype_t* datatype, location_t* loc, ast_node_t* base)
{
    ast_node_t* node = calloc(1, sizeof(ast_node_t));
    *node = *base;
    node->type = type;
    node->datatype = datatype;
    node->loc = loc;
    return node;
}

ast_node_t* ast_file_init(location_t* loc)
{
    return ast_init(AST_FILE, NULL, loc, &(ast_node_t){
        .decls = calloc(1, sizeof(vector_t)),
        .imports = calloc(1, sizeof(vector_t))
    });
}

ast_node_t* ast_import_init(location_t* loc, char* path)
{
    return ast_init(AST_IMPORT, NULL, loc, &(ast_node_t){
        .path = path
    });
}

static void std_ast_print(ast_node_t* node, int indent)
{
    indprintf(indent, "datatype: %i\n", node->datatype ? node->datatype->size : -1);
    indprintf(indent, "loc:\n");
    indent++;
    indprintf(indent, "offset: %i\n", node->loc->offset);
    indprintf(indent, "row: %i\n", node->loc->row);
    indprintf(indent, "col: %i\n", node->loc->col);
}

static void ast_print_recur(ast_node_t* node, int indent)
{
    switch (node->type)
    {
        case AST_FILE:
        {
            indprintf(indent, "AST_FILE:\n");
            indent++;
            std_ast_print(node, indent);
            indprintf(indent, "decls:\n");
            indent++;
            for (int i = 0; i < node->decls->size; i++)
                ast_print_recur((ast_node_t*) vector_get(node->decls, i), indent);
            indent--;
            indprintf(indent, "imports:\n");
            indent++;
            for (int i = 0; i < node->imports->size; i++)
                ast_print_recur((ast_node_t*) vector_get(node->imports, i), indent);
            indent--;
            indent--;
            break;
        }
        case AST_IMPORT:
        {
            indprintf(indent, "AST_IMPORT:\n");
            indent++;
            std_ast_print(node, indent);
            indprintf(indent, "path: %s\n", node->path);
            indent--;
            break;
        }
    }
}

void ast_print(ast_node_t* node)
{
    ast_print_recur(node, 0);
}