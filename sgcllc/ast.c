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

ast_node_t* ast_func_definition_init(datatype_t* dt, location_t* loc, char func_type, char* func_name, char* residing)
{
    return ast_init(AST_FUNC_DEFINITION, dt, loc, &(ast_node_t){
        .func_type = func_type,
        .func_name = func_name,
        .residing = residing,
        .extrn = '\0',
        .params = vector_init(10, 3),
        .local_variables = vector_init(10, 5),
        .body = ast_block_init(loc),
        .end_label = NULL
    });
}

ast_node_t* ast_func_call_init(datatype_t* dt, location_t* loc, ast_node_t* func, vector_t* args)
{
    return ast_init(AST_FUNC_CALL, dt, loc, &(ast_node_t){
        .func = func,
        .args = args
    });
}

ast_node_t* ast_builtin_init(datatype_t* dt, char* func_name, vector_t* params, char* residing, char extrn)
{
    return ast_init(AST_FUNC_DEFINITION, dt, NULL, &(ast_node_t){
        .func_type = 'g',
        .func_name = func_name,
        .func_label = func_name,
        .residing = residing,
        .extrn = extrn,
        .params = params,
        .local_variables = NULL,
        .body = NULL
    });
}

ast_node_t* ast_lvar_init(datatype_t* dt, location_t* loc, char* lvar_name, ast_node_t* vinit, char* residing)
{
    return ast_init(AST_LVAR, dt, loc, &(ast_node_t){
        .var_name = lvar_name,
        .voffset = 0,
        .vinit = vinit,
        .residing = residing
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

ast_node_t* ast_unary_op_init(ast_node_type type, datatype_t* dt, location_t* loc, ast_node_t* operand)
{
    return ast_init(type, dt, loc, &(ast_node_t){
        .operand = operand
    });
}

ast_node_t* ast_return_init(datatype_t* dt, location_t* loc, ast_node_t* retval, ast_node_t* retfunc)
{
    return ast_init(AST_RETURN, dt, loc, &(ast_node_t){
        .retval = retval,
        .retfunc = retfunc
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

ast_node_t* ast_for_init(datatype_t* dt, location_t* loc, ast_node_t* for_init, ast_node_t* for_cond, ast_node_t* for_post)
{
    return ast_init(AST_FOR, dt, loc, &(ast_node_t){
        .for_init = for_init,
        .for_cond = for_cond,
        .for_post = for_post,
        .for_then = ast_block_init(loc)
    });
}

ast_node_t* ast_cast_init(datatype_t* dt, location_t* loc, ast_node_t* castval)
{
    return ast_init(AST_CAST, dt, loc, &(ast_node_t){
        .castval = castval
    });
}

ast_node_t* ast_stub_init(ast_node_type type, datatype_t* dt, location_t* loc)
{
    return ast_init(type, dt, loc, &(ast_node_t){});
}

ast_node_t* ast_make_init(datatype_t* dt, location_t* loc)
{
    return ast_init(AST_MAKE, dt, loc, &(ast_node_t){});
}

ast_node_t* ast_blueprint_init(location_t* loc, char* bp_name, datatype_t* dt)
{
    return ast_init(AST_BLUEPRINT, t_object, loc, &(ast_node_t){
        .bp_name = bp_name,
        .inst_variables = vector_init(5, 5),
        .methods = vector_init(10, 5),
        .bp_datatype = dt
    });
}

ast_node_t* ast_namespace_init(location_t* loc, char* ns_name)
{
    return ast_init(AST_NAMESPACE, NULL, loc, &(ast_node_t){
        .ns_name = ns_name
    });
}

ast_node_t* ast_ternary_init(datatype_t* dt, location_t* loc, ast_node_t* cond, ast_node_t* then, ast_node_t* els)
{
    return ast_init(AST_TERNARY, dt, loc, &(ast_node_t){
        .tern_cond = cond,
        .tern_then = then,
        .tern_els = els
    });
}

ast_node_t* ast_switch_init(location_t* loc, ast_node_t* cmp)
{
    return ast_init(AST_SWITCH, cmp->datatype, loc, &(ast_node_t){
        .cmp = cmp,
        .cases = vector_init(10, 5)
    });
}

ast_node_t* ast_case_init(location_t* loc)
{
    return ast_init(AST_CASE, NULL, loc, &(ast_node_t){
        .case_conditions = vector_init(3, 5),
        .case_then = ast_block_init(loc)
    });
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
        case DTT_OBJECT:
        {
            indprintf(indent, "name: %s\n", dt->name);
            break;
        }
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
            break;
        }
    }
}

void print_datatype(datatype_t* dt)
{
    ast_print_datatype(dt, 0);
}

static void std_ast_print(ast_node_t* node, int indent)
{
    indprintf(indent, "datatype: {\n");
    indent++;
    ast_print_datatype(node->datatype, indent);
    indent--;
    indprintf(indent, "}\n");
    if (node->loc)
    {
        indprintf(indent, "loc: {\n");
        indent++;
        indprintf(indent, "offset: %i\n", node->loc->offset);
        indprintf(indent, "row: %i\n", node->loc->row);
        indprintf(indent, "col: %i\n", node->loc->col);
        indent--;
        indprintf(indent, "}\n");
    }
    else
        indprintf(indent, "loc: (null)\n");
    indprintf(indent, "residing: %s\n", node->residing);
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
            indprintf(indent, "func_label: %s\n", node->func_label);
            indprintf(indent, "params: [\n");
            indent++;
            for (int i = 0; i < node->params->size; i++)
                ast_print_recur((ast_node_t*) vector_get(node->params, i), indent);
            indent--;
            indprintf(indent, "]\n");
            if (node->local_variables)
            {
                indprintf(indent, "local_variables: [\n");
                indent++;
                for (int i = 0; i < node->local_variables->size; i++)
                    ast_print_recur((ast_node_t*) vector_get(node->local_variables, i), indent);
                indent--;
                indprintf(indent, "]\n");
            }
            else
                indprintf(indent, "local_variables: (null)\n");
            if (node->body)
            {
                indprintf(indent, "body: {\n");
                indent++;
                ast_print_recur(node->body, indent);
                indent--;
                indprintf(indent, "}\n");
            }
            else
                indprintf(indent, "body: (null)\n");
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
            indprintf(indent, "lvoffset: %i\n", node->voffset);
            if (node->vinit)
            {
                indprintf(indent, "vinit: {\n");
                indent++;
                ast_print_recur(node->vinit->rhs, indent);
                indent--;
                indprintf(indent, "}\n");
            }
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
        case OP_SUBSCRIPT:
        case OP_SELECTION:
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
        case AST_BLUEPRINT:
        {
            indprintf(indent, "AST_BLUEPRINT {\n");
            indent++;
            std_ast_print(node, indent);
            indprintf(indent, "bp_name: %s\n", node->bp_name);
            indprintf(indent, "bp_size: %i\n", node->bp_size);
            indprintf(indent, "bp_datatype: {\n");
            indent++;
            ast_print_datatype(node->bp_datatype, indent);
            indent--;
            indprintf(indent, "}\n");
            indprintf(indent, "inst_variables: [\n");
            indent++;
            for (int i = 0; i < node->inst_variables->size; i++)
                ast_print_recur((ast_node_t*) vector_get(node->inst_variables, i), indent);
            indent--;
            indprintf(indent, "]\n");
            indprintf(indent, "methods: [\n");
            indent++;
            for (int i = 0; i < node->methods->size; i++)
                ast_print_recur((ast_node_t*) vector_get(node->methods, i), indent);
            indent--;
            indprintf(indent, "]\n");
            indent--;
            indprintf(indent, "}\n");
            break;
        }
        case AST_FUNC_CALL:
        {
            indprintf(indent, "AST_FUNC_CALL {\n");
            indent++;
            std_ast_print(node, indent);
            indprintf(indent, "func: %s\n", node->func->func_label);
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
            std_ast_print(node, indent);
            indprintf(indent, "ivalue: %i\n", node->ivalue);
            indent--;
            indprintf(indent, "}\n");
            break;
        }
        case AST_FLITERAL:
        {
            indprintf(indent, "AST_FLITERAL {\n");
            indent++;
            std_ast_print(node, indent);
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
            std_ast_print(node, indent);
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
        case AST_TERNARY:
        {
            indprintf(indent, "AST_TERNARY {\n");
            indent++;
            std_ast_print(node, indent);
            indprintf(indent, "condition: {\n");
            indent++;
            ast_print_recur(node->tern_cond, indent);
            indent--;
            indprintf(indent, "}\n");
            indprintf(indent, "then: {\n");
            indent++;
            ast_print_recur(node->tern_then, indent);
            indent--;
            indprintf(indent, "}\n");
            indprintf(indent, "else: {\n");
            indent++;
            ast_print_recur(node->tern_els, indent);
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