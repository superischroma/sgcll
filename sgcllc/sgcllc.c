// Much of the code for this project is heavily inspired by https://github.com/rui314/8cc

#include <stdio.h>

#include "sgcllc.h"

map_t* keywords;

void set_up_keywords(void)
{
    keywords = map_init(NULL, 100);
    map_put(keywords, "void", (void*) KW_VOID);
    map_put(keywords, "bool", (void*) KW_BOOL);
    map_put(keywords, "i8", (void*) KW_I8);
    map_put(keywords, "i16", (void*) KW_I16);
    map_put(keywords, "i32", (void*) KW_I32);
    map_put(keywords, "i64", (void*) KW_I64);
    map_put(keywords, "f32", (void*) KW_F32);
    map_put(keywords, "f64", (void*) KW_F64);
    map_put(keywords, "return", (void*) KW_RETURN);
    map_put(keywords, "import", (void*) KW_IMPORT);
}

int main(int argc, char** argv)
{
    set_up_keywords();
    FILE* file = fopen("test/helloworld.sgcll", "r");
    lexer_t* lexer = lex_init(file);
    while (!lex_eof(lexer))
        lex_read_token(lexer);
    parser_t* parser = parser_init(lexer);
    while (!parser_eof(parser))
        parser_read(parser);
    lex_delete(lexer);
    fclose(file);
}