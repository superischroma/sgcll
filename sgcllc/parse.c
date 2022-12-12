#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "sgcllc.h"

#define LOWEST_PRECEDENCE 4
    
static ast_node_t* ast_get_by_token(parser_t* p, token_t* token);
int parser_get_datatype_type(parser_t* p);
static bool parser_is_func_definition(parser_t* p);
static bool parser_is_var_decl(parser_t* p);
static datatype_t* parser_build_datatype(parser_t* p);
static ast_node_t* parser_read_import(parser_t* p);
static ast_node_t* parser_read_func_definition(parser_t* p);
static void parser_read_func_body(parser_t* p, ast_node_t* func_node);
static ast_node_t* parser_read_decl(parser_t* p);
static ast_node_t* parser_read_lvar_decl(parser_t* p);
static ast_node_t* parser_read_stmt(parser_t* p);
static ast_node_t* parser_read_expr(parser_t* p);
static void parser_define_builtins(parser_t* p);

datatype_t* t_void = &(datatype_t){ VT_PUBLIC, DTT_VOID, 0, false };
datatype_t* t_bool = &(datatype_t){ VT_PUBLIC, DTT_BOOL, 1, true };
datatype_t* t_i8 = &(datatype_t){ VT_PUBLIC, DTT_I8, 1, false };
datatype_t* t_i16 = &(datatype_t){ VT_PUBLIC, DTT_I16, 2, false };
datatype_t* t_i32 = &(datatype_t){ VT_PUBLIC, DTT_I32, 4, false };
datatype_t* t_i64 = &(datatype_t){ VT_PUBLIC, DTT_I64, 8, false };
datatype_t* t_ui8 = &(datatype_t){ VT_PUBLIC, DTT_I8, 1, true };
datatype_t* t_ui16 = &(datatype_t){ VT_PUBLIC, DTT_I16, 2, true };
datatype_t* t_ui32 = &(datatype_t){ VT_PUBLIC, DTT_I32, 4, true };
datatype_t* t_ui64 = &(datatype_t){ VT_PUBLIC, DTT_I64, 8, true };
datatype_t* t_f32 = &(datatype_t){ VT_PUBLIC, DTT_F32, 4, false };
datatype_t* t_f64 = &(datatype_t){ VT_PUBLIC, DTT_F64, 8, false };
datatype_t* t_string = NULL;

datatype_t* get_t_string(void)
{
    if (t_string) return t_string;
    return t_string = &(datatype_t){ VT_PUBLIC, DTT_STRING, 8, false, t_i8 };
}

int precedence(int op)
{
    switch (op)
    {
        case KW_COMMA: return LOWEST_PRECEDENCE;
        case OP_ASSIGN:
            return LOWEST_PRECEDENCE - 1;
        case OP_ADD:
        case OP_SUB:
            return LOWEST_PRECEDENCE - 2;
        case OP_MUL:
        case OP_DIV:
        case OP_MOD:
            return LOWEST_PRECEDENCE - 3;
    }
    return LOWEST_PRECEDENCE + 1;
}

static datatype_t* arith_conv(datatype_t* t1, datatype_t* t2)
{
    if (t2->size > t1->size)
    {
        datatype_t* tmp = t1;
        t1 = t2;
        t2 = tmp;
    }
    if (isfloattype(t1->type))
        return t1;
    if (t1->size > t2->size)
        return t1;
    if (t1->usign == t2->usign)
        return t1;
    datatype_t* nt = calloc(1, sizeof(datatype_t));
    *nt = *t1;
    nt->usign = true;
    return nt;
}

parser_t* parser_init(lexer_t* lex)
{
    parser_t* p = calloc(1, sizeof(parser_t));
    location_t* loc = calloc(1, sizeof(location_t));
    loc->row = loc->col = loc->offset = 0;
    p->nfile = ast_file_init(loc);
    p->genv = map_init(NULL, 50);
    p->lenv = NULL;
    p->lex = lex;
    p->oindex = 0;
    p->externs = vector_init(5, 5);
    parser_define_builtins(p);
    return p;
}

static void parser_define_builtins(parser_t* p)
{
    vector_push(p->externs, map_put(p->genv, "__builtin_i32_println", ast_builtin_init(t_void, "__builtin_i32_println",
        vector_qinit(1, ast_lvar_init(t_i32, NULL, "i")))));
}

static ast_node_t* ast_get_by_token(parser_t* p, token_t* token)
{
    switch (token->type)
    {
        case TT_IDENTIFIER:
        {
            ast_node_t* ident = map_get(p->lenv ? p->lenv : p->genv, token->content);
            if (!ident)
                errorp(token->loc->row, token->loc->col, "symbol not defined: %s", token->content);
            return ident;
        }
        case TT_CHAR_LITERAL:
            return ast_iliteral_init(t_i8, token->loc, atoll(token->content));
        case TT_NUMBER_LITERAL:
            return ast_iliteral_init(t_i32, token->loc, atoll(token->content));
        default:
            errorp(token->loc->row, token->loc->col, "unknown token");
    }
}

static token_t* parser_get(parser_t* p)
{
    if (p->oindex >= p->lex->output->size)
        return NULL;
    return lex_get(p->lex, p->oindex++);
}

static void parser_unget(parser_t* p)
{
    p->oindex--;
}

static token_t* parser_peek(parser_t* p)
{
    token_t* token = parser_get(p);
    parser_unget(p);
    return token;
}

static token_t* parser_far_peek(parser_t* p, int distance)
{
    if (p->oindex + distance - 1 >= p->lex->output->size)
        return NULL;
    return lex_get(p->lex, p->oindex + distance - 1);
}

static token_t* parser_expect(parser_t* p, int id)
{
    token_t* token = parser_get(p);
    if (token == NULL)
        errorp(0, 0, "expected %c, got end of file", id);
    if (token->id != id)
        errorp(token->loc->row, token->loc->col, "expected \"%c\", got \"%c\"", id, token->id);
    return token;
}

static token_t* parser_expect_type(parser_t* p, token_type tt)
{
    token_t* token = parser_get(p);
    if (token == NULL)
        errorp(0, 0, "expected type %i, got end of file", tt);
    if (token->type != tt)
        errorp(token->loc->row, token->loc->col, "expected type %i, got type %i", tt, token->type);
    return token;
}

static bool parser_check(parser_t* p, int id)
{
    token_t* token = parser_peek(p);
    if (token == NULL || token->id != id)
        return false;
    return true;
}

static bool parser_check_content(parser_t* p, const char* content)
{
    token_t* token = parser_peek(p);
    if (token == NULL || !token_has_content(token) || strcmp(token->content, content))
        return false;
    return true;
}

static token_t* parser_expect_content(parser_t* p, const char* content)
{
    token_t* token = parser_get(p);
    if (token == NULL)
        errorp(0, 0, "expected \"%s\", got end of file", content);
    if (!token_has_content(token))
        errorp(token->loc->row, token->loc->col, "expected \"%s\", got \"%c\"", content, token->id);
    if (strcmp(token->content, content))
        errorp(token->loc->row, token->loc->col, "expected \"%s\", got \"%s\"", content, token->content);
    return token;
}

bool parser_eof(parser_t* p)
{
    return p->oindex >= p->lex->output->size;
}

int parser_get_datatype_type(parser_t* p)
{
    token_t* token = parser_peek(p);
    if (token == NULL)
        return -2;
    if (token_has_content(token)) // objects...?
        return -1;
    bool found = false;
    switch (token->id)
    {
        #define keyword(id, _, settings) \
        case id: \
        { \
            if (settings & 1) \
                found = true; \
            break; \
        }
        #include "keywords.inc"
        #undef keyword
    }
    if (found) return token->id - KW_VOID;
    return -1;
}

static bool parser_is_func_definition(parser_t* p)
{
    int i = 1;
    for (;; i++)
    {
        token_t* token = parser_far_peek(p, i);
        if (token == NULL)
            return false;
        if (token->type == TT_KEYWORD && token->id == KW_LPAREN)
            break;
        if (token->type != TT_IDENTIFIER && token->type != TT_KEYWORD)
            return false;
    }
    for (i++;; i++)
    {
        token_t* token = parser_far_peek(p, i);
        if (token == NULL)
            return false;
        if (token->type == TT_KEYWORD && token->id == KW_RPAREN)
            break;
    }
    token_t* lbrace = parser_far_peek(p, ++i);
    if (lbrace->type != TT_KEYWORD)
        return false;
    if (lbrace->id != KW_LBRACE)
        return false;
    return true;
}

static bool parser_is_var_decl(parser_t* p)
{
    int i = 1;
    for (;; i++)
    {
        token_t* token = parser_far_peek(p, i);
        if (token == NULL)
            return false;
        if (token->type == TT_KEYWORD)
            continue;
        if (token->type == TT_IDENTIFIER)
        {
            if (map_get_local(p->lenv ? p->lenv : p->genv, token->content))
                return false;
            break;
        }
        return false;
    }
    token_t* equal_or_semicolon = parser_far_peek(p, ++i);
    if (equal_or_semicolon == NULL)
        return false;
    if (equal_or_semicolon->id != OP_ASSIGN && equal_or_semicolon->id != KW_SEMICOLON)
        return false;
    return true;
}

static datatype_t* parser_build_datatype(parser_t* p)
{
    datatype_t* dt = calloc(1, sizeof(datatype_t));
    dt->visibility = VT_PRIVATE;
    dt->usign = false;
    dt->type = DTT_I32;
    dt->size = 4;
    for (;; parser_get(p))
    {
        token_t* token = parser_peek(p);
        if (token != NULL && token->type == TT_IDENTIFIER)
            break;
        datatype_type dtt = parser_get_datatype_type(p);
        if (dtt == -2)
            errorp(0, 0, "reached end of file while parsing datatype");
        if (dtt != -1)
        {
            dt->type = dtt;
            switch (dt->type)
            {
                case DTT_VOID: dt->size = 0; break;
                case DTT_I8:
                case DTT_BOOL:
                    dt->size = 1;
                    break;
                case DTT_I16: dt->size = 2; break;
                case DTT_I32:
                case DTT_F32:
                    dt->size = 4;
                    break;
                default:
                    dt->size = 8;
                    break;
            }
            continue;
        }
        switch (parser_peek(p)->id)
        {
            case KW_PRIVATE:
            case KW_PUBLIC:
            case KW_PROTECTED:
                dt->visibility = parser_peek(p)->id - VT_PRIVATE;
                break;
            case KW_UNSIGNED:
                dt->usign = true;
                break;
        }
    }
    return dt;
}

static ast_node_t* parser_read_import(parser_t* p)
{
    parser_get(p); // skip import keyword
    token_t* path = parser_expect_type(p, TT_STRING_LITERAL);
    ast_node_t* node = ast_import_init(path->loc, unwrap_string_literal(path->content));
    parser_expect(p, ';');
    return node;
}

static ast_node_t* parser_read_func_definition(parser_t* p)
{
    datatype_t* dt = parser_build_datatype(p);
    token_t* func_name_token = parser_expect_type(p, TT_IDENTIFIER);
    ast_node_t* func_node = map_put(p->lenv ? p->lenv : p->genv, func_name_token->content, ast_func_definition_init(dt, func_name_token->loc, func_name_token->content));
    parser_expect(p, '(');
    map_t* func_env = p->lenv = map_init(p->lenv ? p->lenv : p->genv, 100);
    for (;;)
    {
        if (parser_check(p, ')'))
        {
            parser_get(p);
            break;
        }
        if (parser_check(p, ','))
            parser_get(p);
        datatype_t* pdt = parser_build_datatype(p);
        token_t* param_name_token = parser_expect_type(p, TT_IDENTIFIER);
        if (!parser_check(p, ')') && !parser_check(p, ','))
            parser_expect(p, ')');
        ast_node_t* lvar = map_put(p->lenv, param_name_token->content, ast_lvar_init(pdt, param_name_token->loc, param_name_token->content));
        vector_push(func_node->params, lvar);
    }
    parser_expect(p, '{');
    parser_read_func_body(p, func_node);
    p->lenv = p->lenv->parent;
    if (p->lenv == p->genv)
        p->lenv = NULL;
    map_delete(func_env);
    return func_node;
}

static void parser_read_func_body(parser_t* p, ast_node_t* func_node)
{
    for (;;)
    {
        if (parser_check(p, '}'))
        {
            parser_get(p);
            break;
        }
        ast_node_t* stmt = parser_read_stmt(p);
        if (stmt->type == AST_LVAR)
            vector_push(func_node->local_variables, stmt);
        vector_push(func_node->body->statements, stmt);
    }
}

static ast_node_t* parser_read_decl(parser_t* p)
{
    if (parser_is_func_definition(p))
        return parser_read_func_definition(p);
    return NULL;
}

static ast_node_t* parser_read_lvar_decl(parser_t* p)
{
    datatype_t* dt = parser_build_datatype(p);
    token_t* var_name_token = parser_expect_type(p, TT_IDENTIFIER);
    ast_node_t* var_node = map_put(p->lenv ? p->lenv : p->genv, var_name_token->content, ast_lvar_init(dt, var_name_token->loc, var_name_token->content));
    if (parser_check(p, OP_ASSIGN))
    {
        parser_unget(p);
        return var_node;
    }
    parser_expect(p, KW_SEMICOLON);
    return var_node;
}

static ast_node_t* parser_read_stmt(parser_t* p)
{
    if (parser_is_var_decl(p))
        return parser_read_lvar_decl(p);
    return parser_read_expr(p);
}

static ast_node_t* parser_read_expr(parser_t* p)
{
    vector_t* stack = vector_init(20, 10);
    vector_t* expr_result = vector_init(20, 10);
    vector_t* calls = vector_init(5, 5);
    for (;;)
    {
        token_t* token = parser_get(p);
        if (token == NULL)
            errorp(0, 0, "unexpected end of file");
        printf("token: %c\n", token->id);
        if (token->id == ';')
            break;
        if (token->type == TT_CHAR_LITERAL || token->type == TT_STRING_LITERAL || token->type == TT_NUMBER_LITERAL)
            vector_push(expr_result, token);
        else if (token->type == TT_IDENTIFIER)
        {
            ast_node_t* node = ast_get_by_token(p, token);
            if (node->type != AST_FUNC_DEFINITION)
                vector_push(expr_result, token);
        }
        else if (token->id == '(')
            vector_push(stack, token);
        else if (token->id == ')' || token->id == ',')
        {
            while (vector_top(stack) != NULL && ((token_t*) vector_top(stack))->id != '(')
                vector_push(expr_result, vector_pop(stack));
            if (token->id == ')')
            {
                vector_pop(stack);
                token_t* call = vector_pop(calls);
                if (call) vector_push(expr_result, call);
            }
        }
        else
        {
            while (vector_top(stack) != NULL && precedence(token->id) > precedence(((token_t*) vector_top(stack))->id))
                vector_push(expr_result, vector_pop(stack));
            vector_push(stack, token);
        }
        token_t* peek = parser_peek(p);
        if (peek != NULL && peek->id == KW_LPAREN)
        {
            if (token->type == TT_IDENTIFIER)
            {
                ast_node_t* node = ast_get_by_token(p, token);
                if (node->type == AST_FUNC_DEFINITION)
                    vector_push(calls, token);
                else
                    vector_push(calls, NULL);
            }
            else
                vector_push(calls, NULL);
        }
    }
    while (vector_top(stack) != NULL)
        vector_push(expr_result, vector_pop(stack));
    vector_clear(stack, RETAIN_OLD_CAPACITY);
    vector_delete(calls);
    if (!expr_result->size)
        errorp(parser_peek(p)->loc->row, parser_peek(p)->loc->col, "null statement is not allowed");
    for (int i = 0; i < expr_result->size; i++)
    {
        token_t* token = (token_t*) vector_get(expr_result, i);
        if (token_has_content(token))
            printf("result[%i] = %s\n", i, token->content);
        else
            printf("result[%i] = %c\n", i, token->id);
    }
    for (int i = 0; i < expr_result->size; i++)
    {
        token_t* token = (token_t*) vector_get(expr_result, i);
        if (token->type == TT_CHAR_LITERAL || token->type == TT_STRING_LITERAL || token->type == TT_NUMBER_LITERAL)
            vector_push(stack, ast_get_by_token(p, token));
        if (token->type == TT_IDENTIFIER)
        {
            ast_node_t* node = ast_get_by_token(p, token);
            if (node->type != AST_FUNC_DEFINITION)
            {
                vector_push(stack, node);
                continue;
            }
            vector_t* args = vector_init(5, 5);
            for (int i = 0; i < node->params->size; i++)
            {
                ast_node_t* arg = vector_pop(stack);
                if (!arg)
                    errorp(token->loc->row, token->loc->col, "function %s expected %i parameters, got %i", node->func_name, node->params->size, i);
                vector_push(args, arg);
            }
            vector_push(stack, ast_func_call_init(node->datatype, token->loc, node, args));
            break;
        }
        if (token->type == TT_KEYWORD)
        {
            switch (token->id)
            {
                case OP_ASSIGN:
                case OP_ADD:
                case OP_SUB:
                case OP_MUL:
                case OP_DIV:
                case OP_MOD:
                {
                    ast_node_t* rhs = vector_pop(stack);
                    ast_node_t* lhs = vector_pop(stack);
                    if (!lhs || !rhs)
                        errorp(token->loc->row, token->loc->col, "expected 2 operands for operator %i", token->id);
                    vector_push(stack, ast_binary_op_init(token->id, arith_conv(lhs->datatype, rhs->datatype), token->loc, lhs, rhs));
                    break;
                }
            }
        }
    }
    if (!stack->size)
        errorp(0, 0, "dev error: you messed up with the stack goofball");
    ast_node_t* top = (ast_node_t*) vector_top(stack);
    vector_delete(stack);
    vector_delete(expr_result);
    return top;
}

token_t* parser_read(parser_t* p)
{
    token_t* next = parser_peek(p);
    if (next == NULL)
        return NULL;
    if (parser_check(p, KW_IMPORT))
        return vector_push(p->nfile->imports, parser_read_import(p));
    ast_node_t* node = parser_read_decl(p);
    if (node) return vector_push(p->nfile->decls, node);
    ast_print(p->nfile);
    if (token_has_content(next))
        errorp(next->loc->row, next->loc->col, "encountered unknown token: %s", next->content);
    else
        errorp(next->loc->row, next->loc->col, "encountered unknown token: %i", next->id);
    return NULL;
}

void parser_delete(parser_t* p)
{
    if (!p) return;
    map_delete(p->genv);
    map_delete(p->lenv);
    vector_delete(p->externs);
    free(p);
}