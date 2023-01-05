#ifndef SGCLC_H
#define SGCLC_H

#include <stdio.h>
#include <stdbool.h>

/* Typedefs */

typedef int token_type, ast_node_type, datatype_type, visibility_type, register_type, gc_node_type;

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
#define DTT_OBJECT 11

/* Visibility Type */

#define VT_PRIVATE 0
#define VT_PUBLIC 1
#define VT_PROTECTED 2

/* Debug Flag */

#define SGCLLC_DEBUG

/* vector_t Helpful Macros */

#define DEFAULT_CAPACITY 10
#define DEFAULT_ALLOC_DELTA 5
#define RETAIN_OLD_CAPACITY -1

#define DEFAULT_VECTOR vector_init(DEFAULT_CAPACITY, DEFAULT_ALLOC_DELTA)

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

#define read impl_read(in)
#define readi16 impl_readi16(in)
#define readi32 impl_readi32(in)
#define readstr impl_readstr(in)

#define write(c) fputc(c, out)
#define writei16(c) fwrite(&(c), sizeof(short), 1, out)
#define writei32(c) fwrite(&(c), sizeof(int), 1, out)
#define writestr(str) impl_writestr(str, out)

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
    OP_AND = '&',
    OP_OR = '|',
    OP_XOR = '^',
    OP_MUL = '*',
    OP_DIV = '/',
    OP_MOD = '%',
    OP_NOT = '!',
    OP_SUBSCRIPT = '[',
    OP_MAGNITUDE = '#',
    OP_SELECTION = '.',
    OP_GREATER = '>',
    OP_LESS = '<',
    OP_COMPLEMENT = '~',
    OP_TERNARY_Q = '?',
    OP_TERNARY_C = ':',
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
    AST_FOR,
    AST_CAST,
    AST_MAKE,
    AST_BLUEPRINT,
    AST_NAMESPACE,
    AST_TERNARY,
    AST_SWITCH,
    AST_CASE,
    AST_BREAK,
    AST_CONTINUE,
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
    char* filename;
    char* path;
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
        // DTT_OBJECT
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
    char* residing;
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
            struct ast_node_t* vinit;
            // AST_LVAR
            int voffset;
            // AST_GVAR
            char* gvlabel;
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
            char func_type;
            char* func_name;
            char* func_label;
            vector_t* params;
            vector_t* local_variables;
            char extrn;
            char* lowlvl_label;
            struct ast_node_t* body;
            int operator;
            char* end_label;
            int unsafe;
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
        // AST_FOR
        struct
        {
            struct ast_node_t* for_init;
            struct ast_node_t* for_cond;
            struct ast_node_t* for_post;
            struct ast_node_t* for_then;
        };
        // AST_IMPORT
        char* path;
        // AST_RETURN
        struct
        {
            struct ast_node_t* retval;
            struct ast_node_t* retfunc;
        };
        // AST_DELETE
        struct ast_node_t* delsym;
        // AST_BLOCK
        vector_t* statements;
        // AST_CAST
        struct ast_node_t* castval;
        // AST_BLUEPRINT
        struct
        {
            vector_t* inst_variables;
            vector_t* methods;
            char* bp_name;
            datatype_t* bp_datatype;
            int bp_size;
        };
        // AST_NAMESPACE
        char* ns_name;
        // AST_TERNARY
        struct
        {
            struct ast_node_t* tern_cond;
            struct ast_node_t* tern_then;
            struct ast_node_t* tern_els;
        };
        // AST_SWITCH
        struct
        {
            struct ast_node_t* cmp;
            vector_t* cases;
        };
        // AST_CASE
        struct
        {
            vector_t* case_conditions; // null if default
            struct ast_node_t* case_then;
            char* case_label;
        };
    };
} ast_node_t;

typedef struct gc_node_t
{
    ast_node_t* ast_equiv;
    vector_t* parents;
    vector_t* children;
} gc_node_t;

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
    map_t* funcs;
    int oindex; // current index in the token stream
    vector_t* userexterns;
    vector_t* cexterns;
    vector_t* links;
    ast_node_t* current_func;
    ast_node_t* current_blueprint;
    ast_node_t* current_block;
    char* entry;
    bool has_lowlvl;
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
    char* fp_negate_label;
} emitter_t;

typedef struct options_t
{
    vector_t* import_search_paths;
    char* fdlibm_path;
} options_t;

/* sgcllc.c */

extern map_t* keywords;
extern map_t* builtins;
extern options_t* options;

vector_t* build(char* path);

/* lex.c */

lexer_t* lex_init(FILE* file, char* path);
bool lex_eof(lexer_t* lex);
void lex_read_token(lexer_t* lex);
void lex_delete(lexer_t* lex);
token_t* lex_get(lexer_t* lex, int index);

/* buffer.c */

buffer_t* buffer_init(int capacity, int alloc_delta);
char buffer_append(buffer_t* buffer, char c);
char* buffer_nstring(buffer_t* buffer, char* str, int len);
char* buffer_string(buffer_t* buffer, char* str);
long long buffer_int(buffer_t* buffer, long long ll);
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
char* isolate_filename(char* path);
void impl_writestr(char* str, FILE* out);
char impl_read(FILE* in);
short impl_readi16(FILE* in);
int impl_readi32(FILE* in);
char* impl_readstr(FILE* in);
bool fexists(char* path);
int round_up(int num, int multiple);

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
bool vector_check_bounds(vector_t* vec, int index);
void vector_delete(vector_t* vec);
void vector_concat(vector_t* vec, vector_t* other);

/* log.c */

void errorl(lexer_t* lex, char* fmt, ...);
void errorp(int row, int col, char* fmt, ...);
void errore(int row, int col, char* fmt, ...);
void errorc(char* fmt, ...);
void warnf(int row, int col, char* fmt, ...);
void debugf(char* fmt, ...);

/* map.c */

map_t* map_init(map_t* parent, int capacity);
void* map_put(map_t* map, char* k, void* v);
void* map_get_local(map_t* map, char* k);
void* map_get(map_t* map, char* k);
vector_t* map_keys(map_t* map);
bool map_erase(map_t* map, char* k);
void map_delete(map_t* map);

/* ast.c */

ast_node_t* ast_file_init(location_t* loc);
ast_node_t* ast_import_init(location_t* loc, char* path);
ast_node_t* ast_func_definition_init(datatype_t* dt, location_t* loc, char func_type, char* func_name, char* residing);
ast_node_t* ast_builtin_init(datatype_t* dt, char* func_name, vector_t* params, char* residing, char extrn);
ast_node_t* ast_lvar_init(datatype_t* dt, location_t* loc, char* lvar_name, ast_node_t* vinit, char* residing);
ast_node_t* ast_iliteral_init(datatype_t* dt, location_t* loc, long long ivalue);
ast_node_t* ast_sliteral_init(datatype_t* dt, location_t* loc, char* svalue, char* slabel);
ast_node_t* ast_binary_op_init(ast_node_type type, datatype_t* dt, location_t* loc, ast_node_t* lhs, ast_node_t* rhs);
ast_node_t* ast_func_call_init(datatype_t* dt, location_t* loc, ast_node_t* func, vector_t* args);
ast_node_t* ast_fliteral_init(datatype_t* dt, location_t* loc, double fvalue, char* flabel);
ast_node_t* ast_return_init(datatype_t* dt, location_t* loc, ast_node_t* retval, ast_node_t* retfunc);
ast_node_t* ast_cast_init(datatype_t* dt, location_t* loc, ast_node_t* castval);
ast_node_t* ast_if_init(datatype_t* dt, location_t* loc, ast_node_t* if_cond);
ast_node_t* ast_while_init(datatype_t* dt, location_t* loc, ast_node_t* while_cond);
ast_node_t* ast_for_init(datatype_t* dt, location_t* loc, ast_node_t* for_init, ast_node_t* for_cond, ast_node_t* for_post);
ast_node_t* ast_delete_init(datatype_t* dt, location_t* loc, ast_node_t* delsym);
ast_node_t* ast_stub_init(ast_node_type type, datatype_t* dt, location_t* loc);
ast_node_t* ast_make_init(datatype_t* dt, location_t* loc);
ast_node_t* ast_unary_op_init(ast_node_type type, datatype_t* dt, location_t* loc, ast_node_t* operand);
ast_node_t* ast_blueprint_init(location_t* loc, char* bp_name, datatype_t* dt);
ast_node_t* ast_namespace_init(location_t* loc, char* ns_name);
ast_node_t* ast_ternary_init(datatype_t* dt, location_t* loc, ast_node_t* cond, ast_node_t* then, ast_node_t* els);
ast_node_t* ast_switch_init(location_t* loc, ast_node_t* cmp);
ast_node_t* ast_case_init(location_t* loc);
void ast_print(ast_node_t* node);
void print_datatype(datatype_t* dt);

/* parser.c */

extern datatype_t* t_void, * t_bool, * t_i8, * t_i16, * t_i32, * t_i64, * t_ui8, * t_ui16,
    * t_ui32, * t_ui64, * t_f32, * t_f64, * t_string, * t_array, * t_object;

parser_t* parser_init(lexer_t* lex);
void parser_make_header(parser_t* p, FILE* out);
bool parser_eof(parser_t* p);
void parser_read(parser_t* p);
void parser_delete(parser_t* p);
void set_up_builtins(void);
void parser_ensure_cextern(parser_t* p, char* name, datatype_t* dt, vector_t* args);
char* make_label(parser_t* p, void* content);
char* make_func_label(char* filename, ast_node_t* func, ast_node_t* current_blueprint);
datatype_t* arith_conv(datatype_t* t1, datatype_t* t2);

/* emitter.c */

emitter_t* emitter_init(parser_t* p, FILE* out, bool control);
void emitter_emit(emitter_t* e);
emitter_t* emitter_delete(emitter_t* e);

/* header.c */

void write_header(FILE* out, map_t* genv);
vector_t* read_header(FILE* in, char* filename);

/* options.c */

void write_options(options_t* options, FILE* out);
options_t* read_options(FILE* in);

#endif