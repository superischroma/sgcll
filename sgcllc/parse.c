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
datatype_t* t_ui8 = &(datatype_t){ VT_PUBLIC, DT_I8, 1, true };
datatype_t* t_ui16 = &(datatype_t){ VT_PUBLIC, DT_I16, 2, true };
datatype_t* t_ui32 = &(datatype_t){ VT_PUBLIC, DT_I32, 4, true };
datatype_t* t_ui64 = &(datatype_t){ VT_PUBLIC, DT_I64, 8, true };
datatype_t* t_f32 = &(datatype_t){ VT_PUBLIC, DT_F32, 4, false };
datatype_t* t_f64 = &(datatype_t){ VT_PUBLIC, DT_F64, 8, false };

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

bool parser_is_type_spec(parser_t* p)
{
    token_t* token = parser_peek(p);
    if (token_has_content(token))
        return false;
    switch (token->id)
    {
        #define keyword(id, _, istype) \
        case id: \
        { \
            if (istype) \
                return true; \
            break; \
        }
        #include "keywords.inc"
        #undef keyword
    }
    return false;
}

static vector_t* parser_collect_type_specs(parser_t* p)
{
    vector_t* vec = vector_init(10, 5);
    for (;;)
    {
        if (parser_eof(p))
            break;
        if (!parser_is_type_spec(p))
            break;
        token_t* token = parser_get(p);
        vector_push(vec, (void*) token->id);
    }
    return vec;
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
    parser_get(p); // skip import keyword
    token_t* path = parser_expect_type(p, TT_STRING_LITERAL);
    ast_node_t* node = ast_import_init(path->loc, unwrap_string_literal(path->content));
    parser_expect(p, ';');
    return node;
}

static ast_node_t* parser_read_func_definition(parser_t* p)
{
    vector_t* specifiers = parser_collect_type_specs(p);
    token_t* func_name_token = parser_expect_type(p, TT_IDENTIFIER);
    parser_expect(p, '(');
    p->lenv = map_init(p->lenv ? p->lenv : p->genv, 100);
    for (;;)
    {
        if (parser_check(p, ')'))
        {
            parser_get(p);
            break;
        }
        vector_t* parameter_specifiers = parser_collect_type_specs(p);
        token_t* param_name_token = parser_expect_type(p, TT_IDENTIFIER);
        if (!parser_check(p, ')') && !parser_check(p, ','))
            parser_expect(p, ')');
    }
}

token_t* parser_read(parser_t* p)
{
    token_t* next = parser_peek(p);
    if (next == NULL)
        return NULL;
    if (parser_check(p, KW_IMPORT))
        return vector_push(p->file->imports, parser_read_import(p));
    if (parser_is_func_definition(p)) 
        return vector_push(p->file->decls, parser_read_func_definition(p));
    ast_print(p->file);
    if (token_has_content(next))
        errorp(next->loc->row, next->loc->col, "encountered unknown token: %s", next->content);
    else
        errorp(next->loc->row, next->loc->col, "encountered unknown token: %i", next->id);
    return NULL;
}