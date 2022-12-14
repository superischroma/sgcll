#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>

#include "sgcllc.h"

const char x64cc[] = "cd89"; // x64 windows calling convention registers: c, d, 8, 9
const char tmpreg[] = "bot012345"; // temporary registers: b, src, dest, 10, 11, 12, 13, 14, 15

#define TMPREG_COUNT sizeof(tmpreg) - 1

#define emit(...) emitf(e, "\t" __VA_ARGS__)
#define emit_noindent(...) emitf(e, __VA_ARGS__)

#define floatsize(i) (i == 4 ? 's' : 'd')

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

char int_reg_size(int size)
{
    if (size <= 1)
        return 'b';
    if (size <= 2)
        return 'w';
    if (size <= 4)
        return 'l';
    if (size <= 8)
        return 'q';
    return '\0';
}

char float_reg_size(int size)
{
    if (size <= 4)
        return 's';
    if (size <= 8)
        return 'd';
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

int deduce_register_size(char* reg)
{
    #define reg(n, t, s, isfloat) \
    if (!strcmp(reg, n)) \
        return s;
    #include "registers.inc"
    #undef reg
    return -1;
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
    e->itmp = 0;
    e->ftmp = 1;
    e->stackmax = 0;
    return e;
}

static void emitter_stash_int_reg(emitter_t* e, char* reg)
{
    int size = deduce_register_size(reg);
    if (e->itmp >= TMPREG_COUNT)
        errore(0, 0, "tell dev to add stack pushing lol");
    emit("mov%c %%%s, %%%s", int_reg_size(size), reg, find_register(tmpreg[e->itmp++], size));
}

static char* emitter_restore_int_reg(emitter_t* e, int size)
{
    return find_register(tmpreg[--e->itmp], size);
}

static void emitter_stash_float_reg(emitter_t* e, char* reg, int size)
{
    if (e->ftmp >= 16)
        errore(0, 0, "tell dev to add float stack pushing lol");
    emit("movs%c %%%s, %%xmm%i", floatsize(size), reg, e->ftmp++);
}

static char* emitter_restore_float_reg(emitter_t* e, int size)
{
    char* name = malloc(6);
    name[0] = 'x';
    name[1] = name[2] = 'm';
    itos(--e->ftmp, name + 3);
    return name;
}

static void emit_file(emitter_t* e, ast_node_t* file)
{
    for (int i = 0; i < e->p->externs->size; i++)
        emit(".extern %s", ((ast_node_t*) vector_get(e->p->externs, i))->func_name);
    emit(".global main");
    for (int i = 0; i < file->imports->size; i++)
    {
        ast_node_t* node = (ast_node_t*) vector_get(file->imports, i);
        errore(node->loc->row, node->loc->col, "import statements have not been implemented yet");
    }
    for (int i = 0; i < file->decls->size; i++)
    {
        ast_node_t* node = (ast_node_t*) vector_get(file->decls, i);
        if (node->type == AST_GVAR)
            emit_gvar_decl(e, node);
        else if (node->type == AST_FUNC_DEFINITION)
            emit_func_definition(e, node);
    }
    for (int i = 0; i < e->p->labels->capacity; i++)
    {
        char* key = e->p->labels->key[i];
        char* value = e->p->labels->value[i];
        if (key == NULL)
            continue;
        int valuelen = strlen(value);
        char* lastchar = &(value[valuelen - 1]);
        emit_noindent("%s:", key);
        if (value[0] == '"')
            emit(".ascii %s", value);
        else if (*lastchar == 'f' || *lastchar == 'F')
        {
            *lastchar = '\0';
            emit(".single %s", value);
            *lastchar = 'f';
        }
        else
        {
            if (*lastchar == 'd' || *lastchar == 'D') *lastchar = '\0';
            emit(".double %s", value);
            if (*lastchar == '\0') *lastchar = 'd';
        }
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
    emit("subq $%i, %%rsp", stackalloc);
    for (int i = 0; i < min(func_definition->params->size, 4); i++)
    {
        ast_node_t* param = (ast_node_t*) vector_get(func_definition->params, i);
        emit("mov%c %%%s, %i(%%rbp)", int_reg_size(param->datatype->size), find_register(x64cc[i], param->datatype->size), 16 + (i * 8));
    }
    for (int i = 0; i < func_definition->body->statements->size; i++)
        emit_stmt(e, vector_get(func_definition->body->statements, i));
    emit("addq $%i, %%rsp", stackalloc);
    emit("popq %%rbp");
    emit("ret");
}

static void emit_lvar_decl(emitter_t* e, ast_node_t* lvar)
{
    lvar->lvoffset = (e->stackoffset += lvar->datatype->size);
}

static void emit_conv(emitter_t* e, datatype_t* src, datatype_t* dest)
{
    int src_size = src->size, dest_size = dest->size;
    bool src_float = isfloattype(src->type), dest_float = isfloattype(dest->type);
    if (dest_size <= src_size && !src_float && !dest_float)
        return;
    if (dest_size == src_size && src_float && dest_float)
        return;
    if (!src_float && !dest_float)
        emit("movsx %%%s, %%%s", find_register(REG_A, src_size), find_register(REG_A, dest_size));
    else if (!src_float && dest_float)
    {
        emit("pxor %%xmm0, %%xmm0");
        emit("cvtsi2s%c%c %%%s, %%xmm0", floatsize(dest->size), int_reg_size(max(src->size, 4)), find_register(REG_A, max(src_size, 4)));
    }
    else if (src_float && !dest_float)
        emit("cvts%c2si%c %%xmm0, %%%s", floatsize(src->size), int_reg_size(max(dest->size, 4)), find_register(REG_A, max(dest_size, 4)));
    else
        emit("cvts%c2s%c %%xmm0, %%xmm0", floatsize(src->size), floatsize(dest->size));
}

static void emit_assign(emitter_t* e, ast_node_t* op)
{
    switch (op->lhs->type)
    {
        case AST_LVAR:
        {
            if (isfloattype(op->lhs->datatype->type))
                emit("movs%c %%xmm0, -%i(%%rbp)", floatsize(op->lhs->datatype->size), op->lhs->lvoffset);
            else
                emit("mov%c %%%s, -%i(%%rbp)", int_reg_size(op->lhs->datatype->size), find_register(REG_A, op->lhs->datatype->size), op->lhs->lvoffset);
            break;
        }
        default:
            errore(op->loc->row, op->loc->col, "assignment operator cannot be applied to left side of this expression");
    }
}

static void emit_int_add_sub(emitter_t* e, ast_node_t* op)
{
    ast_node_t* lhs = op->lhs, * rhs = op->rhs;
    char* operation = NULL;
    switch (op->type)
    {
        case OP_ADD: case OP_ASSIGN_ADD: operation = "add"; break;
        case OP_SUB: case OP_ASSIGN_SUB: operation = "sub"; break;
        default:
            errore(op->loc->row, op->loc->col, "unknown operation");
    }
    emit_expr(e, rhs);
    emit_conv(e, rhs->datatype, op->datatype);
    emitter_stash_int_reg(e, find_register(REG_A, op->datatype->size));
    emit_expr(e, lhs);
    emit_conv(e, lhs->datatype, op->datatype);
    emit("%s%c %%%s, %%%s", operation, int_reg_size(op->datatype->size), emitter_restore_int_reg(e, op->datatype->size), find_register(REG_A, op->datatype->size));
}

static void emit_float_add_sub_mul_div(emitter_t* e, ast_node_t* op)
{
    ast_node_t* lhs = op->lhs, * rhs = op->rhs;
    char* operation = NULL;
    switch (op->type)
    {
        case OP_ADD: case OP_ASSIGN_ADD: operation = "add"; break;
        case OP_SUB: case OP_ASSIGN_SUB: operation = "sub"; break;
        case OP_MUL: case OP_ASSIGN_MUL: operation = "mul"; break;
        case OP_DIV: case OP_ASSIGN_DIV: operation = "div"; break;
        default:
            errore(op->loc->row, op->loc->col, "unknown operation");
    }
    emit_expr(e, rhs);
    emit_conv(e, rhs->datatype, op->datatype);
    emitter_stash_float_reg(e, find_register(REG_FLOAT, 8), op->datatype->size);
    emit_expr(e, lhs);
    emit_conv(e, lhs->datatype, op->datatype);
    char* restored = emitter_restore_float_reg(e, op->datatype->size);
    emit("%ss%c %%%s, %%%s", operation, floatsize(op->datatype->size), restored, find_register(REG_FLOAT, 8));
    free(restored);
}

static void emit_add_sub(emitter_t* e, ast_node_t* op)
{
    if (!isfloattype(op->datatype->type))
        emit_int_add_sub(e, op);
    else
        emit_float_add_sub_mul_div(e, op);
}

static void emit_int_mul_div(emitter_t* e, ast_node_t* op)
{
    ast_node_t* lhs = op->lhs, * rhs = op->rhs;
    char* operation = NULL;
    switch (op->type)
    {
        case OP_MUL: case OP_ASSIGN_MUL: operation = "imul"; break;
        case OP_DIV:
        case OP_ASSIGN_DIV: 
        case OP_MOD:
        case OP_ASSIGN_MOD:
            operation = "idiv"; break;
        default:
            errore(op->loc->row, op->loc->col, "unknown operation");
    }
    char* regA = find_register(REG_A, op->datatype->size);
    emit_expr(e, rhs);
    emit_conv(e, rhs->datatype, op->datatype);
    emitter_stash_int_reg(e, regA);
    emit_expr(e, lhs);
    emit_conv(e, lhs->datatype, op->datatype);
    emit("%s%c %%%s", operation, int_reg_size(op->datatype->size), emitter_restore_int_reg(e, op->datatype->size));
    if (op->type == OP_MOD || op->type == OP_ASSIGN_MOD)
        emit("mov%c %%%s, %%%s", int_reg_size(op->datatype->size), find_register(REG_D, op->datatype->size), regA);
}

static void emit_float_mod(emitter_t* e, ast_node_t* op)
{
}

static void emit_mul_div(emitter_t* e, ast_node_t* op)
{
    if (!isfloattype(op->datatype->type))
        emit_int_mul_div(e, op);
    else
        emit_float_add_sub_mul_div(e, op);
}

static void emit_func_call(emitter_t* e, ast_node_t* call)
{
    for (int i = call->args->size - 1, j = 0; i >= 0; i--)
    {
        ast_node_t* arg = vector_get(call->args, i);
        if (i >= 4)
            errore(call->loc->row, call->loc->col, "function calls with 4+ arguments are not supported yet");
        else
        {
            emit_expr(e, arg);
            if (isfloattype(arg->datatype->type))
            {
                if (j != 0)
                    emit("movs%c %%xmm0, %%xmm%i", floatsize(arg->datatype->size), j);
            }
            else
                emit("mov%c %%%s, %%%s", int_reg_size(arg->datatype->size), find_register(REG_A, arg->datatype->type), find_register(x64cc[i], arg->datatype->type));
            j++;
        }
    }
    emit("call %s", call->func->func_name); // todo: handle xmm0 return values
}

static void emit_expr(emitter_t* e, ast_node_t* expr)
{
    switch (expr->type)
    {
        case OP_ASSIGN:
        {
            emit_expr(e, expr->rhs);
            if (expr->lhs->datatype != expr->rhs->datatype)
                emit_conv(e, expr->rhs->datatype, expr->lhs->datatype);
            emit_assign(e, expr);
            break;
        }
        case OP_ADD:
        case OP_SUB:
        {
            emit_add_sub(e, expr);
            break;
        }
        case OP_MUL:
        case OP_DIV:
        case OP_MOD:
        {
            emit_mul_div(e, expr);
            break;
        }
        case OP_ASSIGN_ADD:
        case OP_ASSIGN_SUB:
        {
            emit_add_sub(e, expr);
            if (expr->lhs->datatype != expr->rhs->datatype)
                emit_conv(e, expr->rhs->datatype, expr->lhs->datatype);
            emit_assign(e, expr);
            break;
        }
        case OP_ASSIGN_MUL:
        case OP_ASSIGN_DIV:
        case OP_ASSIGN_MOD:
        {
            emit_mul_div(e, expr);
            if (expr->lhs->datatype != expr->rhs->datatype)
                emit_conv(e, expr->rhs->datatype, expr->lhs->datatype);
            emit_assign(e, expr);
            break;
        }
        case AST_ILITERAL:
        {
            emit("mov%c $%i, %%%s", int_reg_size(expr->datatype->size), expr->ivalue, find_register(REG_A, expr->datatype->size));
            break;
        }
        case AST_FLITERAL:
        {
            emit("movs%c %s(%%rip), %%xmm0", floatsize(expr->datatype->size), expr->flabel);
            break;
        }
        case AST_LVAR:
        {
            if (isfloattype(expr->datatype->type))
                emit("movs%c -%i(%%rbp), %%xmm0", floatsize(expr->datatype->size), expr->lvoffset);
            else
                emit("mov%c -%i(%%rbp), %%%s", int_reg_size(expr->datatype->size), expr->lvoffset, find_register(REG_A, expr->datatype->size));
            break;
        }
        case AST_FUNC_CALL:
        {
            emit_func_call(e, expr);
            break;
        }
        case AST_CAST:
        {
            emit_expr(e, expr->castval);
            emit_conv(e, expr->castval->datatype, expr->datatype);
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
        case AST_RETURN:
        {
            emit_expr(e, stmt->retval);
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