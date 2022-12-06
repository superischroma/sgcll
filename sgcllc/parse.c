#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "sgcllc.h"
    
int parser_get_datatype_type(parser_t* p);
static bool parser_is_func_definition(parser_t* p);
static bool parser_is_var_decl(parser_t* p);
static datatype_t* parser_build_datatype(parser_t* p);
static ast_node_t* parser_read_import(parser_t* p);
static ast_node_t* parser_read_func_definition(parser_t* p);
static void parser_read_func_body(parser_t* p, ast_node_t* func_node);
static ast_node_t* parser_read_decl(parser_t* p);
static ast_node_t* parser_read_stmt(parser_t* p);

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

parser_t* parser_init(lexer_t* lex)
{
    parser_t* p = calloc(1, sizeof(parser_t));
    location_t* loc = calloc(1, sizeof(location_t));
    loc->row = loc->col = loc->offset = 0;
    p->file = ast_file_init(loc);
    p->genv = map_init(NULL, 50);
    p->lenv = NULL;
    p->lex = lex;
    p->oindex = 0;
    return p;
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
        if (token->type == TT_KEYWORD && token->id == P_LPARENTHESIS)
            break;
        if (token->type != TT_IDENTIFIER && token->type != TT_KEYWORD)
            return false;
    }
    for (i++;; i++)
    {
        token_t* token = parser_far_peek(p, i);
        if (token == NULL)
            return false;
        if (token->type == TT_KEYWORD && token->id == P_RPARENTHESIS)
            break;
    }
    token_t* lbrace = parser_far_peek(p, ++i);
    if (lbrace->type != TT_KEYWORD)
        return false;
    if (lbrace->id != P_LBRACE)
        return false;
    return true;
}

static bool parser_is_var_decl(parser_t* p)
{
    return false;
}

static datatype_t* parser_build_datatype(parser_t* p)
{
    datatype_t* dt = calloc(1, sizeof(datatype_t));
    dt->visibility = VT_PRIVATE;
    dt->usign = false;
    dt->type = DTT_I32;
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
    //parser_read_func_body(p, func_node);
    p->lenv = p->lenv->parent;
    map_delete(func_env);
    return func_node;
}

static void parser_read_func_body(parser_t* p, ast_node_t* func_node)
{
    for (;;)
    {
        if (parser_check(p, '}'))
            break;
        vector_push(func_node->body->statements, parser_read_stmt(p));
    }
}

static ast_node_t* parser_read_decl(parser_t* p)
{
    if (parser_is_func_definition(p))
        return parser_read_func_definition(p);
    return NULL;
}

static ast_node_t* parser_read_stmt(parser_t* p)
{
    if (parser_is_var_decl(p))
        {}
}

token_t* parser_read(parser_t* p)
{
    token_t* next = parser_peek(p);
    if (next == NULL)
        return NULL;
    if (parser_check(p, KW_IMPORT))
        return vector_push(p->file->imports, parser_read_import(p));
    ast_node_t* node = parser_read_decl(p);
    if (node) return vector_push(p->file->decls, node);
    ast_print(p->file);
    if (token_has_content(next))
        errorp(next->loc->row, next->loc->col, "encountered unknown token: %s", next->content);
    else
        errorp(next->loc->row, next->loc->col, "encountered unknown token: %i", next->id);
    return NULL;
}