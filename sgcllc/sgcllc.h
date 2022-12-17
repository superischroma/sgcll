#ifndef SGCLC_H
#define SGCLC_H

#include <stdio.h>
#include <stdbool.h>

/* Typedefs */

typedef int token_type, ast_node_type, datatype_type, visibility_type, register_type;

/* Macros */

/* Token Type */

#define TT_IDENTIFIER 0
#define TT_NUMBER_LITERAL 1
#define TT_CHAR_LITERAL 2
#define TT_STRING_LITERAL 3
#define TT_KEYWORD 4
#define TT_DATATYPE 5

/* Datatype Type */

#define DTT_VOID 0
#define DTT_BOOL 1
#define DTT_I8 2
#define DTT_I16 3
#define DTT_I32 4
#define DTT_I64 5
#define DTT_F32 6
#define DTT_F64 7
#define DTT_LET 8
#define DTT_STRING 9
#define DTT_ARRAY 10

/* Visibility Type */

#define VT_PRIVATE 0
#define VT_PUBLIC 1
#define VT_PROTECTED 2

/* vector_t Helpful Macros */

#define DEFAULT_CAPACITY 10
#define DEFAULT_ALLOC_DELTA 5
#define RETAIN_OLD_CAPACITY -1

/* Register Types */
#define REG_A 'a'
#define REG_B 'b'
#define REG_C 'c'
#define REG_D 'd'
#define REG_SRC 'o'
#define REG_DEST 't'
#define REG_STACK_BASE 'e'
#define REG_STACK_TOP 's'
#define REG_8 '8'
#define REG_9 '9'
#define REG_10 '0'
#define REG_11 '1'
#define REG_12 '2'
#define REG_13 '3'
#define REG_14 '4'
#define REG_15 '5'
#define REG_FLOAT 'x'

/* Functional Macros */
#define min(a, b) (a < b ? a : b)
#define max(a, b) (a > b ? a : b)

enum {
    KW_LPAREN = '(',
    KW_RPAREN = ')',
    KW_LBRACE = '{',
    KW_RBRACE = '}',
    KW_LBRACK = '[',
    KW_RBRACK = ']',
    KW_SEMICOLON = ';',
    KW_COMMA = ',',
    OP_ASSIGN = '=',
    OP_ADD = '+',
    OP_SUB = '-',
    OP_MUL = '*',
    OP_DIV = '/',
    OP_MOD = '%',
    AST_FILE = 256,
    AST_STUB,
    AST_IMPORT,
    AST_FUNC_DEFINITION,
    AST_FUNC_CALL,
    AST_LVAR,
    AST_GVAR,
    AST_BLOCK,
    AST_ILITERAL,
    AST_FLITERAL,
    AST_SLITERAL,
    AST_RETURN,
    AST_DELETE,
    AST_IF,
    AST_WHILE,
    AST_CAST,
    AST_MAKE,
    #define keyword(id, name, _) id,
    #include "keywords.inc"
    #undef keyword 
};

/* Structs */

typedef struct
{
    void** data;
    int size;
    int capacity;
    int alloc_delta;
} vector_t;

typedef struct
{
    FILE* file;
    int row;
    int col;
    int offset;
    int peek;
    vector_t* output;
} lexer_t;

typedef struct
{
    int offset;
    int row;
    int col;
} location_t;

typedef struct datatype_t datatype_t;

typedef struct
{
    token_type type;
    location_t* loc;
    union
    {
        int id;
        char* content;
        datatype_t* dt;
    };
} token_t;

typedef struct
{
    char* data;
    int size;
    int capacity;
    int alloc_delta;
} buffer_t;

typedef struct ast_node_t ast_node_t;

typedef struct datatype_t
{
    visibility_type visibility;
    datatype_type type;
    int size;
    bool usign;
    union
    {
        // DTT_OBJECT (soon...)
        char* name;
        // DTT_ARRAY
        struct
        {
            int depth;
            struct datatype_t* array_type;
            ast_node_t* length;
        };
    };
} datatype_t;

typedef struct ast_node_t
{
    ast_node_type type;
    datatype_t* datatype;
    location_t* loc;
    union
    {
        // AST_ILITERAL
        long long ivalue;
        // AST_FLITERAL
        struct
        {
            double fvalue;
            char* flabel;
        };
        // AST_SLITERAL
        struct
        {
            char* svalue;
            char* slabel;
        };
        // AST_LVAR/AST_GVAR
        struct
        {
            char* var_name;
            // AST_LVAR
            int lvoffset;
            // AST_GVAR
            char* gvlabel;
            struct ast_node_t* gvinit;
        };
        // AST_BINARY_OP
        struct
        {
            struct ast_node_t* lhs;
            struct ast_node_t* rhs;
        };
        // AST_UNARY_OP
        struct ast_node_t* operand;
        // AST_FUNC_DEFINITION
        struct
        {
            char* func_name;
            vector_t* params;
            vector_t* local_variables;
            bool extrn;
            struct ast_node_t* body;
        };
        // AST_FUNC_CALL
        struct
        {
            struct ast_node_t* func;
            vector_t* args;
        };
        // AST_IF
        struct
        {
            struct ast_node_t* if_cond;
            struct ast_node_t* if_then;
            struct ast_node_t* if_els;
        };
        // AST_FILE
        struct
        {
            vector_t* decls;
            vector_t* imports;
        };
        // AST_WHILE
        struct
        {
            struct ast_node_t* while_cond;
            struct ast_node_t* while_then;
        };
        // AST_IMPORT
        char* path;
        // AST_RETURN
        struct ast_node_t* retval;
        // AST_DELETE
        struct ast_node_t* delsym;
        // AST_BLOCK
        vector_t* statements;
        // AST_CAST
        struct ast_node_t* castval;
    };
} ast_node_t;

typedef struct map_t
{
    char** key;
    void** value;
    int size;
    int capacity;
    struct map_t* parent;
} map_t;

typedef struct parser_t
{
    lexer_t* lex;
    ast_node_t* nfile;
    map_t* genv;
    map_t* lenv;
    map_t* labels;
    int oindex; // current index in the token stream
    vector_t* userexterns;
    vector_t* cexterns;
    vector_t* links;
    ast_node_t* current_func;
} parser_t;

typedef struct emitter_t
{
    parser_t* p;
    FILE* out;
    int stackoffset;
    int stackmax;
    int itmp;
    int ftmp;
    bool control;
} emitter_t;

/* sgcllc.c */

extern map_t* keywords;
extern map_t* builtins;

vector_t* build(char* path);

/* lex.c */

lexer_t* lex_init(FILE* file);
bool lex_eof(lexer_t* lex);
void lex_read_token(lexer_t* lex);
void lex_delete(lexer_t* lex);
token_t* lex_get(lexer_t* lex, int index);

/* buffer.c */

buffer_t* buffer_init(int capacity, int alloc_delta);
char buffer_append(buffer_t* buffer, char c);
char* buffer_export(buffer_t* buffer);
void buffer_delete(buffer_t* buffer);
char buffer_get(buffer_t* buffer, int index);

/* util.c */

bool is_alphanumeric(int c);
bool token_has_content(token_t* token);
char* unwrap_string_literal(char* slit);
void indprintf(int indent, const char* fmt, ...);
bool isfloattype(datatype_type dtt);
int itos(int n, char* buffer);
void systemf(const char* fmt, ...);

/* token.c */

token_t* content_token_init(token_type type, char* content, int offset, int row, int col);
token_t* id_token_init(token_type type, int id, int offset, int row, int col);
token_t* datatype_token_init(token_type type, datatype_t* dt, int offset, int row, int col);
void token_delete(token_t* token);

/* vector.c */

vector_t* vector_init(int capacity, int alloc_delta);
vector_t* vector_qinit(int count, ...);
void* vector_push(vector_t* vec, void* element);
void* vector_pop(vector_t* vec);
void* vector_get(vector_t* vec, int index);
void* vector_set(vector_t* vec, int index, void* element);
void* vector_top(vector_t* vec);
void vector_clear(vector_t* vec, int capacity);
void vector_concat(vector_t* vec, vector_t* other);
void vector_delete(vector_t* vec);

/* error.c */

void errorl(lexer_t* lex, char* fmt, ...);
void errorp(int row, int col, char* fmt, ...);
void errore(int row, int col, char* fmt, ...);
void errorc(char* fmt, ...);

/* map.c */

map_t* map_init(map_t* parent, int capacity);
void* map_put(map_t* map, char* k, void* v);
void* map_get_local(map_t* map, char* k);
void* map_get(map_t* map, char* k);
bool map_erase(map_t* map, char* k);
void map_delete(map_t* map);

/* ast.c */

ast_node_t* ast_file_init(location_t* loc);
ast_node_t* ast_import_init(location_t* loc, char* path);
ast_node_t* ast_func_definition_init(datatype_t* dt, location_t* loc, char* func_name);
ast_node_t* ast_builtin_init(datatype_t* dt, char* func_name, vector_t* params);
ast_node_t* ast_lvar_init(datatype_t* dt, location_t* loc, char* lvar_name);
ast_node_t* ast_iliteral_init(datatype_t* dt, location_t* loc, long long ivalue);
ast_node_t* ast_sliteral_init(datatype_t* dt, location_t* loc, char* svalue, char* slabel);
ast_node_t* ast_binary_op_init(ast_node_type type, datatype_t* dt, location_t* loc, ast_node_t* lhs, ast_node_t* rhs);
ast_node_t* ast_func_call_init(datatype_t* dt, location_t* loc, ast_node_t* func, vector_t* args);
ast_node_t* ast_fliteral_init(datatype_t* dt, location_t* loc, double fvalue, char* flabel);
ast_node_t* ast_return_init(datatype_t* dt, location_t* loc, ast_node_t* retval);
ast_node_t* ast_cast_init(datatype_t* dt, location_t* loc, ast_node_t* castval);
ast_node_t* ast_if_init(datatype_t* dt, location_t* loc, ast_node_t* if_cond);
ast_node_t* ast_while_init(datatype_t* dt, location_t* loc, ast_node_t* while_cond);
ast_node_t* ast_delete_init(datatype_t* dt, location_t* loc, ast_node_t* delsym);
ast_node_t* ast_stub_init(datatype_t* dt, location_t* loc);
ast_node_t* ast_make_init(datatype_t* dt, location_t* loc);
void ast_print(ast_node_t* node);

/* parser.c */

parser_t* parser_init(lexer_t* lex);
void parser_make_header(parser_t* p, FILE* out);
bool parser_eof(parser_t* p);
token_t* parser_read(parser_t* p);
void parser_delete(parser_t* p);
void set_up_builtins(void);
char* make_label(parser_t* p, void* content);

/* emitter.c */

emitter_t* emitter_init(parser_t* p, FILE* out, bool control);
void emitter_emit(emitter_t* e);
emitter_t* emitter_delete(emitter_t* e);

/* header.c */

void write_header(FILE* out, map_t* genv);
vector_t* read_header(FILE* in);

#endif