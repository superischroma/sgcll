#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "sgcllc.h"

#define bassert(condition) \
    if (!(condition)) \
        return false

datatype_t* t_void = &(datatype_t){ VT_PUBLIC, DT_VOID, 0, false };
datatype_t* t_bool = &(datatype_t){ VT_PUBLIC, DT_BOOL, 1, true };
datatype_t* t_i8 = &(datatype_t){ VT_PUBLIC, DT_I8, 1, false };
datatype_t* t_i16 = &(datatype_t){ VT_PUBLIC, DT_I16, 2, false };
datatype_t* t_i32 = &(datatype_t){ VT_PUBLIC, DT_I32, 4, false };
datatype_t* t_i64 = &(datatype_t){ VT_PUBLIC, DT_I64, 8, false };
datatype_t* t_ui8 = &(datatype_t){ VT_PUBLIC, DT_UI8, 1, true };
datatype_t* t_ui16 = &(datatype_t){ VT_PUBLIC, DT_UI16, 2, true };
datatype_t* t_ui32 = &(datatype_t){ VT_PUBLIC, DT_UI32, 4, true };
datatype_t* t_ui64 = &(datatype_t){ VT_PUBLIC, DT_UI64, 8, true };
datatype_t* t_f32 = &(datatype_t){ VT_PUBLIC, DT_F32, 4, false };
datatype_t* t_f64 = &(datatype_t){ VT_PUBLIC, DT_F64, 8, false };

static void parser_init_primitives(parser_t* p)
{
    map_put(p->genv, "void", t_void);
    map_put(p->genv, "bool", t_bool);
    map_put(p->genv, "i8", t_i8);
    map_put(p->genv, "i16", t_i16);
    map_put(p->genv, "i32", t_i32);
    map_put(p->genv, "i64", t_i64);
    map_put(p->genv, "ui8", t_ui8);
    map_put(p->genv, "ui16", t_ui16);
    map_put(p->genv, "ui32", t_ui32);
    map_put(p->genv, "ui64", t_ui64);
    map_put(p->genv, "f32", t_f32);
    map_put(p->genv, "f64", t_f64);
}

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
    parser_init_primitives(p);
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

static bool parser_expect(parser_t* p, int id)
{
    token_t* token = parser_peek(p);
    if (token == NULL)
        errorp(0, 0, "expected %c, got end of file", id);
    if (token->id != id)
        errorp(token->loc->row, token->loc->col, "expected \"%c\", got \"%c\"", id, token->id);
    return true;
}

static bool parser_expect_slit(parser_t* p)
{
    token_t* token = parser_peek(p);
    if (token == NULL)
        errorp(0, 0, "expected string literal, got end of file");
    if (token->type != TT_STRING_LITERAL)
        errorp(token->loc->row, token->loc->col, "expected string literal, got type %c", token->type);
    return true;
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

static bool parser_expect_content(parser_t* p, const char* content)
{
    token_t* token = parser_peek(p);
    if (token == NULL)
        errorp(0, 0, "expected \"%s\", got end of file", content);
    if (!token_has_content(token))
        errorp(token->loc->row, token->loc->col, "expected \"%s\", got \"%c\"", content, token->id);
    if (strcmp(token->content, content))
        errorp(token->loc->row, token->loc->col, "expected \"%s\", got \"%s\"", content, token->content);
    return true;
}

bool parser_eof(parser_t* p)
{
    return p->oindex >= p->lex->output->size;
}

bool parser_is_func_definition(parser_t* p)
{
    int i = 1;
    for (;; i++)
    {
        token_t* token = parser_far_peek(p, i);
        if (token == NULL)
            return false;
        if (token->type == TT_PUNCTUATOR && token->id == P_LPARENTHESIS)
            break;
        if (token->type != TT_IDENTIFIER)
            return false;
    }
    for (i++;; i++)
    {
        token_t* token = parser_far_peek(p, i);
        if (token == NULL)
            return false;
        if (token->type == TT_PUNCTUATOR && token->id == P_RPARENTHESIS)
            break;
    }
    token_t* lbrace = parser_far_peek(p, ++i);
    if (lbrace->type != TT_PUNCTUATOR)
        return false;
    if (lbrace->id != P_LBRACE)
        return false;
    return true;
}

static ast_node_t* parser_read_import(parser_t* p)
{
    parser_get(p); // skip "import"
    parser_expect_slit(p);
    token_t* path = parser_get(p);
    ast_node_t* node = ast_import_init(path->loc, unwrap_string_literal(path->content));
    parser_expect(p, ';');
    parser_get(p); // skip ";"
    return node;
}

static ast_node_t* parser_read_func_definition(parser_t* p)
{
    for (;;)
    {
        token_t* token = parser_get(p);
        
    }
}

token_t* parser_read(parser_t* p)
{
    token_t* next = parser_peek(p);
    if (next == NULL)
        return NULL;
    if (parser_check_content(p, "import"))
        return vector_push(p->file->imports, parser_read_import(p));
    if (parser_is_func_definition(p)) 
        {}
        //return vector_push(p->file->decls, parser_read_func_definition(p));
    ast_print(p->file);
    if (token_has_content(next))
        errorp(next->loc->row, next->loc->col, "encountered unknown token: %s", next->content);
    else
        errorp(next->loc->row, next->loc->col, "encountered unknown token: %c", next->id);
    return NULL;
}