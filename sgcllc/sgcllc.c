// Much of the code for this project is heavily inspired by https://github.com/rui314/8cc

#include <stdio.h>

#include "sgcllc.h"

map_t* keywords;

void set_up_keywords(void)
{
    keywords = map_init(NULL, 100);
    #define keyword(id, name, settings) map_put(keywords, name, (void*) id);
    #include "keywords.inc"
    #undef keyword
}

int main(int argc, char** argv)
{
    set_up_keywords();
    FILE* file = fopen("test/assignment.sgcll", "r");
    lexer_t* lexer = lex_init(file);
    while (!lex_eof(lexer))
        lex_read_token(lexer);
    parser_t* parser = parser_init(lexer);
    while (!parser_eof(parser))
        parser_read(parser);
    lex_delete(lexer);
    fclose(file);
}