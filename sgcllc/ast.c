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
        .decls = vector_init(10, 10),
        .imports = vector_init(10, 10)
    });
}

ast_node_t* ast_import_init(location_t* loc, char* path)
{
    return ast_init(AST_IMPORT, NULL, loc, &(ast_node_t){
        .path = path
    });
}

ast_node_t* ast_block_init(location_t* loc)
{
    return ast_init(AST_BLOCK, NULL, loc, &(ast_node_t){
        .statements = vector_init(10, 10)
    });
}

ast_node_t* ast_func_definition_init(datatype_t* dt, location_t* loc, char* func_name)
{
    return ast_init(AST_FUNC_DEFINITION, dt, loc, &(ast_node_t){
        .func_name = func_name,
        .params = vector_init(10, 3),
        .local_variables = vector_init(10, 5),
        .body = ast_block_init(loc)
    });
}

ast_node_t* ast_func_call_init(datatype_t* dt, location_t* loc, ast_node_t* func, vector_t* args)
{
    return ast_init(AST_FUNC_CALL, dt, loc, &(ast_node_t){
        .func = func,
        .args = args
    });
}

ast_node_t* ast_builtin_init(datatype_t* dt, char* func_name, vector_t* params)
{
    return ast_init(AST_FUNC_DEFINITION, dt, NULL, &(ast_node_t){
        .func_name = func_name,
        .params = params,
        .local_variables = NULL,
        .body = NULL
    });
}

ast_node_t* ast_lvar_init(datatype_t* dt, location_t* loc, char* lvar_name)
{
    return ast_init(AST_LVAR, dt, loc, &(ast_node_t){
        .var_name = lvar_name,
        .lvoffset = 0
    });
}

ast_node_t* ast_iliteral_init(datatype_t* dt, location_t* loc, long long ivalue)
{
    return ast_init(AST_ILITERAL, dt, loc, &(ast_node_t){
        .ivalue = ivalue
    });
}

ast_node_t* ast_sliteral_init(datatype_t* dt, location_t* loc, char* svalue)
{
    return ast_init(AST_SLITERAL, dt, loc, &(ast_node_t){
        .svalue = svalue
    });
}

ast_node_t* ast_binary_op_init(ast_node_type type, datatype_t* dt, location_t* loc, ast_node_t* lhs, ast_node_t* rhs)
{
    return ast_init(type, dt, loc, &(ast_node_t){
        .lhs = lhs,
        .rhs = rhs
    });
}

static void std_ast_print(ast_node_t* node, int indent)
{
    indprintf(indent, "datatype: %i\n", node->datatype ? node->datatype->type : -1);
    indprintf(indent, "loc: {\n");
    indent++;
    indprintf(indent, "offset: %i\n", node->loc->offset);
    indprintf(indent, "row: %i\n", node->loc->row);
    indprintf(indent, "col: %i\n", node->loc->col);
    indent--;
    indprintf(indent, "}\n");
}

static void ast_print_recur(ast_node_t* node, int indent)
{
    switch (node->type)
    {
        case AST_FILE:
        {
            indprintf(indent, "AST_FILE {\n");
            indent++;
            std_ast_print(node, indent);
            indprintf(indent, "decls: [\n");
            indent++;
            for (int i = 0; i < node->decls->size; i++)
                ast_print_recur((ast_node_t*) vector_get(node->decls, i), indent);
            indent--;
            indprintf(indent, "]\n");
            indprintf(indent, "imports: [\n");
            indent++;
            for (int i = 0; i < node->imports->size; i++)
                ast_print_recur((ast_node_t*) vector_get(node->imports, i), indent);
            indent--;
            indprintf(indent, "]\n");
            indent--;
            indprintf(indent, "}\n");
            break;
        }
        case AST_IMPORT:
        {
            indprintf(indent, "AST_IMPORT {\n");
            indent++;
            std_ast_print(node, indent);
            indprintf(indent, "path: %s\n", node->path);
            indent--;
            indprintf(indent, "}\n");
            break;
        }
        case AST_FUNC_DEFINITION:
        {
            indprintf(indent, "AST_FUNC_DEFINITION {\n");
            indent++;
            std_ast_print(node, indent);
            indprintf(indent, "func_name: %s\n", node->func_name);
            indprintf(indent, "params: [\n");
            indent++;
            for (int i = 0; i < node->params->size; i++)
                ast_print_recur((ast_node_t*) vector_get(node->params, i), indent);
            indent--;
            indprintf(indent, "]\n");
            indprintf(indent, "local_variables: [\n");
            indent++;
            for (int i = 0; i < node->local_variables->size; i++)
                ast_print_recur((ast_node_t*) vector_get(node->local_variables, i), indent);
            indent--;
            indprintf(indent, "]\n");
            indprintf(indent, "body: {\n");
            indent++;
            ast_print_recur(node->body, indent);
            indent--;
            indprintf(indent, "}\n");
            indent--;
            indprintf(indent, "}\n");
            break;
        }
        case AST_LVAR:
        {
            indprintf(indent, "AST_LVAR {\n");
            indent++;
            std_ast_print(node, indent);
            indprintf(indent, "var_name: %s\n", node->var_name);
            indprintf(indent, "lvoffset: %i\n", node->lvoffset);
            indent--;
            indprintf(indent, "}\n");
            break;
        }
        case AST_BLOCK:
        {
            indprintf(indent, "AST_BLOCK {\n");
            indent++;
            std_ast_print(node, indent);
            indprintf(indent, "statements: [\n");
            indent++;
            for (int i = 0; i < node->statements->size; i++)
                ast_print_recur((ast_node_t*) vector_get(node->statements, i), indent);
            indent--;
            indprintf(indent, "]\n");
            indent--;
            indprintf(indent, "}\n");
            break;
        }
        case OP_ASSIGN:
        case OP_ADD:
        {
            indprintf(indent, "AST_BINARY_OP (%c) {\n", node->type);
            indent++;
            std_ast_print(node, indent);
            indprintf(indent, "lhs: {\n");
            indent++;
            ast_print_recur(node->lhs, indent);
            indent--;
            indprintf(indent, "}\n");
            indprintf(indent, "rhs: {\n");
            indent++;
            ast_print_recur(node->rhs, indent);
            indent--;
            indprintf(indent, "}\n");
            indent--;
            indprintf(indent, "}\n");
            break;
        }
        case AST_FUNC_CALL:
        {
            indprintf(indent, "AST_FUNC_CALL {\n");
            indent++;
            std_ast_print(node, indent);
            indprintf(indent, "func: %s\n", node->func->func_name);
            indprintf(indent, "args: [\n");
            indent++;
            for (int i = 0; i < node->args->size; i++)
                ast_print_recur((ast_node_t*) vector_get(node->args, i), indent);
            indent--;
            indprintf(indent, "]\n");
            indent--;
            indprintf(indent, "}\n");
            break;
        }
        case AST_ILITERAL:
        {
            indprintf(indent, "AST_ILITERAL {\n");
            indent++;
            indprintf(indent, "ivalue: %i\n", node->ivalue);
            indent--;
            indprintf(indent, "}\n");
            break;
        }
    }
}

void ast_print(ast_node_t* node)
{
    ast_print_recur(node, 0);
}