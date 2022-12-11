#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>

#include "sgcllc.h"

const char* x64cc = "cd89";

#define emit(...) emitf(e, "\t" __VA_ARGS__)
#define emit_noindent(...) emitf(e, __VA_ARGS__)

static void emit_file(emitter_t* e, ast_node_t* file);
static void emit_gvar_decl(emitter_t* e, ast_node_t* gvar);
static void emit_func_definition(emitter_t* e, ast_node_t* func_definition);
static void emit_stmt(emitter_t* e, ast_node_t* stmt);
static void emit_expr(emitter_t* e, ast_node_t* expr);
static void emit_lvar_decl(emitter_t* e, ast_node_t* lvar);

int find_stackalloc(vector_t* lvars)
{
    int stackalloc = 32;
    for (int i = 0; i < lvars->size; i++)
    {
        ast_node_t* node = vector_get(lvars, i);
        stackalloc += node->datatype->size;
    }
    return stackalloc + 16 - (stackalloc % 16);
}

char size_ident(int size)
{
    if (size <= 1)
        return 'b';
    if (size <= 2)
        return 'w';
    if (size <= 4)
        return 'd';
    if (size <= 8)
        return 'q';
    return '\0';
}

char* find_register(register_type rt, int size)
{
    #define reg(n, t, s, isfloat) \
    if (rt == t && size == s) \
        return n;
    #include "registers.inc"
    #undef reg
    return NULL;
}

void emitf(emitter_t* e, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vfprintf(e->out, fmt, args);
    va_end(args);
    fprintf(e->out, "\n");
}

emitter_t* emitter_init(parser_t* p, FILE* out)
{
    emitter_t* e = calloc(1, sizeof(emitter_t));
    e->p = p;
    e->out = out;
    return e;
}

static void emit_file(emitter_t* e, ast_node_t* file)
{
    for (int i = 0; i < file->decls->size; i++)
    {
        ast_node_t* node = (ast_node_t*) vector_get(file->decls, i);
        if (node->type == AST_GVAR)
            emit_gvar_decl(e, node);
        else if (node->type == AST_FUNC_DEFINITION)
            emit_func_definition(e, node);
    }
    for (int i = 0; i < file->imports->size; i++)
    {
        ast_node_t* node = (ast_node_t*) vector_get(file->imports, i);
        errore(node->loc->row, node->loc->col, "import statements have not been implemented yet");
    }
}

static void emit_gvar_decl(emitter_t* e, ast_node_t* gvar)
{
    errore(gvar->loc->row, gvar->loc->col, "global variable decls are not implemented yet");
}

static void emit_func_definition(emitter_t* e, ast_node_t* func_definition)
{
    emit_noindent("%s:", func_definition->func_name);
    emit("pushq %%rbp");
    emit("movq %%rsp, %%rbp");
    int stackalloc = find_stackalloc(func_definition->local_variables);
    emit("subq %i, %%rsp", stackalloc);
    for (int i = 0; i < min(func_definition->params->size, 4); i++)
    {
        ast_node_t* param = (ast_node_t*) vector_get(func_definition->params, i);
        emit("mov%c %%%s, %i(%%rbp)", size_ident(param->datatype->size), find_register(x64cc[i], param->datatype->size), 16 + (i * 8));
    }
    for (int i = 0; i < func_definition->body->statements->size; i++)
        emit_stmt(e, vector_get(func_definition->body->statements, i));
    emit("addq %i, %%rsp", stackalloc);
    emit("popq %%rbp");
    emit("ret");
}

static void emit_lvar_decl(emitter_t* e, ast_node_t* lvar)
{
    lvar->lvoffset = (e->stackoffset += lvar->datatype->size);
}

static void emit_assign_op(emitter_t* e, ast_node_t* op)
{
    switch (op->lhs->type)
    {
        case AST_LVAR:
        {
            emit_expr(e, op->rhs);
            ast_print(op->lhs);
            ast_print(op->rhs);
            printf("%p, %i\n", op->lhs->datatype, op->lhs->datatype->type);
            emit("mov%c %%%s, -%i(%%rbp)", size_ident(op->lhs->datatype->size), find_register(REG_A, op->lhs->datatype->size), op->lhs->lvoffset);
            break;
        }
        default:
            errore(op->loc->row, op->loc->col, "assignment operator cannot be applied to left side of this expression");
    }
}

static void emit_expr(emitter_t* e, ast_node_t* expr)
{
    switch (expr->type)
    {
        case '=':
        {
            emit_assign_op(e, expr);
            break;
        }
        case AST_ILITERAL:
        {
            emit("mov%c $%i, %%%s", size_ident(expr->datatype->size), expr->ivalue, find_register(REG_A, expr->datatype->size));
            break;
        }
        default:
            errore(expr->loc->row, expr->loc->col, "unable to emit expressions of type %i at this time", expr->type);
    }
}

static void emit_stmt(emitter_t* e, ast_node_t* stmt)
{
    switch (stmt->type)
    {
        case AST_LVAR:
        {
            emit_lvar_decl(e, stmt);
            break;
        }
        default:
        {
            emit_expr(e, stmt);
            break;
        }
    }
}

void emitter_emit(emitter_t* e)
{
    emit_file(e, e->p->nfile);
}

emitter_t* emitter_delete(emitter_t* e)
{
    free(e);
}