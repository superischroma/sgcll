// Much of the code for this project is heavily inspired by https://github.com/rui314/8cc

#include <stdio.h>

#include "sgcllc.h"

int main(int argc, char** argv)
{
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