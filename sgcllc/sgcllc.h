#ifndef SGCLC_H
#define SGCLC_H

#include <stdio.h>
#include <stdbool.h>

/* Typedefs */

typedef int token_type, ast_node_type, datatype_type, visibility_type;

/* Macros */

#define TT_IDENTIFIER 0
#define TT_OPERATOR 1
#define TT_NUMBER_LITERAL 2
#define TT_CHAR_LITERAL 3
#define TT_STRING_LITERAL 4
#define TT_KEYWORD 5

#define P_LPARENTHESIS '('
#define P_RPARENTHESIS ')'
#define P_LBRACE '{'
#define P_RBRACE '}'
#define P_SEMICOLON ';'

#define DTT_VOID 0
#define DTT_BOOL 1
#define DTT_I8 2
#define DTT_I16 3
#define DTT_I32 4
#define DTT_I64 5
#define DTT_F32 6
#define DTT_F64 7
#define DTT_ARRAY 8
#define DTT_PTR 9
#define DTT_FUNCTION 10
#define DTT_STRING 11

#define VT_PRIVATE 0
#define VT_PUBLIC 1
#define VT_PROTECTED 2

enum {
    AST_FILE = 256,
    AST_IMPORT,
    AST_FUNC_DEFINITION,
    AST_LVAR,
    AST_GVAR,
    AST_BLOCK,
    #define keyword(id, name, _) id,
    #include "keywords.inc"
    #undef keyword 
};

/* Structs */

typedef struct filehistory_element_t
{
    int c;
    struct filehistory_element_t* behind;
    struct filehistory_element_t* ahead;
} filehistory_element_t;

typedef struct
{
    filehistory_element_t* front;
    filehistory_element_t* back;
    int size;
} filehistory_t;

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
    filehistory_t* history;
    int row;
    int col;
    int offset;
    vector_t* output;
} lexer_t;

typedef struct
{
    int offset;
    int row;
    int col;
} location_t;

typedef struct
{
    token_type type;
    location_t* loc;
    union
    {
        int id;
        char* content;
    };
} token_t;

typedef struct
{
    char* data;
    int size;
    int capacity;
    int alloc_delta;
} buffer_t;

typedef struct datatype_t
{
    visibility_type visibility;
    datatype_type type;
    int size;
    bool usign;
    struct datatype_t* ptr_type; // array/pointer
    int length; // array
    struct datatype_t* ret_type; // function
    vector_t* parameters;
} datatype_t;

typedef struct ast_node_t
{
    ast_node_type type;
    datatype_t* datatype;
    location_t* loc;
    union
    {
        // i8/i16/i32/i64
        long long ivalue;
        // f32/f64
        struct
        {
            double fvalue;
            char* flabel;
        };
        // string
        struct
        {
            char* svalue;
            char* slabel;
        };
        // local/global variable
        struct
        {
            char* var_name;
            // local
            int lvoffset;
            // global
            char* gvlabel;
        };
        // binary operator
        struct
        {
            struct ast_node_t* lhs;
            struct ast_node_t* rhs;
        };
        // unary operator
        struct
        {
            struct ast_node_t* operand;
        };
        // function call/definition
        struct
        {
            char* func_name;
            // function call
            vector_t* args;
            struct datatype_t* func_type;
            // function definition
            vector_t* params;
            vector_t* local_variables;
            struct ast_node_t* body;
        };
        // initializer
        struct
        {
            struct ast_node_t* initval;
            datatype_t* dest_type;
        };
        // if statement/ternary operator
        struct
        {
            struct ast_node_t* condition;
            struct ast_node_t* then;
            struct ast_node_t* otherwise;
        };
        // AST_FILE
        struct
        {
            vector_t* decls;
            vector_t* imports;
        };
        // AST_IMPORT
        struct
        {
            char* path;
        };
        // return value
        struct ast_node_t* retval;
        // AST_BLOCK
        vector_t* statements;
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
    ast_node_t* file;
    map_t* genv;
    map_t* lenv;
    int oindex; // current index in the token stream
} parser_t;

/* sgcllc.c */

extern map_t* keywords;

void set_up_keywords(void);

/* lex.c */

lexer_t* lex_init(FILE* file);
bool lex_eof(lexer_t* lex);
void lex_read_token(lexer_t* lex);
void lex_delete(lexer_t* lex);
token_t* lex_get(lexer_t* lex, int index);

/* filehistory.c */

filehistory_t* filehistory_init();
int filehistory_enqueue(filehistory_t* fs, int c);
int filehistory_serve(filehistory_t* fs);
void filehistory_delete(filehistory_t* fs);

/* buffer.c */

buffer_t* buffer_init(int capacity, int alloc_delta);
char buffer_append(buffer_t* buffer, char c);
char* buffer_export(buffer_t* buffer);
void buffer_delete(buffer_t* buffer);

/* util.c */

bool is_alphanumeric(int c);
bool token_has_content(token_t* token);
char* unwrap_string_literal(char* slit);
void indprintf(int indent, const char* fmt, ...);

/* token.c */

token_t* content_token_init(token_type type, char* content, int offset, int row, int col);
token_t* id_token_init(token_type type, int id, int offset, int row, int col);
void token_delete(token_t* token);

/* vector.c */

vector_t* vector_init(int capacity, int alloc_delta);
void* vector_push(vector_t* vec, void* element);
void* vector_pop(vector_t* vec);
void* vector_get(vector_t* vec, int index);
void* vector_set(vector_t* vec, int index, void* element);
void vector_delete(vector_t* vec);

/* error.c */

void errorl(lexer_t* lex, char* fmt, ...);
void errorp(int row, int col, char* fmt, ...);

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
ast_node_t* ast_lvar_init(datatype_t* dt, location_t* loc, char* lvar_name);
void ast_print(ast_node_t* node);

/* parser.c */

parser_t* parser_init(lexer_t* lex);
bool parser_eof(parser_t* p);
token_t* parser_read(parser_t* p);

#endif