#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <io.h>

#include "sgcllc.h"

#define NO_TERMINATOR -2

#define isarithtype(type) (type != DTT_ARRAY && type != DTT_STRING && type != DTT_OBJECT)

typedef struct 
{
    int operator;
    bool lowlvl;
    int unsafe; // -2 for no unsafe, -1 for unsafe no specified stack allocation
} header_plus_t;
    
static ast_node_t* ast_get_by_token(parser_t* p, token_t* token);
int parser_get_datatype_type(parser_t* p);
static bool parser_is_func_definition(parser_t* p);
static bool parser_is_var_decl(parser_t* p);
static datatype_t* parser_build_datatype(parser_t* p, datatype_type unspecified_dtt, int terminator, header_plus_t* additional);
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
}

int precedence(int op)
{
    switch (op)
    {
        case KW_COMMA:
            return 17;
        case OP_ASSIGN:
        case OP_ASSIGN_ADD:
        case OP_ASSIGN_SUB:
        case OP_ASSIGN_MUL:
        case OP_ASSIGN_DIV:
        case OP_ASSIGN_MOD:
        case OP_ASSIGN_AND:
        case OP_ASSIGN_OR:
        case OP_ASSIGN_XOR:
        case OP_ASSIGN_SHIFT_LEFT:
        case OP_ASSIGN_SHIFT_RIGHT:
        case OP_ASSIGN_SHIFT_URIGHT:
            return 16;
        case OP_TERNARY_Q:
        case OP_TERNARY_C:
            return 15;
        case OP_EQUAL:
        case OP_NOT_EQUAL:
            return 14;
        case OP_GREATER:
        case OP_GREATER_EQUAL:
        case OP_LESS:
        case OP_LESS_EQUAL:
            return 13;
        case OP_SPACESHIP:
            return 12;
        case OP_LOGICAL_OR:
            return 11;
        case OP_LOGICAL_AND:
            return 10;
        case OP_OR:
            return 9;
        case OP_XOR:
            return 8;
        case OP_AND:
            return 7;
        case OP_SHIFT_LEFT:
        case OP_SHIFT_RIGHT:
        case OP_SHIFT_URIGHT:
            return 6;
        case OP_ADD:
        case OP_SUB:
            return 5;
        case OP_MUL:
        case OP_DIV:
        case OP_MOD:
            return 4;
        case OP_MAKE:
        case OP_NOT:
        case OP_MAGNITUDE:
        case OP_CAST:
        case OP_COMPLEMENT:
        case OP_MINUS:
        case OP_PREFIX_INCREMENT:
        case OP_PREFIX_DECREMENT:
            return 3;
        case OP_POSTFIX_INCREMENT:
        case OP_POSTFIX_DECREMENT:
        case OP_SUBSCRIPT:
        case OP_SELECTION:
        case OP_FUNC_CALL:
            return 2;
        case OP_SCOPE:
            return 1;
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
        if (c == 'B') update(t_i8);
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

datatype_t* arith_conv(datatype_t* t1, datatype_t* t2)
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
    if (t1->type == DTT_OBJECT && t2->type == DTT_OBJECT)
        return !strcmp(t1->name, t2->name);
    return t1->type == t2->type;
}

bool convertible_datatype(parser_t* p, datatype_t* t1, datatype_t* t2)
{
    if (!t1 || !t2)
        return false;
    if (t1->type == DTT_OBJECT && t2->type == DTT_OBJECT)
        return !strcmp(t1->name, t2->name);
    if (isarithtype(t1->type) && isarithtype(t2->type))
        return true;
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
    for (int i = (current_blueprint && func->func_type != 'c') ? 1 : 0; i < func->params->size; i++)
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
            case DTT_STRING: buffer_string(buffer, "string"); break; \
            case DTT_OBJECT: buffer_string(buffer, dt->name); break
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
    buffer_append(buffer, '\0');
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
    p->has_lowlvl = false;
    return p;
}

void parser_make_header(parser_t* p, FILE* out)
{
    write_header(out, p->genv);
}

// vector is deleted by function if already exists
void parser_ensure_cextern(parser_t* p, char* name, datatype_t* dt, vector_t* args)
{
    ast_node_t* extrn = map_get(p->lenv ? p->lenv : p->genv, name);
    if (extrn)
        return;
    map_put(p->genv, name, vector_push(p->cexterns, ast_builtin_init(dt, name, args, NULL, 'b')));
}

static long long read_iliteral(char* str, location_t* loc)
{
    int tlen = strlen(str), radix = 10;
    if (tlen >= 2 && str[0] == '0')
    {
        switch (str[1])
        {
            case 'x': radix = 16; break;
            case 'b': radix = 2; break;
            case 'o': radix = 8; break;
            case 'a':
            case 'c' ... 'n':
            case 'p' ... 'w':
                errorp(loc->row, loc->col, "unexpected radix prefix for integer literal");
        }
    }
    return strtoll(radix == 10 ? str : str + 2, NULL, radix);
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
            return ast_iliteral_init(t_i8, token->loc, token->content[1]);
        case TT_NUMBER_LITERAL:
        {
            datatype_t* dt = get_arith_type(token);
            if (isfloattype(dt->type))
            {
                char* label = make_label(p, token->content);
                return ast_fliteral_init(dt, token->loc, atof(token->content), label);
            }
            return ast_iliteral_init(dt, token->loc, read_iliteral(token->content, token->loc));
        }
        case TT_STRING_LITERAL:
        {
            char* label = make_label(p, token->content);
            return ast_sliteral_init(t_string, token->loc, token->content, label);
        }
        case TT_KEYWORD:
        {
            switch (token->id)
            {
                case KW_TRUE:
                    return ast_iliteral_init(t_bool, token->loc, 1LL);
                case KW_FALSE:
                    return ast_iliteral_init(t_bool, token->loc, 0LL);
                default:
                    errorp(token->loc->row, token->loc->col, "unknown token");
            }
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

datatype_t* get_default_type(int kw)
{
    switch (kw)
    {
        case KW_VOID: return t_void;
        case KW_BOOL: return t_bool;
        case KW_I8: return t_i8;
        case KW_I16: return t_i16;
        case KW_I32: return t_i32;
        case KW_I64: return t_i64;
        case KW_F32: return t_f32;
        case KW_F64: return t_f64;
        case KW_STRING: return t_string;
        default: return t_object;
    }
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
        token_t* prev = parser_far_peek(p, i - 1);
        if (token == NULL)
            return false;
        if (prev && token->type == TT_KEYWORD && token->id == '(' && prev->type == TT_IDENTIFIER)
            break;
    }
    for (i++;; i++)
    {
        token_t* token = parser_far_peek(p, i);
        if (token == NULL)
            return false;
        if (token->type == TT_KEYWORD && token->id == KW_RPAREN)
            break;
    }
    token_t* lbrace_or_semicolon = parser_far_peek(p, ++i);
    if (lbrace_or_semicolon->type != TT_KEYWORD)
        return false;
    if (lbrace_or_semicolon->id != KW_LBRACE && lbrace_or_semicolon->id != KW_SEMICOLON)
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

static ast_node_t* parser_read_if_statement(parser_t* p, bool elif)
{
    token_t* if_keyword = parser_expect(p, elif ? KW_ELIF : KW_IF);
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
    else if (parser_check(p, KW_ELIF))
        vector_push(if_stmt->if_els->statements, parser_read_if_statement(p, true));
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

static ast_node_t* parser_read_stub_statement(parser_t* p, int kw, ast_node_type type)
{
    token_t* kwtok = parser_expect(p, kw);
    parser_expect(p, ';');
    return ast_stub_init(type, NULL, kwtok->loc);
}

static ast_node_t* parser_read_switch_statement(parser_t* p)
{
    token_t* switch_kw = parser_expect(p, KW_SWITCH);
    parser_expect(p, '(');
    ast_node_t* cmp = parser_read_expr(p, ')');
    parser_expect(p, ')');
    ast_node_t* switch_stmt = ast_switch_init(switch_kw->loc, cmp);
    parser_expect(p, '{');
    bool last_stmt_case = false;
    ast_node_t* current_case = NULL;
    for (;;)
    {
        token_t* token = parser_peek(p);
        if (token == NULL)
            errorp(0, 0, "unexpected end of file");
        if (parser_check(p, '}'))
        {
            parser_get(p);
            break;
        }
        if (parser_check(p, KW_CASE) || parser_check(p, KW_DEFAULT))
        {
            int id = token->id;
            if (!last_stmt_case)
            {
                last_stmt_case = true;
                if (current_case)
                    vector_push(switch_stmt->cases, current_case);
                current_case = ast_case_init(token->loc);
                current_case->case_label = make_label(p, NULL);
            }
            parser_get(p);
            if (id == KW_CASE)
                vector_push(current_case->case_conditions, ast_get_by_token(p, parser_get(p)));
            parser_expect(p, ':');
        }
        else if (parser_check(p, '{'))
        {
            if (!current_case)
                errorp(token->loc->row, token->loc->col, "statement in switch body before case label");
            parser_get(p);
            parser_read_body(p, current_case->case_then);
            last_stmt_case = false;
        }
        else
        {
            if (!current_case)
                errorp(token->loc->row, token->loc->col, "statement in switch body before case label");
            vector_push(current_case->case_then->statements, parser_read_stmt(p));
            last_stmt_case = false;
        }
    }
    if (current_case)
        vector_push(switch_stmt->cases, current_case);
    return switch_stmt;
}

static datatype_t* parser_build_datatype(parser_t* p, datatype_type unspecified_dtt, int terminator, header_plus_t* additional)
{
    if (additional)
    {
        additional->operator = -1;
        additional->lowlvl = false;
        additional->unsafe = -2;
    }
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
            case KW_OPERATOR:
            {
                parser_get(p);
                parser_expect(p, '(');
                if (!additional)
                    errorp(token->loc->row, token->loc->col, "operator overload only allowed on functions");
                token_t* operator = parser_get(p);
                if (!operator || operator->type != TT_KEYWORD)
                    errorp(token->loc->row, token->loc->col, "expected operator type after operator specifier");
                parser_expect(p, ')');
                parser_unget(p);
                additional->operator = operator->id;
                break;
            }
            case KW_LOWLVL:
                additional->lowlvl = true;
                break;
            case KW_UNSAFE:
            {
                parser_get(p);
                if (parser_check(p, '('))
                {
                    parser_get(p);
                    token_t* ilit = parser_expect_type(p, TT_NUMBER_LITERAL);
                    parser_expect(p, ')');
                    parser_unget(p);
                    additional->unsafe = read_iliteral(ilit->content, ilit->loc);
                }
                else
                {
                    parser_unget(p);
                    additional->unsafe = -1;
                }
                break;
            }
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
    parser_expect(p, KW_IMPORT); // skip import keyword
    token_t* path = parser_expect_type(p, TT_STRING_LITERAL);
    ast_node_t* node = ast_import_init(path->loc, unwrap_string_literal(path->content));
    parser_expect(p, ';');
    char* headerpath = NULL;
    for (int i = 0; i < options->import_search_paths->size; i++)
    {
        char* path = vector_get(options->import_search_paths, i);
        buffer_t* checkbuf = buffer_init(256, 128);
        buffer_string(checkbuf, path);
        buffer_append(checkbuf, '/');
        buffer_string(checkbuf, node->path);
        buffer_string(checkbuf, ".o");
        char* fullpath = buffer_export(checkbuf);
        buffer_delete(checkbuf);
        buffer_t* headerbuf = buffer_init(256, 128);
        buffer_string(headerbuf, path);
        buffer_append(headerbuf, '/');
        buffer_string(headerbuf, node->path);
        buffer_string(headerbuf, ".sgcllh");
        char* lheaderpath = buffer_export(headerbuf);
        if (fexists(fullpath) && fexists(lheaderpath))
        {
            vector_push(p->links, fullpath);
            headerpath = lheaderpath;
            break;
        }
        free(fullpath);
    }
    if (!headerpath)
        errorp(path->loc->row, path->loc->col, "could not open a library by the name of '%s'", node->path);
    FILE* header = fopen(headerpath, "rb");
    vector_t* symbols = read_header(header, node->path);
    char* lowlvl_path = NULL;
    for (int i = 0; i < symbols->size; i++)
    {
        ast_node_t* symbol = vector_get(symbols, i);
        char* name;
        if (symbol->type == AST_FUNC_DEFINITION)
        {
            name = symbol->func_label;
            vector_t* nonspecific_vec = map_get(p->funcs, symbol->func_name);
            if (!nonspecific_vec)
                map_put(p->funcs, symbol->func_name, vector_qinit(1, symbol));
            else
                vector_push(nonspecific_vec, symbol);
            if (symbol->lowlvl_label)
            {
                parser_ensure_cextern(p, symbol->lowlvl_label, symbol->datatype, symbol->params);
                if (!lowlvl_path)
                {
                    lowlvl_path = malloc(256);
                    sprintf(lowlvl_path, "libsgcll/%s_lowlvl.o", node->path);
                    vector_push(p->links, lowlvl_path);
                }
            }
        }
        else if (symbol->type == AST_GVAR)
            name = symbol->var_name;
        debugf("included symbol from %s: %s\n", node->path, name);
        map_put(p->genv, name, vector_push(p->userexterns, symbol));
    }
    vector_delete(symbols);
    return node;
}

static ast_node_t* parser_read_func_definition(parser_t* p)
{
    header_plus_t* hp = calloc(1, sizeof(header_plus_t));
    datatype_t* dt = parser_build_datatype(p, DTT_VOID, NO_TERMINATOR, hp);
    token_t* func_name_token = parser_expect_type(p, TT_IDENTIFIER);
    ast_node_t* func_node = ast_func_definition_init(dt, func_name_token->loc, 'g', func_name_token->content, p->lex->filename);
    func_node->operator = hp->operator;
    func_node->unsafe = hp->unsafe;
    func_node->lowlvl_label = NULL;
    if (!strcmp(func_name_token->content, "constructor"))
    {
        if (p->current_blueprint == NULL)
            errorp(func_name_token->loc->row, func_name_token->loc->col, "constructor defined outside of blueprint");
        func_node->func_type = 'c';
        func_node->func_name = p->current_blueprint->bp_name;
        func_node->datatype = p->current_blueprint->bp_datatype;
        parser_ensure_cextern(p, "__libsgcllc_alloc_bytes", t_void, vector_init(DEFAULT_CAPACITY, DEFAULT_ALLOC_DELTA));
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
        datatype_t* pdt = parser_build_datatype(p, DTT_I32, NO_TERMINATOR, NULL);
        token_t* param_name_token = parser_expect_type(p, TT_IDENTIFIER);
        if (!parser_check(p, ')') && !parser_check(p, ','))
            parser_expect(p, ')');
        ast_node_t* lvar = map_put(p->lenv, param_name_token->content, ast_lvar_init(pdt, param_name_token->loc, param_name_token->content, NULL, p->lex->filename));
        lvar->voffset = i;
        vector_push(func_node->params, lvar);
    }
    func_node->func_label = make_func_label(p->lex->filename, func_node, p->current_blueprint);
    if (map_get(func_host_env, func_node->func_label))
        errorp(func_name_token->loc->row, func_name_token->loc->col, "function is identical to an already-defined function");
    map_put(func_host_env, func_node->func_label, func_node);
    vector_t* nonspecific_vec = map_get(p->funcs, func_node->func_name);
    int func_index;
    if (!nonspecific_vec)
    {
        map_put(p->funcs, func_node->func_name, vector_qinit(1, func_node));
        func_index = 0;
    }
    else
    {
        vector_push(nonspecific_vec, func_node);
        func_index = nonspecific_vec->size - 1;
    }
    if (!hp->lowlvl)
    {
        parser_expect(p, '{');
        ast_node_t* cf = p->current_func;
        p->current_func = func_node;
        parser_read_func_body(p);
        p->current_func = cf;
    }
    else
    {
        parser_expect(p, ';');
        int label_len = strlen(func_node->func_label);
        char* lowlvl_label = malloc(label_len + 1);
        memcpy(lowlvl_label, func_node->func_label, label_len);
        lowlvl_label[label_len] = '\0';
        for (int i = 0; i < label_len + 1; i++)
        {
            if (lowlvl_label[i] == '@')
                lowlvl_label[i] = '_';
        }
        parser_ensure_cextern(p, lowlvl_label, dt, func_node->params);
        func_node->lowlvl_label = lowlvl_label;
        if (!p->has_lowlvl)
        {
            buffer_t* pathbuffer = buffer_init(50, 15);
            int i = strlen(p->lex->path) - 1;
            for (; i >= 0 && (p->lex->path)[i] != '/' && (p->lex->path)[i] != '\\'; i--);
            if (i < 0)
                errorp(func_name_token->loc->row, func_name_token->loc->col, "ya path is really messed up boy");
            buffer_nstring(pathbuffer, p->lex->path, i + 1);
            buffer_string(pathbuffer, p->lex->filename);
            buffer_string(pathbuffer, "_lowlvl.o");
            buffer_append(pathbuffer, '\0');
            vector_push(p->links, buffer_export(pathbuffer));
            buffer_delete(pathbuffer);
            p->has_lowlvl = true;
        }
    }
    p->lenv = p->lenv->parent;
    if (p->lenv == p->genv)
        p->lenv = NULL;
    map_delete(func_env);
    free(hp);
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
        {
            if (p->current_func->func_type == 'c')
                vector_push(p->current_func->body->statements, ast_return_init(p->current_func->datatype, stmt->loc, vector_get(p->current_func->local_variables, 0), p->current_func));
            else
                vector_push(p->current_func->body->statements, ast_return_init(p->current_func->datatype, stmt->loc, ast_iliteral_init(p->current_func->datatype, stmt->loc, 0), p->current_func));
        }
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
    datatype_t* dt = parser_build_datatype(p, DTT_I32, NO_TERMINATOR, NULL);
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
    return ast_return_init(p->current_func->datatype, expr->loc, expr->datatype != p->current_func->datatype ? ast_cast_init(p->current_func->datatype, expr->loc, expr) : expr, p->current_func);
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
    parser_ensure_cextern(p, "__libsgcllc_delete_array", t_void, vector_init(DEFAULT_CAPACITY, DEFAULT_ALLOC_DELTA));
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
        return parser_read_if_statement(p, false);
    if (parser_is_header_statement(p, KW_WHILE))
        return parser_read_while_statement(p);
    if (parser_is_header_statement(p, KW_FOR))
        return parser_read_for_statement(p);
    if (parser_is_header_statement(p, KW_SWITCH))
        return parser_read_switch_statement(p);
    if (parser_is_header_statement(p, KW_BREAK))
        return parser_read_stub_statement(p, KW_BREAK, AST_BREAK);
    if (parser_is_header_statement(p, KW_CONTINUE))
        return parser_read_stub_statement(p, KW_CONTINUE, AST_CONTINUE);
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
        else if (token->id == KW_TRUE || token->id == KW_FALSE)
            vector_push(expr_result, token);
        else if (token->id == '(')
        {
            token_t* prev = parser_far_peek(p, -1);
            if (prev != NULL && prev->type == TT_IDENTIFIER && map_get(p->funcs, prev->content))
            {
                if (vector_top(stack) != NULL && (((token_t*) vector_top(stack))->id == OP_SELECTION || ((token_t*) vector_top(stack))->id == OP_SCOPE))
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
            datatype_t* type = parser_build_datatype(p, DTT_VOID, terminator, NULL);
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
            if (token->id >= KW_VOID && token->id <= KW_STRING)
                vector_push(expr_result, datatype_token_init(TT_DATATYPE, get_default_type(token->id), token->loc->offset, token->loc->row, token->loc->col));
            else
            {
                while (vector_top(stack) != NULL && precedence(token->id) > precedence(((token_t*) vector_top(stack))->id))
                    vector_push(expr_result, vector_pop(stack));
                vector_push(stack, token);
            }
        }
    }
    while (vector_top(stack) != NULL)
        vector_push(expr_result, vector_pop(stack));
    vector_delete(calls);
}

static ast_node_t* parser_find_operator_overload(parser_t* p, vector_t* args, datatype_t* rettype, token_t* op)
{
    vector_t* keys = map_keys(p->genv);
    ast_node_t* found = NULL;
    unsigned int lowest_conv = -1;
    for (int i = 0; i < keys->size; i++)
    {
        ast_node_t* node = map_get(p->genv, vector_get(keys, i));
        if (!node)
            continue;
        if (node->type != AST_FUNC_DEFINITION)
            continue;
        if (node->operator != op->id)
            continue;
        if (node->params->size != args->size)
            continue;
        if (rettype && !same_datatype(p, node->datatype, rettype))
            continue;
        int conversions = 0;
        debugf("comparing against: %s\n", node->func_label);
        for (int j = 0; j < args->size; j++)
        {
            ast_node_t* param = vector_get(node->params, j);
            ast_node_t* arg = vector_get(args, j);
            debugf("    comparing arg: %i, to %i\n", arg->datatype->type, param->datatype->type);
            if (!convertible_datatype(p, param->datatype, arg->datatype))
                goto try_again_overload;
            if (same_datatype(p, param->datatype, arg->datatype))
                continue;
            conversions++;
            if ((isfloattype(param->datatype->type) && !isfloattype(arg->datatype->type)) ||
                (!isfloattype(param->datatype->type) && isfloattype(arg->datatype->type)))
                conversions++;
        }
        if (conversions < lowest_conv)
        {
            lowest_conv = conversions;
            found = node;
        }
try_again_overload:
    }
    if (found)
        debugf("found operator overload: %s\n", found->func_label);
    return found ? ast_func_call_init(found->datatype, op->loc, found, args) : NULL;
}

static ast_node_t* parser_read_expr(parser_t* p, int terminator)
{
    vector_t* stack = vector_init(20, 10);
    vector_t* expr_result = vector_init(20, 10);
    parser_rpn(p, stack, expr_result, terminator);
    vector_clear(stack, RETAIN_OLD_CAPACITY);
    if (!expr_result->size)
        errorp(parser_peek(p)->loc->row, parser_peek(p)->loc->col, "null statement is not allowed");
    #ifdef SGCLLC_DEBUG
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
    #endif
    for (int i = 0; i < expr_result->size; i++)
    {
        token_t* token = (token_t*) vector_get(expr_result, i);
        if (token->type == TT_CHAR_LITERAL || token->type == TT_STRING_LITERAL || token->type == TT_NUMBER_LITERAL)
            vector_push(stack, ast_get_by_token(p, token));
        if (token->type == TT_DATATYPE)
            vector_push(stack, ast_stub_init(AST_STUB, token->dt, token->loc));
        if (token->type == TT_IDENTIFIER)
        {
            ast_node_t* found = NULL;
            ast_node_t* builtin = map_get(builtins, token->content);
            if (!builtin)
            {
                vector_t* flavors = map_get(p->funcs, token->content);
                if (!flavors)
                {
                    #define binop_chk(op, offset) (i + offset < expr_result->size && ((token_t*) vector_get(expr_result, i + offset))->id == op)
                    if (binop_chk(OP_SELECTION, 1))
                    {
                        ast_node_t* top = vector_top(stack);
                        ast_node_t* blueprint = map_get(p->lenv ? p->lenv : p->genv, top->datatype->name);
                        for (int i = 0; i < blueprint->inst_variables->size; i++)
                        {
                            ast_node_t* inst_var = vector_get(blueprint->inst_variables, i);
                            if (!strcmp(inst_var->var_name, token->content))
                            {
                                vector_push(stack, inst_var);
                                goto next_token;
                            }
                        }
                    }
                    if (binop_chk(OP_SCOPE, 2))
                    {
                        vector_push(stack, ast_namespace_init(token->loc, token->content));
                        continue;
                    }
                    #undef binop_chk
                    vector_push(stack, ast_get_by_token(p, token));
next_token:
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
                case KW_TRUE:
                case KW_FALSE:
                {
                    vector_push(stack, ast_get_by_token(p, token));
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
                case OP_EQUAL:
                case OP_NOT_EQUAL:
                case OP_GREATER:
                case OP_GREATER_EQUAL:
                case OP_LESS:
                case OP_LESS_EQUAL:
                case OP_AND:
                case OP_OR:
                case OP_XOR:
                case OP_ASSIGN_AND:
                case OP_ASSIGN_OR:
                case OP_ASSIGN_XOR:
                case OP_SHIFT_LEFT:
                case OP_SHIFT_RIGHT:
                case OP_SHIFT_URIGHT:
                case OP_ASSIGN_SHIFT_LEFT:
                case OP_ASSIGN_SHIFT_RIGHT:
                case OP_ASSIGN_SHIFT_URIGHT:
                case OP_LOGICAL_AND:
                case OP_LOGICAL_OR:
                case OP_SPACESHIP:
                case OP_SUBSCRIPT:
                {
                    ast_node_t* rhs = vector_pop(stack);
                    ast_node_t* lhs = vector_pop(stack);
                    if (!lhs || !rhs)
                        errorp(token->loc->row, token->loc->col, "expected 2 operands for operator %i", token->id);
                    if (lhs->datatype->type == DTT_LET) lhs->datatype = rhs->datatype;
                    if (rhs->type == AST_MAKE) lhs->datatype = rhs->datatype;
                    ast_node_t* overload = parser_find_operator_overload(p, vector_qinit(2, lhs, rhs), NULL, token);
                    if (overload)
                    {
                        vector_push(stack, overload);
                        break;
                    }
                    datatype_t* rettype = arith_conv(lhs->datatype, rhs->datatype);
                    if (token->id == OP_EQUAL ||
                        token->id == OP_NOT_EQUAL ||
                        token->id == OP_GREATER ||
                        token->id == OP_GREATER_EQUAL ||
                        token->id == OP_LESS ||
                        token->id == OP_LESS_EQUAL ||
                        token->id == OP_LOGICAL_AND ||
                        token->id == OP_LOGICAL_OR)
                        rettype = t_bool;
                    ast_node_type type = token->id;
                    if (type == OP_SPACESHIP)
                        type = OP_SUB;
                    else if (type == OP_SUBSCRIPT)
                    {
                        switch (lhs->datatype->type)
                        {
                            case DTT_ARRAY:
                                rettype = lhs->datatype->array_type;
                                break;
                            case DTT_STRING:
                                rettype = t_i8;
                                break;
                            default:
                                errorp(token->loc->row, token->loc->col, "subscript operator may not be applied to left hand side");
                        }
                    }
                    vector_push(stack, ast_binary_op_init(type, rettype, token->loc, lhs, rhs));
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
                case OP_CAST:
                {
                    ast_node_t* type = vector_pop(stack);
                    ast_node_t* castval = vector_pop(stack);
                    if (!type || !castval)
                        errorp(token->loc->row, token->loc->col, "expected 2 operands for cast operator");
                    ast_node_t* overload = parser_find_operator_overload(p, vector_qinit(1, castval), type->datatype, token);
                    if (overload)
                    {
                        vector_push(stack, overload);
                        break;
                    }
                    vector_push(stack, ast_cast_init(type->datatype, token->loc, castval));
                    break;
                }
                case OP_MAGNITUDE:
                case OP_NOT:
                case OP_COMPLEMENT:
                case OP_MINUS:
                case OP_PREFIX_INCREMENT:
                case OP_PREFIX_DECREMENT:
                case OP_POSTFIX_INCREMENT:
                case OP_POSTFIX_DECREMENT:
                case OP_ASM:
                {
                    ast_node_t* operand = vector_pop(stack);
                    if (!operand)
                        errorp(token->loc->row, token->loc->col, "expected operand for operator %i", token->id);
                    datatype_t* dt = operand->datatype;
                    if (token->id == OP_MAGNITUDE)
                    {
                        parser_ensure_cextern(p, "__libsgcllc_array_size", t_void, vector_init(DEFAULT_CAPACITY, DEFAULT_ALLOC_DELTA));
                        parser_ensure_cextern(p, "__libsgcllc_string_length", t_void, vector_init(DEFAULT_CAPACITY, DEFAULT_ALLOC_DELTA));
                        parser_ensure_cextern(p, "__libsgcllc_blueprint_size", t_void, vector_init(DEFAULT_CAPACITY, DEFAULT_ALLOC_DELTA));
                        dt = t_i64;
                    }
                    else if (token->id == OP_NOT)
                        dt = t_bool;
                    else if (token->id == OP_ASM)
                    {
                        if (p->current_func->unsafe == -2)
                            errorp(token->loc->row, token->loc->col, "asm operator may not be used inside a safe function");
                        if (dt->type != DTT_STRING)
                            errorp(token->loc->row, token->loc->col, "asm operator expected string literal");
                        dt = t_void;
                    }
                    ast_node_t* overload = parser_find_operator_overload(p, vector_qinit(1, operand), NULL, token);
                    if (token->id != OP_ASM && overload)
                    {
                        vector_push(stack, overload);
                        break;
                    }
                    vector_push(stack, ast_unary_op_init(token->id, dt, token->loc, operand));
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
                    parser_ensure_cextern(p, "__libsgcllc_dynamic_ndim_array", t_void, vector_init(DEFAULT_CAPACITY, DEFAULT_ALLOC_DELTA));
                    break;
                }
                case OP_SCOPE:
                {
                    ast_node_t* identifier = vector_pop(stack);
                    ast_node_t* ns = vector_pop(stack);
                    if (!identifier || !ns)
                        errorp(token->loc->row, token->loc->col, "expected 2 operands for scope operator");
                    vector_push(stack, ast_binary_op_init(token->id, identifier->datatype, token->loc, ns, identifier));
                    break;
                }
                case OP_TERNARY_Q:
                {
                    ast_node_t* els = vector_pop(stack);
                    ast_node_t* then = vector_pop(stack);
                    ast_node_t* cond = vector_pop(stack);
                    if (!els || !then || !cond)
                        errorp(token->loc->row, token->loc->col, "expected 3 operands for ternary operator");
                    vector_push(stack, ast_ternary_init(arith_conv(then->datatype, els->datatype), token->loc, cond, then, els));
                    break;
                }
                case OP_FUNC_CALL:
                {
                    ast_node_t* random_flavor = NULL;
                    ast_node_t* modifier = NULL;
                    ast_node_type ntype = -1;
                    for (int i = stack->size - 1; i >= 0; i--)
                    {
                        ast_node_t* node = vector_get(stack, i);
                        ntype = node->type;
                        if (node->type == AST_FUNC_DEFINITION)
                        {
                            random_flavor = node;
                            break;
                        }
                        if ((node->type == OP_SELECTION || node->type == OP_SCOPE) && node->rhs->type == AST_FUNC_DEFINITION)
                        {
                            random_flavor = node->rhs;
                            modifier = node->lhs;
                            break;
                        }
                    }
                    if (!random_flavor)
                        errorp(token->loc->row, token->loc->col, "no function name provided for function call");
                    vector_t* flavors = map_get(p->funcs, random_flavor->func_name);
                    ast_node_t* found = NULL;
                    if (random_flavor->extrn != 'b')
                    {
                        unsigned int lowest_conv = -1;
                        for (int i = 0; i < flavors->size; i++)
                        {
                            ast_node_t* flavor = vector_get(flavors, i);
                            if (ntype == OP_SCOPE && strcmp(modifier->ns_name, flavor->residing))
                                continue;
                            if (ntype != OP_SCOPE && strcmp(flavor->residing, p->lex->filename))
                                continue;
                            bool thisless = flavor->func_type == 'g' || flavor->func_type == 'c';
                            int thisless_size = flavor->params->size - (thisless ? 0 : 1);
                            int conversions = 0;
                            for (int j = thisless ? 0 : 1; j < flavor->params->size && vector_check_bounds(stack, stack->size - thisless_size + j); j++)
                            {
                                ast_node_t* param = vector_get(flavor->params, j);
                                ast_node_t* arg = vector_get(stack, stack->size - thisless_size + j);
                                if (!convertible_datatype(p, param->datatype, arg->datatype))
                                    goto try_again;
                                if (same_datatype(p, param->datatype, arg->datatype))
                                    continue;
                                conversions++;
                                if ((isfloattype(param->datatype->type) && !isfloattype(arg->datatype->type)) ||
                                    (!isfloattype(param->datatype->type) && isfloattype(arg->datatype->type)))
                                    conversions++;
                            }
                            if (conversions < lowest_conv)
                            {
                                lowest_conv = conversions;
                                found = flavor;
                            }
try_again:
                        }
                    }
                    else
                        found = random_flavor;
found_function:
                    if (!found)
                        errorp(token->loc->row, token->loc->col, "could not find a function by the name of '%s' that matched the specified args", random_flavor->func_name);
                    if (!strcmp(found->func_name, "_"))
                        errorp(token->loc->row, token->loc->col, "cannot call unnamed functions");
                    debugf("found function flavor: %s\n", found->func_label);
                    if (found->residing != NULL && strcmp(p->lex->filename, found->residing) && found->datatype->visibility != VT_PUBLIC)
                        errorp(token->loc->row, token->loc->col, "can't call a function that's private to '%s'", found->residing);
                    vector_t* args = vector_init(found->params->size, 1);
                    args->size = found->params->size;
                    bool thisless = found->func_type == 'g' || found->func_type == 'c';
                    int thisless_offset = !thisless ? 1 : 0;
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
                        args->data[0] = modifier;
                    vector_pop(stack); // pop func name
                    vector_push(stack, ast_func_call_init(found->datatype, token->loc, found, args));
                    break;
                }
            }
        }
    }
    if (!stack->size)
    {
        #ifdef SGCLLC_DEBUG
        printf("-- end of stack trace -\n");
        for (int i = 0; i < stack->size; i++)
            ast_print(vector_get(stack, i));
        printf("-^^^- stack trace -^^^-\n");
        #endif
        errorp(0, 0, "dev error: you messed up with the stack goofball");
    }
    ast_node_t* top = (ast_node_t*) vector_top(stack);
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