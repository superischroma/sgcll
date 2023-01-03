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
#define reference_load_operation(dt) (dt->ref ? "lea" : "mov")

static void emit_file(emitter_t* e, ast_node_t* file);
static void emit_gvar_decl(emitter_t* e, ast_node_t* gvar);
static void emit_func_definition(emitter_t* e, ast_node_t* func_definition, ast_node_t* blueprint);
static void emit_subscript(emitter_t* e, ast_node_t* op, bool deref);
static void emit_selection(emitter_t* e, ast_node_t* op, bool deref);
static void emit_stmt(emitter_t* e, ast_node_t* stmt);
static void emit_expr(emitter_t* e, ast_node_t* expr);
static void emit_lvar_decl(emitter_t* e, ast_node_t* lvar);

// operands should be evaluated in-order
int operand_priority(ast_node_t* op)
{
    switch (op->type)
    {
        case AST_FUNC_CALL:
            return 1;
        default:
            return 2;
    }
}

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

emitter_t* emitter_init(parser_t* p, FILE* out, bool control)
{
    emitter_t* e = calloc(1, sizeof(emitter_t));
    e->p = p;
    e->out = out;
    e->itmp = 0;
    e->ftmp = 4;
    e->stackmax = 0;
    e->control = control;
    return e;
}

static char* emitter_stash_int_reg(emitter_t* e, char* reg)
{
    int size = deduce_register_size(reg);
    if (e->itmp >= TMPREG_COUNT)
        errore(0, 0, "tell dev to add stack pushing lol");
    char* stashreg;
    emit("mov%c %%%s, %%%s", int_reg_size(size), reg, stashreg = find_register(tmpreg[e->itmp++], size));
    return stashreg;
}

static char* emitter_restore_int_reg(emitter_t* e, int size)
{
    return find_register(tmpreg[--e->itmp], size);
}

static char* emitter_stash_float_reg(emitter_t* e, char* reg, int size)
{
    if (e->ftmp >= 16)
        errore(0, 0, "tell dev to add float stack pushing lol");
    char* name = malloc(6);
    name[0] = 'x';
    name[1] = name[2] = 'm';
    itos(e->ftmp++, name + 3);
    emit("movs%c %%%s, %%%s", floatsize(size), reg, name);
    return name;
}

static char* emitter_restore_float_reg(emitter_t* e, int size)
{
    char* name = malloc(6);
    name[0] = 'x';
    name[1] = name[2] = 'm';
    itos(--e->ftmp, name + 3);
    return name;
}

static void emit_blueprint(emitter_t* e, ast_node_t* blueprint)
{
    for (int i = 0; i < blueprint->methods->size; i++)
        emit_func_definition(e, vector_get(blueprint->methods, i), blueprint);
}

static void emit_file(emitter_t* e, ast_node_t* file)
{
    for (int i = 0; i < e->p->userexterns->size; i++)
    {
        ast_node_t* userextern = vector_get(e->p->userexterns, i);
        if (userextern->lowlvl_label)
            continue;
        emit(".extern %s", userextern->func_label);
    }
    for (int i = 0; i < e->p->cexterns->size; i++)
    {
        ast_node_t* cextern = vector_get(e->p->cexterns, i);
        if (cextern->lowlvl_label)
            continue;
        emit(".extern %s", cextern->func_label);
    }
    ast_node_t* defaul_main = ast_func_definition_init(t_i32, NULL, 'g', e->p->entry, e->p->lex->filename);
    char* defaul_label = make_func_label(e->p->lex->filename, defaul_main, NULL);
    debugf("entry label: %s\n", defaul_label);
    if (map_get(e->p->genv, defaul_label))
    {
        emit_noindent("main:");
        emit("jmp %s", defaul_label);
        emit(".global main");
    }
    free(defaul_label);
    for (int i = 0; i < file->decls->size; i++)
    {
        ast_node_t* node = (ast_node_t*) vector_get(file->decls, i);
        if (node->type == AST_GVAR)
            emit_gvar_decl(e, node);
        else if (node->type == AST_FUNC_DEFINITION)
            emit_func_definition(e, node, NULL);
        else if (node->type == AST_BLUEPRINT)
            emit_blueprint(e, node);
    }
    for (int i = 0; i < e->p->labels->capacity; i++)
    {
        char* key = e->p->labels->key[i];
        char* value = e->p->labels->value[i];
        if (key == NULL || value == NULL)
            continue;
        int valuelen = strlen(value);
        char* lastchar = &(value[valuelen - 1]);
        emit_noindent("%s:", key);
        if (value[0] == '"')
            emit(".string %s", value);
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

static void emit_func_definition(emitter_t* e, ast_node_t* func_definition, ast_node_t* blueprint)
{
    if (func_definition->lowlvl_label)
        return;
    emit_noindent("%s:", func_definition->func_label);
    emit("pushq %%rbp");
    emit("movq %%rsp, %%rbp");
    int stackalloc = find_stackalloc(func_definition->local_variables);
    emit("subq $%i, %%rsp", func_definition->unsafe < 0 ? stackalloc : func_definition->unsafe);
    for (int i = 0; i < min(func_definition->params->size, 4); i++)
    {
        ast_node_t* param = (ast_node_t*) vector_get(func_definition->params, i);
        if (isfloattype(param->datatype->type))
            emit("movs%c %%xmm%i, %i(%%rbp)", floatsize(param->datatype->size), i, 16 + (i * 8));
        else
            emit("mov%c %%%s, %i(%%rbp)", int_reg_size(param->datatype->size), find_register(x64cc[i], param->datatype->size), 16 + (i * 8));
    }
    if (!strcmp(func_definition->func_name, "main"))
    {
        parser_ensure_cextern(e->p, "__libsgcllc_init", t_void, vector_init(DEFAULT_CAPACITY, DEFAULT_ALLOC_DELTA));
        emit("call __libsgcllc_init");
    }
    if (func_definition->func_type == 'c')
    {
        emit("movl $%i, %%ecx", blueprint->bp_size);
        emit("call __libsgcllc_alloc_bytes");
        emit("movq %%rax, -8(%%rbp)");
        emit_lvar_decl(e, vector_get(func_definition->local_variables, 0)); // move forward stackalloc
    }
    for (int i = 0; i < func_definition->body->statements->size; i++)
        emit_stmt(e, vector_get(func_definition->body->statements, i));
    if (func_definition->end_label)
        emit_noindent("%s:", func_definition->end_label);
    if (!strcmp(func_definition->func_name, "main"))
    {
        parser_ensure_cextern(e->p, "__libsgcllc_gc_finalize", t_void, vector_init(DEFAULT_CAPACITY, DEFAULT_ALLOC_DELTA));
        emit("call __libsgcllc_gc_finalize");
    }
    emit("addq $%i, %%rsp", func_definition->unsafe < 0 ? stackalloc : func_definition->unsafe);
    e->stackoffset = 0;
    emit("popq %%rbp");
    emit("ret");
    emit(".global %s", func_definition->func_label);
}

static bool chk_type_mismatch(datatype_t* lhs, datatype_t* rhs)
{
    if ((lhs->type == DTT_STRING && rhs->type != DTT_STRING) || (lhs->type != DTT_STRING && rhs->type == DTT_STRING))
        return true;
    return false;
}

static void emit_assign(emitter_t* e, ast_node_t* op)
{
    switch (op->lhs->type)
    {
        case AST_LVAR:
        {
            if (chk_type_mismatch(op->lhs->datatype, op->rhs->datatype))
                errore(op->loc->row, op->loc->col, "type '%i' cannot be assigned to '%i'", op->rhs->datatype->type, op->lhs->datatype->type);
            if (op->lhs->datatype->type == DTT_STRING)
                emit("movq %%rax, %i(%%rbp)", op->lhs->voffset);
            else if (isfloattype(op->lhs->datatype->type))
                emit("movs%c %%xmm0, %i(%%rbp)", floatsize(op->lhs->datatype->size), op->lhs->voffset);
            else
                emit("mov%c %%%s, %i(%%rbp)", int_reg_size(op->lhs->datatype->size), find_register(REG_A, op->lhs->datatype->size), op->lhs->voffset);
            break;
        }
        case OP_SUBSCRIPT:
        {
            if (chk_type_mismatch(op->lhs->datatype, op->rhs->datatype))
                errore(op->loc->row, op->loc->col, "type '%i' cannot be assigned to '%i'", op->rhs->datatype->type, op->lhs->datatype->type);
            if (isfloattype(op->lhs->datatype->type))
                emitter_stash_float_reg(e, find_register(REG_FLOAT, 8), op->lhs->datatype->size);
            else
                emitter_stash_int_reg(e, find_register(REG_A, op->lhs->datatype->size));
            emit_subscript(e, op->lhs, false);
            if (isfloattype(op->lhs->datatype->type))
                emit("movs%c %%%s, (%%rax)", floatsize(op->lhs->datatype->size), emitter_restore_float_reg(e, op->lhs->datatype->size));
            else
                emit("mov%c %%%s, (%%rax)", int_reg_size(op->lhs->datatype->size), emitter_restore_int_reg(e, op->lhs->datatype->size));
            break;
        }
        case OP_SELECTION:
        {
            if (chk_type_mismatch(op->lhs->datatype, op->rhs->datatype))
                errore(op->loc->row, op->loc->col, "type '%i' cannot be assigned to '%i'", op->rhs->datatype->type, op->lhs->datatype->type);
            if (isfloattype(op->lhs->datatype->type))
                emitter_stash_float_reg(e, find_register(REG_FLOAT, 8), op->lhs->datatype->size);
            else
                emitter_stash_int_reg(e, find_register(REG_A, op->lhs->datatype->size));
            emit_selection(e, op->lhs, false);
            if (isfloattype(op->lhs->datatype->type))
                emit("movs%c %%%s, (%%rax)", floatsize(op->lhs->datatype->size), emitter_restore_float_reg(e, op->lhs->datatype->size));
            else
                emit("mov%c %%%s, (%%rax)", int_reg_size(op->lhs->datatype->size), emitter_restore_int_reg(e, op->lhs->datatype->size));
            break;
        }
        default:
            errore(op->loc->row, op->loc->col, "assignment operator cannot be applied to left side of this expression");
    }
}

static void emit_lvar_decl(emitter_t* e, ast_node_t* lvar)
{
    lvar->voffset = -(e->stackoffset = round_up(e->stackoffset + lvar->datatype->size, lvar->datatype->size));
    if (lvar->vinit)
        emit_expr(e, lvar->vinit);
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

static void emit_make(emitter_t* e, ast_node_t* make)
{
    datatype_t* dt = make->datatype;
    emit("movl $%i, %%edx", dt->depth);
    datatype_t* current = dt;
    for (int i = 0; i < dt->depth; i++, current = current->array_type)
    {
        emit_expr(e, current->length);
        if (i >= 2)
            emit("mov%c %%%s, %i(%%rsp)", int_reg_size(current->length->datatype->size),
                find_register(REG_A, current->length->datatype->type), 32 + 8 * (i - 2));
        else
            emit("mov%c %%%s, %%%s", int_reg_size(current->length->datatype->size),
                find_register(REG_A, current->length->datatype->type), find_register(x64cc[i + 2], current->length->datatype->type));
    }
    emit("movl $%i, %%ecx", current->size);
    emit("call __libsgcllc_dynamic_ndim_array");
}

static void emit_binary_op(emitter_t* e, ast_node_t* lhs, ast_node_t* rhs, datatype_t* agreed_type)
{
    if (lhs->type == AST_FUNC_CALL) // bandaid patch
    {
        emit_expr(e, lhs);
        emit_conv(e, lhs->datatype, agreed_type);
        int offset = round_up(e->stackoffset + agreed_type->size, agreed_type->size);
        if (isfloattype(agreed_type->type))
            emit("movs%c %%xmm0, -%i(%%rbp)", floatsize(agreed_type->size), offset);
        else
            emit("mov%c %%%s, -%i(%%rbp)", int_reg_size(agreed_type->size), find_register(REG_A, agreed_type->size), offset);
        emit_expr(e, rhs);
        emit_conv(e, rhs->datatype, agreed_type);
        if (isfloattype(agreed_type->type))
        {
            emitter_stash_float_reg(e, find_register(REG_FLOAT, 8), agreed_type->size);
            emit("movs%c -%i(%%rbp), %%xmm0", floatsize(agreed_type->size), offset);
        }
        else
        {
            emitter_stash_int_reg(e, find_register(REG_A, agreed_type->size));
            emit("mov%c -%i(%%rbp), %%%s", int_reg_size(agreed_type->size), offset, find_register(REG_A, agreed_type->size));
        }
    }
    else
    {
        emit_expr(e, rhs);
        emit_conv(e, rhs->datatype, agreed_type);
        if (isfloattype(agreed_type->type))
            emitter_stash_float_reg(e, find_register(REG_FLOAT, 8), agreed_type->size);
        else
            emitter_stash_int_reg(e, find_register(REG_A, agreed_type->size));
        emit_expr(e, lhs);
        emit_conv(e, lhs->datatype, agreed_type);
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
        case OP_AND: case OP_ASSIGN_AND: operation = "and"; break;
        case OP_OR: case OP_ASSIGN_OR: operation = "or"; break;
        case OP_XOR: case OP_ASSIGN_XOR: operation = "xor"; break;
        default:
            errore(op->loc->row, op->loc->col, "unknown operation");
    }
    emit_binary_op(e, lhs, rhs, op->datatype);
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
            errore(op->loc->row, op->loc->col, "operator %i does not exist or cannot be applied to a floating type", op->type);
    }
    emit_binary_op(e, lhs, rhs, op->datatype);
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
    char* regD = find_register(REG_D, op->datatype->size);
    emit_binary_op(e, lhs, rhs, op->datatype);
    emit("xor%c %%%s, %%%s", int_reg_size(op->datatype->size), regD, regD);
    emit("%s%c %%%s", operation, int_reg_size(op->datatype->size), emitter_restore_int_reg(e, op->datatype->size));
    if (op->type == OP_MOD || op->type == OP_ASSIGN_MOD)
        emit("mov%c %%%s, %%%s", int_reg_size(op->datatype->size), regD, regA);
}

static void emit_mul_div(emitter_t* e, ast_node_t* op)
{
    if (!isfloattype(op->datatype->type))
        emit_int_mul_div(e, op);
    else
        emit_float_add_sub_mul_div(e, op);
}

static void emit_conditional(emitter_t* e, ast_node_t* op)
{
    ast_node_t* lhs = op->lhs, * rhs = op->rhs;
    datatype_t* agreed_type = arith_conv(lhs->datatype, rhs->datatype);
    char* operation = NULL;
    bool ftype = isfloattype(agreed_type->type);
    switch (op->type)
    {
        case OP_EQUAL: operation = "sete"; break;
        case OP_NOT_EQUAL: operation = "setne"; break;
        case OP_GREATER: operation = ftype ? "seta" : "setg"; break;
        case OP_GREATER_EQUAL: operation = ftype ? "setae" : "setge"; break;
        case OP_LESS: operation = ftype ? "setb" : "setl"; break;
        case OP_LESS_EQUAL: operation = ftype ? "setbe" : "setle"; break;
        default:
            errore(op->loc->row, op->loc->col, "unknown operation");
    }
    char* regA = find_register(REG_A, agreed_type->size);
    char* regAb = find_register(REG_A, 1);
    emit_binary_op(e, lhs, rhs, agreed_type);
    if (!ftype)
        emit("cmp%c %%%s, %%%s", int_reg_size(agreed_type->size), emitter_restore_int_reg(e, agreed_type->size), regA);
    else
        emit("comis%c %%%s, %%xmm0", floatsize(agreed_type->size), emitter_restore_float_reg(e, agreed_type->size));
    emit("%s %%%s", operation, regAb);
    emit("movzb%c %%%s, %%%s", int_reg_size(agreed_type->size), regAb, regA);
}

static void emit_logical_and(emitter_t* e, ast_node_t* op)
{
    ast_node_t* lhs = op->lhs, * rhs = op->rhs;
    char* regA = find_register(REG_A, 1);
    emit_binary_op(e, lhs, rhs, t_bool);
    char* fail = make_label(e->p, NULL);
    emit("cmpb $0, %%al");
    emit("je %s", fail);
    emit("cmpb $0, %%%s", emitter_restore_int_reg(e, 1));
    emit("je %s", fail);
    emit("movb $1, %%al");
    char* success = make_label(e->p, NULL);
    emit("jmp %s", success);
    emit_noindent("%s:", fail);
    emit("movb $0, %%al");
    emit_noindent("%s:", success);
}

static void emit_logical_or(emitter_t* e, ast_node_t* op)
{
    ast_node_t* lhs = op->lhs, * rhs = op->rhs;
    char* regA = find_register(REG_A, 1);
    emit_binary_op(e, lhs, rhs, t_bool);
    char* success = make_label(e->p, NULL);
    emit("cmpb $0, %%al");
    emit("jne %s", success);
    emit("cmpb $0, %%%s", emitter_restore_int_reg(e, 1));
    char* fail = make_label(e->p, NULL);
    emit("je %s", fail);
    emit_noindent("%s:", success);
    emit("movb $1, %%al");
    char* skip = make_label(e->p, NULL);
    emit("jmp %s", skip);
    emit_noindent("%s:", fail);
    emit("movb $0, %%al");
    emit_noindent("%s:", skip);
}

static void emit_logical_not(emitter_t* e, ast_node_t* op)
{
    emit_expr(e, op->operand);
    emit_conv(e, op->operand->datatype, t_bool);
    emit("cmpb $0, %%al");
    emit("sete %%al");
}

static void emit_minus(emitter_t* e, ast_node_t* op)
{
    if (!isfloattype(op->operand->datatype->type))
    {
        emit_expr(e, op->operand);
        emit("not%c %%%s", int_reg_size(op->datatype->size), find_register(REG_A, op->datatype->size));
    }
    else
    {
        if (!e->fp_negate_label) e->fp_negate_label = make_label(e->p, "-0.0");
        emit("movsd %s(%%rip), %%xmm0", e->fp_negate_label);
        emit_conv(e, t_f64, op->operand->datatype);
        emit("movs%c %%xmm0, %%xmm1", floatsize(op->operand->datatype->size));
        emit_expr(e, op->operand);
        emit("xorp%c %%xmm1, %%xmm0", floatsize(op->operand->datatype->size));
    }
}

static void emit_complement(emitter_t* e, ast_node_t* op)
{
    emit_expr(e, op->operand);
    emit("not%c %%%s", int_reg_size(op->datatype->size), find_register(REG_A, op->datatype->size));
}

static void emit_shift(emitter_t* e, ast_node_t* op)
{
    ast_node_t* lhs = op->lhs, * rhs = op->rhs;
    char* operation = NULL;
    switch (op->type)
    {
        case OP_SHIFT_LEFT: case OP_ASSIGN_SHIFT_LEFT: operation = "sal"; break;
        case OP_SHIFT_RIGHT: case OP_ASSIGN_SHIFT_RIGHT: operation = "sar"; break;
        case OP_SHIFT_URIGHT: case OP_ASSIGN_SHIFT_URIGHT: operation = "shr"; break;
        default:
            errore(op->loc->row, op->loc->col, "unknown operation");
    }
    char* regA = find_register(REG_A, op->datatype->size);
    emit_expr(e, rhs);
    emit_conv(e, rhs->datatype, op->datatype);
    emitter_stash_int_reg(e, "rcx");
    emit("mov%c %%%s, %%%s", int_reg_size(op->datatype->size), regA, find_register(REG_C, op->datatype->size));
    emit_expr(e, lhs);
    emit_conv(e, lhs->datatype, op->datatype);
    emit("%s%c %%cl, %%%s", operation, int_reg_size(op->datatype->size), regA);
    emit("movq %%%s, %%rcx", emitter_restore_int_reg(e, 8));
}

static void emit_subscript(emitter_t* e, ast_node_t* op, bool deref)
{
    char* regA = "rax";
    emit_expr(e, op->rhs);
    emit_conv(e, op->rhs->datatype, t_i64);
    emitter_stash_int_reg(e, regA);
    switch (op->lhs->type)
    {
        case AST_LVAR:
        {
            emit("movq %i(%%rbp), %%%s", op->lhs->voffset, regA);
            break;
        }
    }
    char* regIndex = emitter_restore_int_reg(e, 8);
    if (op->lhs->datatype->type == DTT_ARRAY)
        emit("imulq $%i, %%%s, %%%s", op->lhs->datatype->array_type->size, regIndex, regIndex);
    emit("addq %%%s, %%%s", regIndex, regA);
    if (deref)
    {
        if (!isfloattype(op->datatype->type))
            emit("mov%c (%%%s), %%%s", int_reg_size(op->datatype->size), regA, find_register(REG_A, op->datatype->size));
        else
            emit("movs%c (%%%s), %%xmm0", floatsize(op->datatype->size), regA);
    }
}

static void emit_selection(emitter_t* e, ast_node_t* op, bool deref)
{
    switch (op->lhs->type)
    {
        case AST_LVAR:
        {
            emit("movq %i(%%rbp), %%rax", op->lhs->voffset);
            break;
        }
    }
    if (op->rhs->voffset)
        emit("leaq %i(%%rax), %%rax", op->rhs->voffset);
    if (deref)
    {
        if (!isfloattype(op->datatype->type))
            emit("mov%c (%%rax), %%%s", int_reg_size(op->datatype->size), find_register(REG_A, op->datatype->size));
        else
            emit("movs%c (%%rax), %%xmm0", floatsize(op->datatype->size));
    }
}

static void emit_func_call(emitter_t* e, ast_node_t* call)
{
    for (int i = call->args->size - 1; i >= 0; i--)
    {
        ast_node_t* arg = vector_get(call->args, i);
        if (i >= 4)
            errore(call->loc->row, call->loc->col, "function calls with 4+ arguments are not supported yet");
        else
        {
            emit_expr(e, arg);
            if (arg->datatype->type == DTT_STRING || arg->datatype->type == DTT_OBJECT)
                emit("movq %%rax, %%%s", find_register(x64cc[i], 8));
            else if (isfloattype(arg->datatype->type))
            {
                if (i)
                    emit("movs%c %%xmm0, %%xmm%i", floatsize(arg->datatype->size), i);
            }
            else
                emit("mov%c %%%s, %%%s", int_reg_size(arg->datatype->size), find_register(REG_A, arg->datatype->size), find_register(x64cc[i], arg->datatype->size));
        }
    }
    emit("call %s", call->func->lowlvl_label != NULL ? call->func->lowlvl_label : call->func->func_label);
}

static void emit_if_statement(emitter_t* e, ast_node_t* stmt)
{
    emit_expr(e, stmt->if_cond);
    emit("cmp%c $0, %%%s", int_reg_size(stmt->if_cond->datatype->size), find_register(REG_A, stmt->if_cond->datatype->size));
    char* skip = make_label(e->p, NULL);
    emit("je %s", skip);
    for (int i = 0; i < stmt->if_then->statements->size; i++)
        emit_stmt(e, vector_get(stmt->if_then->statements, i));
    bool els_exists = stmt->if_els->statements->size;
    if (els_exists)
    {
        char* skip_els = make_label(e->p, NULL);
        emit("jmp %s", skip_els);
        emit_noindent("%s:", skip);
        for (int i = 0; i < stmt->if_els->statements->size; i++)
            emit_stmt(e, vector_get(stmt->if_els->statements, i));
        emit_noindent("%s:", skip_els);
    }
    else
        emit_noindent("%s:", skip);
}

static void emit_while_statement(emitter_t* e, ast_node_t* stmt)
{
    char* check_cond = make_label(e->p, NULL);
    emit("jmp %s", check_cond);
    char* loop = make_label(e->p, NULL);
    emit_noindent("%s:", loop);
    for (int i = 0; i < stmt->while_then->statements->size; i++)
        emit_stmt(e, vector_get(stmt->while_then->statements, i));
    emit_noindent("%s:", check_cond);
    emit_expr(e, stmt->while_cond);
    emit("cmp%c $0, %%%s", int_reg_size(stmt->while_cond->datatype->size), find_register(REG_A, stmt->while_cond->datatype->type));
    emit("jne %s", loop);
}

static void emit_for_statement(emitter_t* e, ast_node_t* stmt)
{
    emit_stmt(e, stmt->for_init);
    char* check_cond = make_label(e->p, NULL);
    emit("jmp %s", check_cond);
    char* loop = make_label(e->p, NULL);
    emit_noindent("%s:", loop);
    for (int i = 0; i < stmt->for_then->statements->size; i++)
        emit_stmt(e, vector_get(stmt->for_then->statements, i));
    emit_expr(e, stmt->for_post);
    emit_noindent("%s:", check_cond);
    emit_expr(e, stmt->for_cond);
    emit("cmp%c $0, %%%s", int_reg_size(stmt->for_cond->datatype->size), find_register(REG_A, stmt->for_cond->datatype->type));
    emit("jne %s", loop);
}

static void emit_switch_statement(emitter_t* e, ast_node_t* stmt)
{
    ast_node_t* cmp = stmt->cmp;
    int default_case_index = -1;
    for (int i = 0; i < stmt->cases->size; i++)
    {
        ast_node_t* case_stmt = vector_get(stmt->cases, i);
        if (!case_stmt->case_conditions->size)
        {
            default_case_index = i;
            continue;
        }
        for (int j = 0; j < case_stmt->case_conditions->size; j++)
        {
            ast_node_t* case_cond = vector_get(case_stmt->case_conditions, j);
            datatype_t* agreed_type = arith_conv(cmp->datatype, case_cond->datatype);
            bool ftype = isfloattype(agreed_type->type);
            char* regA = find_register(REG_A, agreed_type->size);
            emit_expr(e, cmp);
            emit_conv(e, cmp->datatype, agreed_type);
            if (!ftype)
                emitter_stash_int_reg(e, regA); // rhs stashed
            else
                emitter_stash_float_reg(e, "xmm0", agreed_type->size);
            emit_expr(e, case_cond);
            emit_conv(e, case_cond->datatype, agreed_type);
            if (!ftype)
                emit("cmp%c %%%s, %%%s", int_reg_size(agreed_type->size), emitter_restore_int_reg(e, agreed_type->size), regA);
            else
                emit("comis%c %%%s, %%xmm0", floatsize(agreed_type->size), emitter_restore_float_reg(e, agreed_type->size));
            emit("je %s", case_stmt->case_label);
        }
    }
    char* end_label = make_label(e->p, NULL);
    if (default_case_index != -1)
    {
        ast_node_t* case_stmt = vector_get(stmt->cases, default_case_index);
        emit("jmp %s", case_stmt->case_label);
    }
    else
        emit("jmp %s", end_label);
    for (int i = 0; i < stmt->cases->size; i++)
    {
        ast_node_t* case_stmt = vector_get(stmt->cases, i);
        emit_noindent("%s:", case_stmt->case_label);
        for (int j = 0; j < case_stmt->case_then->statements->size; j++)
            emit_stmt(e, vector_get(case_stmt->case_then->statements, j));
        emit("jmp %s", end_label);
    }
    emit_noindent("%s:", end_label);
}

static void emit_delete_statement(emitter_t* e, ast_node_t* stmt)
{
    switch (stmt->delsym->type)
    {
        case AST_LVAR:
        {
            if (stmt->delsym->datatype->type == DTT_ARRAY)
            {
                emit("movq %i(%%rbp), %%rcx", stmt->delsym->voffset);
                emit("movl $%i, %%edx", stmt->delsym->datatype->depth);
                datatype_t* current = stmt->delsym->datatype;
                for (int i = 0; i < stmt->delsym->datatype->depth; i++, current = current->array_type)
                {
                    emit_expr(e, current->length);
                    if (i >= 2)
                        emit("mov%c %%%s, %i(%%rsp)", int_reg_size(current->length->datatype->size),
                            find_register(REG_A, current->length->datatype->type), 32 + 8 * (i - 2));
                    else
                        emit("mov%c %%%s, %%%s", int_reg_size(current->length->datatype->size),
                            find_register(REG_A, current->length->datatype->type), find_register(x64cc[i + 2], current->length->datatype->type));
                }
                emit("call __libsgcllc_delete_array");
            }
            else
                errore(stmt->loc->row, stmt->loc->col, "delete operator cannot be applied here");
            break;
        }
        default:
        {
            errore(stmt->loc->row, stmt->loc->col, "delete operator cannot be applied here");
            break;
        }
    }
}

static void emit_ternary(emitter_t* e, ast_node_t* expr)
{
    emit_expr(e, expr->tern_cond);
    emit("cmp%c $0, %%%s", int_reg_size(expr->tern_cond->datatype->size), find_register(REG_A, expr->tern_cond->datatype->size));
    char* skip = make_label(e->p, NULL);
    emit("je %s", skip);
    emit_expr(e, expr->tern_then);
    emit_conv(e, expr->tern_then->datatype, expr->datatype);
    char* skip_els = make_label(e->p, NULL);
    emit("jmp %s", skip_els);
    emit_noindent("%s:", skip);
    emit_expr(e, expr->tern_els);
    emit_conv(e, expr->tern_els->datatype, expr->datatype);
    emit_noindent("%s:", skip_els);
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
        case OP_AND:
        case OP_OR:
        case OP_XOR:
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
        case OP_ASSIGN_AND:
        case OP_ASSIGN_OR:
        case OP_ASSIGN_XOR:
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
        case OP_EQUAL:
        case OP_NOT_EQUAL:
        case OP_GREATER:
        case OP_GREATER_EQUAL:
        case OP_LESS:
        case OP_LESS_EQUAL:
        {
            emit_conditional(e, expr);
            break;
        }
        case OP_LOGICAL_AND:
        {
            emit_logical_and(e, expr);
            break;
        }
        case OP_LOGICAL_OR:
        {
            emit_logical_or(e, expr);
            break;
        }
        case OP_NOT:
        {
            emit_logical_not(e, expr);
            break;
        }
        case OP_MINUS:
        {
            emit_minus(e, expr);
            break;
        }
        case OP_COMPLEMENT:
        {
            emit_complement(e, expr);
            break;
        }
        case OP_ASM:
        {
            emit("%s /* inline assembly (line %i, row %i) */", unwrap_string_literal(expr->operand->svalue), expr->loc->row, expr->loc->col);
            break;
        }
        case OP_PREFIX_INCREMENT:
        case OP_PREFIX_DECREMENT:
        {
            ast_node_t* operation = ast_binary_op_init(expr->type == OP_PREFIX_INCREMENT ? OP_ASSIGN_ADD : OP_ASSIGN_SUB,
                expr->operand->datatype, expr->loc, expr->operand, ast_iliteral_init(t_i64, expr->loc, 1LL));
            emit_expr(e, operation);
            // delete?
            break;
        }
        case OP_POSTFIX_INCREMENT:
        case OP_POSTFIX_DECREMENT:
        {
            emit_expr(e, expr->operand);
            char* regA = find_register(REG_A, expr->datatype->size);
            if (isfloattype(expr->datatype->type))
                emitter_stash_float_reg(e, "xmm0", expr->datatype->size);
            else
                emitter_stash_int_reg(e, regA);
            ast_node_t* operation = ast_binary_op_init(expr->type == OP_POSTFIX_INCREMENT ? OP_ASSIGN_ADD : OP_ASSIGN_SUB,
                expr->operand->datatype, expr->loc, expr->operand, ast_iliteral_init(t_i64, expr->loc, 1LL));
            emit_expr(e, operation);
            if (isfloattype(expr->datatype->type))
                emit("movs%c %%%s, %%xmm0", floatsize(expr->datatype->size), emitter_restore_float_reg(e, expr->datatype->size));
            else
                emit("mov%c %%%s, %%%s", int_reg_size(expr->datatype->size), emitter_restore_int_reg(e, expr->datatype->size), regA);
            // delete?
            break;
        }
        case OP_SHIFT_LEFT:
        case OP_SHIFT_RIGHT:
        case OP_SHIFT_URIGHT:
        {
            emit_shift(e, expr);
            break;
        }
        case OP_ASSIGN_SHIFT_LEFT:
        case OP_ASSIGN_SHIFT_RIGHT:
        case OP_ASSIGN_SHIFT_URIGHT:
        {
            emit_shift(e, expr);
            if (expr->lhs->datatype != expr->rhs->datatype)
                emit_conv(e, expr->rhs->datatype, expr->lhs->datatype);
            emit_assign(e, expr);
            break;
        }
        case OP_SUBSCRIPT:
        {
            emit_subscript(e, expr, true);
            break;
        }
        case OP_SELECTION:
        {
            emit_selection(e, expr, true);
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
        case AST_SLITERAL:
        {
            emit("leaq %s(%%rip), %%rax", expr->slabel);
            break;
        }
        case AST_LVAR:
        {
            if (isfloattype(expr->datatype->type))
                emit("movs%c %i(%%rbp), %%xmm0", floatsize(expr->datatype->size), expr->voffset);
            else
                emit("mov%c %i(%%rbp), %%%s", int_reg_size(expr->datatype->size), expr->voffset, find_register(REG_A, expr->datatype->size));
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
        case AST_MAKE:
        {
            emit_make(e, expr);
            break;
        }
        case AST_TERNARY:
        {
            emit_ternary(e, expr);
            break;
        }
        case OP_MAGNITUDE:
        {
            emit_expr(e, expr->operand);
            emit("mov %%rax, %%rcx");
            switch (expr->operand->datatype->type)
            {
                case DTT_ARRAY:
                    emit("call __libsgcllc_array_size");
                    break;
                case DTT_STRING:
                    emit("call __libsgcllc_string_length");
                    break;
                case DTT_OBJECT:
                    emit("call __libsgcllc_blueprint_size");
                    break;
                default:
                    errore(expr->loc->row, expr->loc->col, "magnitude operator cannot be applied to this expression");
            }
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
            char* end_label = stmt->retfunc->end_label ? stmt->retfunc->end_label : (stmt->retfunc->end_label = make_label(e->p, NULL));
            emit("jmp %s", end_label);
            break;
        }
        case AST_IF:
        {
            emit_if_statement(e, stmt);
            break;
        }
        case AST_WHILE:
        {
            emit_while_statement(e, stmt);
            break;
        }
        case AST_FOR:
        {
            emit_for_statement(e, stmt);
            break;
        }
        case AST_DELETE:
        {
            emit_delete_statement(e, stmt);
            break;
        }
        case AST_SWITCH:
        {
            emit_switch_statement(e, stmt);
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