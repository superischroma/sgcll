#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <io.h>

#include "sgcllc.h"

#define NO_TERMINATOR -2
    
static ast_node_t* ast_get_by_token(parser_t* p, token_t* token);
int parser_get_datatype_type(parser_t* p);
static bool parser_is_func_definition(parser_t* p);
static bool parser_is_var_decl(parser_t* p);
static datatype_t* parser_build_datatype(parser_t* p, datatype_type unspecified_dtt, int terminator);
static ast_node_t* parser_read_import(parser_t* p);
static ast_node_t* parser_read_func_definition(parser_t* p);
static void parser_read_func_body(parser_t* p);
static ast_node_t* parser_read_decl(parser_t* p);
static ast_node_t* parser_read_lvar_decl(parser_t* p);
static ast_node_t* parser_read_stmt(parser_t* p);
static ast_node_t* parser_read_expr(parser_t* p, int terminator);
static void parser_read_body(parser_t* p, ast_node_t* body);

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
datatype_t* t_string = &(datatype_t){ VT_PUBLIC, DTT_STRING, 8, false };
datatype_t* t_array = &(datatype_t){ VT_PUBLIC, DTT_ARRAY, 8, false };
datatype_t* t_object = &(datatype_t){ VT_PUBLIC, DTT_OBJECT, 8, false };

void set_up_builtins(void)
{
    builtins = map_init(NULL, 15);
    map_put(builtins, "__builtin_i32_println", ast_builtin_init(t_void, "__builtin_i32_println",
        vector_qinit(1, ast_lvar_init(t_i32, NULL, "i", NULL, NULL)), NULL));
    map_put(builtins, "__builtin_f32_println", ast_builtin_init(t_void, "__builtin_f32_println",
        vector_qinit(1, ast_lvar_init(t_f32, NULL, "f", NULL, NULL)), NULL));
    map_put(builtins, "__builtin_string_println", ast_builtin_init(t_void, "__builtin_string_println",
        vector_qinit(1, ast_lvar_init(t_string, NULL, "str", NULL, NULL)), NULL));
}

int precedence(int op)
{
    switch (op)
    {
        case KW_COMMA:
            return 17;
        case OP_ASSIGN:
            return 16;
        case OP_EQUAL:
        case OP_NOT_EQUAL:
            return 10;
        case OP_ADD:
        case OP_SUB:
            return 6;
        case OP_MUL:
        case OP_DIV:
        case OP_MOD:
            return 5;
        case OP_MAKE:
        case OP_NOT:
        case OP_MAGNITUDE:
            return 3;
        case OP_SUBSCRIPT:
        case OP_SELECTION:
        case OP_FUNC_CALL:
            return 2;
    }
    return 18;
}

datatype_t* make_unsign_type(datatype_t* dt)
{
    if (dt == t_i8) return t_ui8;
    if (dt == t_i16) return t_ui16;
    if (dt == t_i32) return t_ui32;
    if (dt == t_i64) return t_ui64;
    return dt;
}

datatype_t* get_arith_type(token_t* token)
{
    int len = strlen(token->content);
    datatype_t* dt = t_i32;
    bool usign = false, certainly_integral = false;
    #define update(type) dt = (usign ? make_unsign_type(type) : type), certainly_integral = true
    for (int i = len - 1; i >= 0; i--)
    {
        char c = token->content[i];
        if (c == 'u' || c == 'U') usign = true, certainly_integral = true;
        if (c == 'b' || c == 'B') update(t_i8);
        if (c == 's' || c == 'S') update(t_i16);
        if (c == 'l' || c == 'L') update(t_i64);
        if (c == 'f' || c == 'F')
        {
            if (certainly_integral) errorp(token->loc->row, token->loc->col, "attempting to use floating point type signature on an integer literal");
            dt = t_f32;
        }
        if (c == 'd' || c == 'D' || (c == '.' && !isfloattype(dt->type)))
        {
            if (certainly_integral) errorp(token->loc->row, token->loc->col, "attempting to use floating point type signature on an integer literal");
            dt = t_f64;
        }
    }
    #undef update
    return dt;
}

static datatype_t* arith_conv(datatype_t* t1, datatype_t* t2)
{
    if ((t2->size > t1->size && !isfloattype(t1->type)) || (t2->type == DTT_F32 && t1->type != DTT_F64))
    {
        datatype_t* tmp = t1;
        t1 = t2;
        t2 = tmp;
    }
    // integer promotion
    if (t1->type == DTT_I16 || t1->type == DTT_I8)
        t1 = t_i32;
    if (t2->type == DTT_I16 || t2->type == DTT_I8)
        t2 = t_i32;
    if (t1->type == t2->type)
    {
        if (t1->usign)
            return t1;
        if (t2->usign)
            return t2;
    }
    return t1;
}

bool same_datatype(parser_t* p, datatype_t* t1, datatype_t* t2)
{
    if (!t1 || !t2)
        return false;
    // this function exists so that object checks can be performed soon
    return t1->type == t2->type;
}

datatype_t* clone_datatype(datatype_t* dt)
{
    datatype_t* new = calloc(1, sizeof(datatype_t));
    *new = *dt;
    return new;
}

char* make_label(parser_t* p, void* content)
{
    buffer_t* namebuffer = buffer_init(4, 2);
    buffer_append(namebuffer, '.');
    buffer_append(namebuffer, 'L');
    char idbuffer[33];
    itos(p->labels->size, idbuffer);
    for (int i = 0; i < 33; i++)
    {
        if (idbuffer[i] == '\0')
            break;
        buffer_append(namebuffer, idbuffer[i]);
    }
    buffer_append(namebuffer, '\0');
    char* label = buffer_export(namebuffer);
    buffer_delete(namebuffer);
    map_put(p->labels, label, content);
    return label;
}

char* make_func_label(char* filename, ast_node_t* func, ast_node_t* current_blueprint)
{
    buffer_t* buffer = buffer_init(15, 10);
    buffer_string(buffer, filename);
    buffer_append(buffer, '@');
    if (func->func_type == 'c')
        buffer_append(buffer, 'c');
    else if (func->func_type == 'd')
        buffer_append(buffer, 'd');
    else if (current_blueprint)
    {
        buffer_string(buffer, "b@");
        buffer_string(buffer, current_blueprint->bp_name);
    }
    else
        buffer_append(buffer, 'g');
    buffer_append(buffer, '@');
    buffer_string(buffer, func->func_name);
    for (int i = current_blueprint && func->func_type != 'c' ? 1 : 0; i < func->params->size; i++)
    {
        buffer_append(buffer, '@');
        ast_node_t* param = vector_get(func->params, i);
        datatype_t* dt = param->datatype;
        if (dt->usign)
            buffer_string(buffer, "unsigned$");
        #define types \
            case DTT_I8: buffer_string(buffer, "i8"); break; \
            case DTT_I16: buffer_string(buffer, "i16"); break; \
            case DTT_I32: buffer_string(buffer, "i32"); break; \
            case DTT_I64: buffer_string(buffer, "i64"); break; \
            case DTT_F32: buffer_string(buffer, "f32"); break; \
            case DTT_F64: buffer_string(buffer, "f64"); break; \
            case DTT_STRING: buffer_string(buffer, "string"); break
        switch (dt->type)
        {
            types;
            case DTT_ARRAY:
            {
                datatype_t* basetype = dt;
                for (; dt->array_type != NULL; dt = dt->array_type);
                switch (basetype->type)
                {
                    types;
                }
                buffer_string(buffer, "$$");
                char dbuffer[33];
                itos(dt->depth, dbuffer);
                buffer_string(buffer, dbuffer);
                break;
            }
        }
    }
    return buffer_export(buffer);
}

parser_t* parser_init(lexer_t* lex)
{
    parser_t* p = calloc(1, sizeof(parser_t));
    location_t* loc = calloc(1, sizeof(location_t));
    loc->row = loc->col = loc->offset = 0;
    p->nfile = ast_file_init(loc);
    p->genv = map_init(NULL, 50);
    p->lenv = NULL;
    p->labels = map_init(NULL, 20);
    p->lex = lex;
    p->oindex = 0;
    p->userexterns = vector_init(5, 5);
    p->cexterns = vector_init(5, 5);
    p->links = vector_init(5, 5);
    p->funcs = map_init(NULL, 50);
    p->entry = "main";
    vector_push(p->cexterns, ast_builtin_init(t_void, "__builtin_init",
        vector_init(DEFAULT_CAPACITY, DEFAULT_ALLOC_DELTA), NULL));
    return p;
}

void parser_make_header(parser_t* p, FILE* out)
{
    write_header(out, p->genv);
}

// vector is deleted by function if already exists
static void parser_ensure_cextern(parser_t* p, char* name, datatype_t* dt, vector_t* args)
{
    ast_node_t* extrn = map_get(p->lenv ? p->lenv : p->genv, name);
    if (extrn)
    {
        vector_delete(args);
        return;
    }
    map_put(p->genv, name, vector_push(p->cexterns, ast_builtin_init(dt, name, args, NULL)));
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
        {
            datatype_t* dt = get_arith_type(token);
            if (isfloattype(dt->type))
            {
                char* label = make_label(p, token->content);
                return ast_fliteral_init(dt, token->loc, atof(token->content), label);
            }
            return ast_iliteral_init(dt, token->loc, atoll(token->content));
        }
        case TT_STRING_LITERAL:
        {
            char* label = make_label(p, token->content);
            return ast_sliteral_init(t_string, token->loc, token->content, label);
        }
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

int parser_get_kw_settings(parser_t* p, int kw)
{
    bool found = false;
    switch (kw)
    {
        #define keyword(id, _, settings) case id: return settings;
        #include "keywords.inc"
        #undef keyword
    }
    return -1;
}

int parser_get_datatype_type(parser_t* p)
{
    token_t* token = parser_peek(p);
    if (token == NULL)
        return -2;
    if (token_has_content(token)) // objects...?
    {
        ast_node_t* node = map_get(p->lenv ? p->lenv : p->genv, token->content);
        if (node->type != AST_BLUEPRINT)
            return -1;
        return DTT_OBJECT;
    }
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
        {
            if (parser_get_kw_settings(p, token->id) == 0)
                return false;
            continue;
        }
        if (token->type == TT_IDENTIFIER)
        {
            if (map_get_local(p->lenv ? p->lenv : p->genv, token->content))
                return false;
            ast_node_t* node = map_get(p->lenv ? p->lenv : p->genv, token->content);
            if (node && node->type == AST_BLUEPRINT)
                continue;
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

static bool parser_is_header_statement(parser_t* p, int kw)
{
    return parser_check(p, kw);
}

static ast_node_t* parser_read_if_statement(parser_t* p)
{
    token_t* if_keyword = parser_expect(p, KW_IF);
    parser_expect(p, '(');
    ast_node_t* condition = parser_read_expr(p, ')');
    parser_expect(p, ')');
    ast_node_t* if_stmt = ast_if_init(p->current_func->datatype, if_keyword->loc,
        isfloattype(condition->datatype->type) ? ast_cast_init(t_i64, condition->loc, condition) : condition);
    if (parser_check(p, '{'))
    {
        parser_get(p);
        parser_read_body(p, if_stmt->if_then);
    }
    else
        vector_push(if_stmt->if_then->statements, parser_read_stmt(p));
    if (parser_check(p, KW_ELSE))
    {
        parser_get(p);
        if (parser_check(p, '{'))
        {
            parser_get(p);
            parser_read_body(p, if_stmt->if_els);
        }
        else
            vector_push(if_stmt->if_els->statements, parser_read_stmt(p));
    }
    return if_stmt;
}

static ast_node_t* parser_read_while_statement(parser_t* p)
{
    token_t* while_keyword = parser_expect(p, KW_WHILE);
    parser_expect(p, '(');
    ast_node_t* condition = parser_read_expr(p, ')');
    parser_expect(p, ')');
    ast_node_t* while_stmt = ast_while_init(p->current_func->datatype, while_keyword->loc,
        isfloattype(condition->datatype->type) ? ast_cast_init(t_i64, condition->loc, condition) : condition);
    if (parser_check(p, '{'))
    {
        parser_get(p);
        parser_read_body(p, while_stmt->while_then);
    }
    else
        vector_push(while_stmt->while_then->statements, parser_read_stmt(p));
    return while_stmt;
}

static ast_node_t* parser_read_for_statement(parser_t* p)
{
    token_t* for_keyword = parser_expect(p, KW_FOR);
    parser_expect(p, '(');
    ast_node_t* init = parser_read_stmt(p);
    ast_node_t* condition = parser_read_expr(p, ';');
    ast_node_t* post = parser_read_expr(p, ')');
    parser_expect(p, ')');
    ast_node_t* for_stmt = ast_for_init(p->current_func->datatype, for_keyword->loc,
        init, isfloattype(condition->datatype->type) ? ast_cast_init(t_i64, condition->loc, condition) : condition, post);
    if (parser_check(p, '{'))
    {
        parser_get(p);
        parser_read_body(p, for_stmt->for_then);
    }
    else
        vector_push(for_stmt->for_then->statements, parser_read_stmt(p));
    return for_stmt;
}

static datatype_t* parser_build_datatype(parser_t* p, datatype_type unspecified_dtt, int terminator)
{
    datatype_t* dt = calloc(1, sizeof(datatype_t));
    dt->visibility = VT_PRIVATE;
    dt->usign = false;
    dt->type = unspecified_dtt;
    dt->size = 4;
    for (;; parser_get(p))
    {
        token_t* token = parser_peek(p);
        if (token == NULL)
            errorp(0, 0, "unexpected end of file while parsing datatype");
        if (token->type == TT_IDENTIFIER)
        {
            ast_node_t* node = map_get(p->lenv ? p->lenv : p->genv, token->content);
            if (!node)
                break;
            if (node->type != AST_BLUEPRINT)
                break;
        }
        if (token->id == terminator)
            break;
        datatype_type dtt = parser_get_datatype_type(p);
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
                case DTT_LET:
                    dt->size = 4;
                    break;
                case DTT_OBJECT:
                {
                    dt->size = 8;
                    dt->name = token->content;
                    break;
                }
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
                dt->visibility = parser_peek(p)->id - KW_PRIVATE;
                break;
            case KW_UNSIGNED:
                dt->usign = true;
                break;
            case KW_LBRACK:
            {
                parser_get(p);
                datatype_t* ddt = calloc(1, sizeof(datatype_t));
                *ddt = *dt;
                dt->array_type = ddt;
                dt->size = 8;
                dt->type = DTT_ARRAY;
                dt->usign = false;
                dt->length = NULL;
                break;
            }
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
    char* filepath = malloc(256);
    sprintf(filepath, "libsgcll/%s.o", node->path);
    vector_push(p->links, filepath);
    char* headerpath = malloc(256);
    sprintf(headerpath, "libsgcll/%s.sgcllh", node->path);
    FILE* header = fopen(headerpath, "rb");
    char* filename = isolate_filename(headerpath);
    vector_t* symbols = read_header(header, filename);
    for (int i = 0; i < symbols->size; i++)
    {
        ast_node_t* node = vector_get(symbols, i);
        char* name;
        if (node->type == AST_FUNC_DEFINITION)
        {
            name = node->func_label;
            vector_t* nonspecific_vec = map_get(p->funcs, node->func_name);
            if (!nonspecific_vec)
                map_put(p->funcs, node->func_name, vector_qinit(1, node));
            else
                vector_push(nonspecific_vec, node);
        }
        else if (node->type == AST_GVAR)
            name = node->var_name;
        ast_print(node);
        printf("included symbol from %s: %s\n", filename, name);
        map_put(p->genv, name, vector_push(p->userexterns, node));
    }
    vector_delete(symbols);
    return node;
}

static ast_node_t* parser_read_func_definition(parser_t* p)
{
    datatype_t* dt = parser_build_datatype(p, DTT_VOID, NO_TERMINATOR);
    token_t* func_name_token = parser_expect_type(p, TT_IDENTIFIER);
    ast_node_t* func_node = ast_func_definition_init(dt, func_name_token->loc, 'g', func_name_token->content, p->lex->filename);
    if (!strcmp(func_name_token->content, "constructor"))
    {
        if (p->current_blueprint == NULL)
            errorp(func_name_token->loc->row, func_name_token->loc->col, "constructor defined outside of blueprint");
        func_node->func_type = 'c';
        func_node->func_name = p->current_blueprint->bp_name;
        func_node->datatype = p->current_blueprint->bp_datatype;
        parser_ensure_cextern(p, "__builtin_alloc_bytes", t_void, vector_init(DEFAULT_CAPACITY, DEFAULT_ALLOC_DELTA));
    }
    else if (!strcmp(func_name_token->content, "destructor"))
    {
        if (p->current_blueprint == NULL)
            errorp(func_name_token->loc->row, func_name_token->loc->col, "destructor defined outside of blueprint");
        func_node->func_type = 'd';
        func_node->func_name = p->current_blueprint->bp_name;
        func_node->datatype = t_void;
    }
    else if (p->current_blueprint)
        func_node->func_type = 'b';
    parser_expect(p, '(');
    map_t* func_host_env = p->lenv ? p->lenv : p->genv;
    map_t* func_env = p->lenv = map_init(func_host_env, 100);
    bool this_arg = p->current_blueprint && func_node->func_type != 'c';
    if (p->current_blueprint)
    {
        ast_node_t* this_var = map_put(p->lenv, "this", ast_lvar_init(p->current_blueprint->bp_datatype, func_name_token->loc, "this", NULL, p->lex->filename));
        if (this_arg) // implicit this arg
        {
            this_var->voffset = 16;
            vector_push(func_node->params, this_var);
        }
        else // constructor this
            vector_push(func_node->local_variables, this_var);
    }
    for (int i = this_arg ? 24 : 16;; i += 8)
    {
        if (parser_check(p, ')'))
        {
            parser_get(p);
            break;
        }
        if (parser_check(p, ','))
            parser_get(p);
        datatype_t* pdt = parser_build_datatype(p, DTT_I32, NO_TERMINATOR);
        token_t* param_name_token = parser_expect_type(p, TT_IDENTIFIER);
        if (!parser_check(p, ')') && !parser_check(p, ','))
            parser_expect(p, ')');
        ast_node_t* lvar = map_put(p->lenv, param_name_token->content, ast_lvar_init(pdt, param_name_token->loc, param_name_token->content, NULL, p->lex->filename));
        lvar->voffset = i;
        vector_push(func_node->params, lvar);
    }
    parser_expect(p, '{');
    func_node->func_label = make_func_label(p->lex->filename, func_node, p->current_blueprint);
    if (map_get(func_host_env, func_node->func_label))
        errorp(func_name_token->loc->row, func_name_token->loc->col, "function is identical to an already-defined function");
    map_put(func_host_env, func_node->func_label, func_node);
    vector_t* nonspecific_vec = map_get(p->funcs, func_node->func_name);
    if (!nonspecific_vec)
        map_put(p->funcs, func_node->func_name, vector_qinit(1, func_node));
    else
        vector_push(nonspecific_vec, func_node);
    ast_node_t* cf = p->current_func;
    p->current_func = func_node;
    parser_read_func_body(p);
    p->current_func = cf;
    p->lenv = p->lenv->parent;
    if (p->lenv == p->genv)
        p->lenv = NULL;
    map_delete(func_env);
    return func_node;
}


static void parser_read_body(parser_t* p, ast_node_t* body)
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
            vector_push(p->current_func->local_variables, stmt);
        vector_push(body->statements, stmt);
        if (!parser_check(p, '}') && stmt->type == AST_RETURN) // early return statement
            errorp(stmt->loc->row, stmt->loc->col, "return statement is not at the end of the block");
    }
}

static void parser_read_func_body(parser_t* p)
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
            vector_push(p->current_func->local_variables, stmt);
        vector_push(p->current_func->body->statements, stmt);
        if (parser_check(p, '}') && stmt->type != AST_RETURN && p->current_func->datatype->type != DTT_VOID) // no return statement
            vector_push(p->current_func->body->statements, ast_return_init(p->current_func->datatype, stmt->loc, ast_iliteral_init(p->current_func->datatype, stmt->loc, 0)));
        else if (!parser_check(p, '}') && stmt->type == AST_RETURN) // early return statement
            errorp(stmt->loc->row, stmt->loc->col, "return statement is not at the end of the block");
    }
}

static bool parser_is_blueprint(parser_t* p)
{
    for (int i = 1;; i++)
    {
        token_t* token = parser_far_peek(p, i);
        if (token == NULL)
            return false;
        if (token->id == KW_BLUEPRINT)
            break;
        if (token->type != TT_KEYWORD)
            return false;
    }
    return true;
}

static ast_node_t* parser_read_blueprint(parser_t* p)
{
    visibility_type vt = VT_PRIVATE;
    for (;;)
    {
        token_t* token = parser_get(p);
        if (token == NULL)
            errorp(0, 0, "unexpected end of file");
        switch (token->id)
        {
            case KW_PRIVATE:
            case KW_PUBLIC:
            {
                vt = parser_peek(p)->id - KW_PRIVATE;
                break;
            }
            case KW_BLUEPRINT:
                goto leave_loop;
            case KW_PROTECTED:
                errorp(token->loc->row, token->loc->col, "blueprint may not be protected", token->id);
            default:
                errorp(token->loc->row, token->loc->col, "unexpected keyword: %i", token->id);
        }
    }
leave_loop:
    token_t* name_token = parser_get(p);
    if (name_token == NULL)
        errorp(0, 0, "unexpected end of file");
    if (!token_has_content(name_token))
        errorp(name_token->loc->row, name_token->loc->col, "expected identifier for blueprint");
    datatype_t* dt = clone_datatype(t_object);
    dt->name = name_token->content;
    ast_node_t* blueprint = ast_blueprint_init(name_token->loc, name_token->content, dt);
    parser_expect(p, '{');
    map_t* bp_host_env = p->lenv ? p->lenv : p->genv;
    map_t* bp_env = p->lenv = map_init(bp_host_env, 100);
    if (map_get(bp_env, name_token->content))
        errorp(name_token->loc->row, name_token->loc->col, "symbol already exists with the name '%s'", name_token->content);
    map_put(bp_host_env, name_token->content, blueprint);
    ast_node_t* cbp = p->current_blueprint;
    p->current_blueprint = blueprint;
    int size = 0;
    for (;;)
    {
        token_t* token = parser_peek(p);
        if (token == NULL)
            errorp(0, 0, "unexpected end of file");
        if (token->id == '}')
        {
            parser_get(p);
            break;
        }
        if (parser_is_var_decl(p))
        {
            ast_node_t* lvar = parser_read_lvar_decl(p);
            lvar->voffset = size;
            size += max(2, lvar->datatype->size);
            vector_push(p->current_blueprint->inst_variables, lvar);
        }
        else if (parser_is_func_definition(p))
            vector_push(p->current_blueprint->methods, parser_read_func_definition(p));
        else
            errorp(token->loc->row, token->loc->col, "expected variable declaration or method definition");
    }
    p->current_blueprint->bp_size = size;
    p->current_blueprint = cbp;
    p->lenv = p->lenv->parent;
    if (p->lenv == p->genv)
        p->lenv = NULL;
    map_delete(bp_env);
    return blueprint;
}

static ast_node_t* parser_read_decl(parser_t* p)
{
    if (parser_is_blueprint(p))
        return parser_read_blueprint(p);
    if (parser_is_func_definition(p))
        return parser_read_func_definition(p);
    return NULL;
}

static ast_node_t* parser_read_lvar_decl(parser_t* p)
{
    datatype_t* dt = parser_build_datatype(p, DTT_I32, NO_TERMINATOR);
    token_t* var_name_token = parser_expect_type(p, TT_IDENTIFIER);
    bool init = parser_check(p, OP_ASSIGN);
    ast_node_t* var_node = map_put(p->lenv ? p->lenv : p->genv, var_name_token->content,
        ast_lvar_init(dt, var_name_token->loc, var_name_token->content, NULL, p->lex->filename));
    if (parser_check(p, OP_ASSIGN))
    {
        parser_unget(p);
        var_node->vinit = parser_read_expr(p, ';');
        return var_node;
    }
    parser_expect(p, KW_SEMICOLON);
    return var_node;
}

static bool parser_is_return_statement(parser_t* p)
{
    token_t* token = parser_peek(p);
    if (token == NULL)
        return false;
    return token->id == KW_RETURN;
}

static bool parser_is_delete_statement(parser_t* p)
{
    token_t* token = parser_peek(p);
    if (token == NULL)
        return false;
    return token->id == KW_DELETE;
}

static ast_node_t* parser_read_return_statement(parser_t* p)
{
    parser_expect(p, KW_RETURN);
    ast_node_t* expr = parser_read_expr(p, ';');
    return ast_return_init(p->current_func->datatype, expr->loc, expr->datatype != p->current_func->datatype ? ast_cast_init(p->current_func->datatype, expr->loc, expr) : expr);
}

static bool parser_is_enter_statement(parser_t* p)
{
    token_t* token = parser_peek(p);
    if (token == NULL)
        return false;
    return token->id == KW_ENTER;
}

static char* parser_read_enter_statement(parser_t* p)
{
    token_t* enter_keyword = parser_expect(p, KW_ENTER);
    token_t* entry = parser_get(p);
    if (entry == NULL)
        errorp(0, 0, "unexpected end of file");
    if (!token_has_content(entry))
        errorp(entry->loc->row, entry->loc->col, "expected identifier");
    parser_expect(p, ';');
    return entry->content;
}

static ast_node_t* parser_read_delete_statement(parser_t* p)
{
    token_t* del_keyword = parser_expect(p, KW_DELETE);
    ast_node_t* node = ast_get_by_token(p, parser_get(p));
    parser_expect(p, ';');
    parser_ensure_cextern(p, "__builtin_delete_array", t_void, vector_init(DEFAULT_CAPACITY, DEFAULT_ALLOC_DELTA));
    return ast_delete_init(t_void, del_keyword->loc, node);
}

static ast_node_t* parser_read_stmt(parser_t* p)
{
    if (parser_is_var_decl(p))
        return parser_read_lvar_decl(p);
    if (parser_is_return_statement(p))
        return parser_read_return_statement(p);
    if (parser_is_delete_statement(p))
        return parser_read_delete_statement(p);
    if (parser_is_header_statement(p, KW_IF))
        return parser_read_if_statement(p);
    if (parser_is_header_statement(p, KW_WHILE))
        return parser_read_while_statement(p);
    if (parser_is_header_statement(p, KW_FOR))
        return parser_read_for_statement(p);
    return parser_read_expr(p, ';');
}

static void parser_rpn(parser_t* p, vector_t* stack, vector_t* expr_result, int terminator)
{
    vector_t* calls = vector_init(5, 5);
    for (;;)
    {
        token_t* token = parser_get(p);
        if (token == NULL)
            errorp(0, 0, "unexpected end of file");
        if (token_has_content(token))
            printf("token: %s\n", token->content);
        else
            printf("token: %c\n", token->id);
        if (token->id == ';' && terminator == ';')
            break;
        if (token->type == TT_CHAR_LITERAL || token->type == TT_STRING_LITERAL || token->type == TT_NUMBER_LITERAL)
            vector_push(expr_result, token);
        else if (token->type == TT_IDENTIFIER)
        {
            ast_node_t* builtin = map_get(builtins, token->content);
            if (builtin && !map_get(p->genv, token->content))
            {
                vector_push(p->userexterns, map_put(p->genv, token->content, builtin));
                map_put(p->funcs, token->content, builtin);
            }
            vector_push(expr_result, token);
        }
        else if (token->id == '(')
        {
            token_t* prev = parser_far_peek(p, -1);
            if (prev != NULL && prev->type == TT_IDENTIFIER && map_get(p->funcs, prev->content))
            {
                if (vector_top(stack) != NULL && ((token_t*) vector_top(stack))->id == OP_SELECTION)
                    vector_push(expr_result, vector_pop(stack));
                vector_push(stack, id_token_init(TT_KEYWORD, OP_FUNC_CALL, token->loc->offset, token->loc->row, token->loc->col));
            }
            vector_push(stack, token);
        }
        else if (token->id == ')' || token->id == ',' || token->id == ']')
        {
            while (vector_top(stack) != NULL && ((token_t*) vector_top(stack))->id != '(')
                vector_push(expr_result, vector_pop(stack));
            if (token->id == ')')
            {
                vector_pop(stack);
                if (terminator == ')' && (vector_top(stack) == NULL || ((token_t*) vector_top(stack))->id != '('))
                {
                    parser_unget(p);
                    break;
                }
            }
            if (token->id == ']' && terminator == ']' && (vector_top(stack) == NULL || ((token_t*) vector_top(stack))->id != '['))
            {
                parser_unget(p);
                break;
            }
        }
        else if (token->id == OP_MAKE)
        {
            int depth = 0;
            int terminator = ';';
            for (int i = 1;; i++)
            {
                token_t* far = parser_far_peek(p, i);
                if (far == NULL)
                    errorp(token->loc->row, token->loc->col, "unexpected end of file");
                if (far->id == '[' || far->id == ';')
                {
                    terminator = far->id;
                    break;
                }
            }
            datatype_t* type = parser_build_datatype(p, DTT_VOID, terminator);
            if (type == NULL)
                errorp(token->loc->row, token->loc->col, "unexpected end of file");
            for (;;)
            {
                token_t* mtoken = parser_get(p);
                if (mtoken == NULL)
                    errorp(token->loc->row, token->loc->col, "unexpected end of file");
                if (mtoken->id == ',' || mtoken->id == ')' || mtoken->id == ']' || mtoken->id == ';')
                {
                    parser_unget(p);
                    break;
                }
                if (mtoken->id == KW_LBRACK)
                {
                    vector_t* mstack = vector_init(20, 10);
                    vector_t* mresult = vector_init(20, 10);
                    parser_rpn(p, mstack, mresult, ']');
                    vector_delete(mstack);
                    parser_expect(p, ']');
                    for (int i = 0; i < mresult->size; i++)
                        vector_push(expr_result, vector_get(mresult, i));
                    vector_delete(mresult);
                    depth++;
                }
            }
            char* depthbuffer = malloc(33);
            itos(depth, depthbuffer);
            vector_push(expr_result, datatype_token_init(TT_DATATYPE, type, token->loc->offset, token->loc->row, token->loc->col));
            vector_push(expr_result, content_token_init(TT_NUMBER_LITERAL, depthbuffer, token->loc->offset, token->loc->row, token->loc->col));
            vector_push(expr_result, token);
        }
        else
        {
            while (vector_top(stack) != NULL && precedence(token->id) > precedence(((token_t*) vector_top(stack))->id))
                vector_push(expr_result, vector_pop(stack));
            vector_push(stack, token);
        }
    }
    while (vector_top(stack) != NULL)
        vector_push(expr_result, vector_pop(stack));
    vector_delete(calls);
}

static ast_node_t* parser_read_expr(parser_t* p, int terminator)
{
    vector_t* stack = vector_init(20, 10);
    vector_t* expr_result = vector_init(20, 10);
    parser_rpn(p, stack, expr_result, terminator);
    vector_clear(stack, RETAIN_OLD_CAPACITY);
    if (!expr_result->size)
        errorp(parser_peek(p)->loc->row, parser_peek(p)->loc->col, "null statement is not allowed");
    printf("----- results: (size %i)\n", expr_result->size);
    for (int i = 0; i < expr_result->size; i++)
    {
        token_t* token = (token_t*) vector_get(expr_result, i);
        if (token_has_content(token))
            printf("result[%i] = %s\n", i, token->content);
        else if (token->type == TT_DATATYPE)
            printf("result[%i] = %i\n", i, token->dt->type);
        else
            printf("result[%i] = %c (id: %i)\n", i, token->id, token->id);
    }
    printf("----- end results\n");
    for (int i = 0; i < expr_result->size; i++)
    {
        token_t* token = (token_t*) vector_get(expr_result, i);
        if (token->type == TT_CHAR_LITERAL || token->type == TT_STRING_LITERAL || token->type == TT_NUMBER_LITERAL)
            vector_push(stack, ast_get_by_token(p, token));
        if (token->type == TT_DATATYPE)
            vector_push(stack, ast_stub_init(token->dt, token->loc));
        if (token->type == TT_IDENTIFIER)
        {
            ast_node_t* found = NULL;
            ast_node_t* builtin = map_get(builtins, token->content);
            if (!builtin)
            {
                vector_t* flavors = map_get(p->funcs, token->content);
                if (!flavors)
                {
                    if (i + 1 < expr_result->size && ((token_t*) vector_get(expr_result, i + 1))->id == OP_SELECTION)
                    {
                        ast_node_t* top = vector_top(stack);
                        ast_node_t* blueprint = map_get(p->lenv ? p->lenv : p->genv, top->datatype->name);
                        for (int i = 0; i < blueprint->inst_variables->size; i++)
                        {
                            ast_node_t* inst_var = vector_get(blueprint->inst_variables, i);
                            if (!strcmp(inst_var->var_name, token->content))
                            {
                                vector_push(stack, inst_var);
                                goto inst_var_success;
                            }
                        }
                    } 
                    vector_push(stack, ast_get_by_token(p, token));
inst_var_success:
                    continue;
                }
                vector_push(stack, vector_get(flavors, 0));
                continue;
            }
            else
                vector_push(stack, builtin);
            continue;
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
                case OP_ASSIGN_ADD:
                case OP_ASSIGN_SUB:
                case OP_ASSIGN_MUL:
                case OP_ASSIGN_DIV:
                case OP_ASSIGN_MOD:
                case OP_EQUAL:
                case OP_NOT_EQUAL:
                {
                    ast_node_t* rhs = vector_pop(stack);
                    ast_node_t* lhs = vector_pop(stack);
                    if (!lhs || !rhs)
                        errorp(token->loc->row, token->loc->col, "expected 2 operands for operator %i", token->id);
                    if (lhs->datatype->type == DTT_LET) lhs->datatype = rhs->datatype;
                    if (rhs->type == AST_MAKE) lhs->datatype = rhs->datatype;
                    vector_push(stack, ast_binary_op_init(token->id, arith_conv(lhs->datatype, rhs->datatype), token->loc, lhs, rhs));
                    break;
                }
                case OP_SUBSCRIPT:
                {
                    ast_node_t* index = vector_pop(stack);
                    ast_node_t* symbol = vector_pop(stack);
                    if (!symbol || !index)
                        errorp(token->loc->row, token->loc->col, "expected 2 operands for subscript operator");
                    vector_push(stack, ast_binary_op_init(token->id, symbol->datatype->array_type, token->loc, symbol, index));
                    break;
                }
                case OP_SELECTION:
                {
                    ast_node_t* member = vector_pop(stack);
                    ast_node_t* obj = vector_pop(stack);
                    if (!member || !obj)
                        errorp(token->loc->row, token->loc->col, "expected 2 operands for selection operator");
                    vector_push(stack, ast_binary_op_init(token->id, member->datatype, token->loc, obj, member));
                    break;
                }
                case OP_MAGNITUDE:
                {
                    ast_node_t* operand = vector_pop(stack);
                    if (!operand)
                        errorp(token->loc->row, token->loc->col, "expected operand for operator %i", token->id);
                    vector_push(stack, ast_unary_op_init(token->id, t_ui64, token->loc, operand));
                    parser_ensure_cextern(p, "__builtin_array_size", t_void, vector_init(DEFAULT_CAPACITY, DEFAULT_ALLOC_DELTA));
                    break;
                }
                case OP_MAKE:
                {
                    ast_node_t* depth_node = vector_pop(stack);
                    int depth = depth_node->ivalue;
                    ast_node_t* dt_node = vector_pop(stack);
                    datatype_t* dt = dt_node->datatype;
                    for (int i = 0; i < depth; i++)
                    {
                        ast_node_t* dimension = vector_pop(stack);
                        datatype_t* adt = calloc(1, sizeof(datatype_t));
                        adt->array_type = dt;
                        adt->length = dimension;
                        adt->size = 8;
                        adt->type = DTT_ARRAY;
                        adt->usign = false;
                        adt->visibility = VT_PRIVATE;
                        adt->depth = i + 1;
                        dt = adt;
                    }
                    vector_push(stack, ast_make_init(dt, token->loc));
                    parser_ensure_cextern(p, "__builtin_dynamic_ndim_array", t_void, vector_init(DEFAULT_CAPACITY, DEFAULT_ALLOC_DELTA));
                    break;
                }
                case OP_FUNC_CALL:
                {
                    /*
                    printf("----- stack: \n");
                    for (int i = stack->size - 1; i >= 0; i--)
                        ast_print(vector_get(stack, i));
                    printf("-----\n");
                    */
                    ast_node_t* random_flavor = NULL;
                    ast_node_t* obj = NULL;
                    for (int i = stack->size - 1; i >= 0; i--)
                    {
                        ast_node_t* node = vector_get(stack, i);
                        if (node->type == AST_FUNC_DEFINITION)
                        {
                            random_flavor = node;
                            break;
                        }
                        if (node->type == OP_SELECTION && node->rhs->type == AST_FUNC_DEFINITION)
                        {
                            random_flavor = node->rhs;
                            obj = node->lhs;
                            break;
                        }
                    }
                    if (!random_flavor)
                        errorp(token->loc->row, token->loc->col, "no function name provided for function call");
                    vector_t* flavors = map_get(p->funcs, random_flavor->func_name);
                    ast_node_t* found = NULL;
                    if (!random_flavor->extrn)
                    {
                        for (int i = 0; i < flavors->size; i++)
                        {
                            ast_node_t* flavor = vector_get(flavors, i);
                            bool thisless = flavor->func_type == 'g' || flavor->func_type == 'c';
                            int thisless_size = flavor->params->size - (thisless ? 0 : 1);
                            for (int j = thisless ? 0 : 1; j < flavor->params->size && vector_check_bounds(stack, stack->size - thisless_size + j); j++)
                            {
                                ast_node_t* param = vector_get(flavor->params, j);
                                if (!same_datatype(p, param->datatype, ((ast_node_t*) vector_get(stack, stack->size - thisless_size + j))->datatype))
                                    goto try_again;
                            }
                            found = flavor;
                            break;
try_again:
                        }
                    }
                    else
                        found = random_flavor;
found_function:
                    if (!found)
                        errorp(token->loc->row, token->loc->col, "could not find a function by the name of '%s' that matched the specified args", token->content);
                    printf("found: %s\n", found->func_label);
                    if (found->residing != NULL && strcmp(p->lex->filename, found->residing) && found->datatype->visibility != VT_PUBLIC)
                        errorp(token->loc->row, token->loc->col, "can't call a function that's private to '%s'", found->residing);
                    vector_t* args = vector_init(found->params->size, 1);
                    args->size = found->params->size;
                    bool thisless = found->func_type == 'g' || found->func_type == 'c';
                    int thisless_offset = !thisless ? 1 : 0;
                    // add(p, 5, 3)
                    // add(5, 3)
                    for (int i = found->params->size - 1; i >= thisless_offset; i--)
                    {
                        ast_node_t* arg = vector_pop(stack), * param = vector_get(found->params, i);
                        if (!arg)
                            errorp(token->loc->row, token->loc->col, "function %s expected %i parameters, got %i", found->func_name, found->params->size, i);
                        if (arg->datatype->type == param->datatype->type)
                            args->data[i] = arg;
                        else
                            args->data[i] = ast_cast_init(param->datatype, arg->loc, arg);
                    }
                    if (!thisless)
                        args->data[0] = obj;
                    vector_pop(stack); // pop func name
                    vector_push(stack, ast_func_call_init(found->datatype, token->loc, found, args));
                    break;
                }
            }
        }
    }
    if (!stack->size)
    {
        printf("-- end of stack trace -\n");
        for (int i = 0; i < stack->size; i++)
            ast_print(vector_get(stack, i));
        printf("-^^^- stack trace -^^^-\n");
        errorp(0, 0, "dev error: you messed up with the stack goofball");
    }
    ast_node_t* top = (ast_node_t*) vector_top(stack);
    printf("exporting: \n");
    ast_print(top);
    vector_delete(stack);
    vector_delete(expr_result);
    return top;
}

void parser_read(parser_t* p)
{
    token_t* next = parser_peek(p);
    if (next == NULL)
        return;
    if (parser_check(p, KW_IMPORT))
    {
        vector_push(p->nfile->imports, parser_read_import(p));
        return;
    }
    if (parser_check(p, KW_ENTER))
    {
        p->entry = parser_read_enter_statement(p);
        return;
    }
    ast_node_t* node = parser_read_decl(p);
    if (node)
    {
        vector_push(p->nfile->decls, node);
        return;
    }
    ast_print(p->nfile);
    if (token_has_content(next))
        errorp(next->loc->row, next->loc->col, "encountered unknown token: %s", next->content);
    else
        errorp(next->loc->row, next->loc->col, "encountered unknown token: %i", next->id);
    return;
}

void parser_delete(parser_t* p)
{
    if (!p) return;
    map_delete(p->genv);
    map_delete(p->lenv);
    vector_delete(p->userexterns);
    vector_delete(p->cexterns);
    free(p);
}