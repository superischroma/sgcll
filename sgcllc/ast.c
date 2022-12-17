#include <stdio.h>
#include <stdlib.h>

#include "sgcllc.h"

static void ast_print_recur(ast_node_t* node, int indent);

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
        .extrn = false,
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
        .extrn = true,
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

ast_node_t* ast_fliteral_init(datatype_t* dt, location_t* loc, double fvalue, char* flabel)
{
    return ast_init(AST_FLITERAL, dt, loc, &(ast_node_t){
        .fvalue = fvalue,
        .flabel = flabel
    });
}

ast_node_t* ast_sliteral_init(datatype_t* dt, location_t* loc, char* svalue, char* slabel)
{
    return ast_init(AST_SLITERAL, dt, loc, &(ast_node_t){
        .svalue = svalue,
        .slabel = slabel
    });
}

ast_node_t* ast_binary_op_init(ast_node_type type, datatype_t* dt, location_t* loc, ast_node_t* lhs, ast_node_t* rhs)
{
    return ast_init(type, dt, loc, &(ast_node_t){
        .lhs = lhs,
        .rhs = rhs
    });
}

ast_node_t* ast_return_init(datatype_t* dt, location_t* loc, ast_node_t* retval)
{
    return ast_init(AST_RETURN, dt, loc, &(ast_node_t){
        .retval = retval
    });
}

ast_node_t* ast_delete_init(datatype_t* dt, location_t* loc, ast_node_t* delsym)
{
    return ast_init(AST_DELETE, dt, loc, &(ast_node_t){
        .delsym = delsym
    });
}

ast_node_t* ast_if_init(datatype_t* dt, location_t* loc, ast_node_t* if_cond)
{
    return ast_init(AST_IF, dt, loc, &(ast_node_t){
        .if_cond = if_cond,
        .if_then = ast_block_init(loc),
        .if_els = ast_block_init(loc)
    });
}

ast_node_t* ast_while_init(datatype_t* dt, location_t* loc, ast_node_t* while_cond)
{
    return ast_init(AST_WHILE, dt, loc, &(ast_node_t){
        .while_cond = while_cond,
        .while_then = ast_block_init(loc)
    });
}

ast_node_t* ast_cast_init(datatype_t* dt, location_t* loc, ast_node_t* castval)
{
    return ast_init(AST_CAST, dt, loc, &(ast_node_t){
        .castval = castval
    });
}

ast_node_t* ast_stub_init(datatype_t* dt, location_t* loc)
{
    return ast_init(AST_STUB, dt, loc, &(ast_node_t){});
}

ast_node_t* ast_make_init(datatype_t* dt, location_t* loc)
{
    return ast_init(AST_MAKE, dt, loc, &(ast_node_t){});
}

static void ast_print_datatype(datatype_t* dt, int indent)
{
    if (!dt) return;
    indprintf(indent, "visibility: %i\n", dt->visibility);
    indprintf(indent, "type: %i\n", dt->type);
    indprintf(indent, "size: %i\n", dt->size);
    indprintf(indent, "usign: %i\n", dt->usign);
    switch (dt->type)
    {
        case DTT_ARRAY:
        {
            indprintf(indent, "array_type: {\n");
            indent++;
            ast_print_datatype(dt->array_type, indent);
            indent--;
            indprintf(indent, "}\n");
            if (dt->length)
            {
                indprintf(indent, "length: {\n");
                indent++;
                ast_print_recur(dt->length, indent);
                indent--;
                indprintf(indent, "}\n");
            }
            else
                indprintf(indent, "length: undefined\n");
            indprintf(indent, "depth: %i\n", dt->depth);
        }
    }
}

static void std_ast_print(ast_node_t* node, int indent)
{
    indprintf(indent, "datatype: {\n");
    indent++;
    ast_print_datatype(node->datatype, indent);
    indent--;
    indprintf(indent, "}\n");
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
        case OP_SUB:
        case OP_MUL:
        case OP_DIV:
        case OP_MOD:
        case OP_ASSIGN_ADD:
        case OP_ASSIGN_SUB:
        case OP_ASSIGN_MUL:
        case OP_ASSIGN_DIV:
        case OP_ASSIGN_MOD:
        {
            indprintf(indent, "AST_BINARY_OP (%c, id: %i) {\n", node->type, node->type);
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
        case AST_FLITERAL:
        {
            indprintf(indent, "AST_FLITERAL {\n");
            indent++;
            indprintf(indent, "fvalue: %f\n", node->fvalue);
            indprintf(indent, "flabel: %s\n", node->flabel);
            indent--;
            indprintf(indent, "}\n");
            break;
        }
        case AST_SLITERAL:
        {
            indprintf(indent, "AST_SLITERAL {\n");
            indent++;
            indprintf(indent, "svalue: %s\n", node->svalue);
            indprintf(indent, "slabel: %s\n", node->slabel);
            indent--;
            indprintf(indent, "}\n");
            break;
        }
        case AST_RETURN:
        {
            indprintf(indent, "AST_RETURN {\n");
            indent++;
            std_ast_print(node, indent);
            indprintf(indent, "retval: {\n");
            indent++;
            ast_print_recur(node->retval, indent);
            indent--;
            indprintf(indent, "}\n");
            indent--;
            indprintf(indent, "}\n");
            break;
        }
        case AST_DELETE:
        {
            indprintf(indent, "AST_DELETE {\n");
            indent++;
            std_ast_print(node, indent);
            indprintf(indent, "delsym: {\n");
            indent++;
            ast_print_recur(node->delsym, indent);
            indent--;
            indprintf(indent, "}\n");
            indent--;
            indprintf(indent, "}\n");
            break;
        }
        case AST_IF:
        {
            indprintf(indent, "AST_IF {\n");
            indent++;
            std_ast_print(node, indent);
            indprintf(indent, "condition: {\n");
            indent++;
            ast_print_recur(node->if_cond, indent);
            indent--;
            indprintf(indent, "}\n");
            indprintf(indent, "then: {\n");
            indent++;
            ast_print_recur(node->if_then, indent);
            indent--;
            indprintf(indent, "}\n");
            indprintf(indent, "otherwise: {\n");
            indent++;
            ast_print_recur(node->if_els, indent);
            indent--;
            indprintf(indent, "}\n");
            indent--;
            indprintf(indent, "}\n");
            break;
        }
        case AST_CAST:
        {
            indprintf(indent, "AST_CAST {\n");
            indent++;
            std_ast_print(node, indent);
            indprintf(indent, "castval: {\n", node->fvalue);
            indent++;
            ast_print_recur(node->castval, indent);
            indent--;
            indprintf(indent, "}\n");
            indent--;
            indprintf(indent, "}\n");
            break;
        }
        case AST_MAKE:
        {
            indprintf(indent, "AST_MAKE {\n");
            indent++;
            std_ast_print(node, indent);
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