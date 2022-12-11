// Much of the code for this project is heavily inspired by https://github.com/rui314/8cc

#include <stdio.h>
#include <stdlib.h>

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
    FILE* file = fopen("test/math.sgcll", "r");
    lexer_t* lexer = lex_init(file);
    while (!lex_eof(lexer))
        lex_read_token(lexer);
    for (int i = 0; i < lexer->output->size; i++)
    {
        token_t* token = vector_get(lexer->output, i);
        if (token_has_content(token))
            printf("%s ", token->content);
        else
            printf("%c ", token->id);
    }
    parser_t* parser = parser_init(lexer);
    while (!parser_eof(parser))
        parser_read(parser);
    ast_print(parser->nfile);
    FILE* out = fopen("test/math.s", "w");
    emitter_t* emitter = emitter_init(parser, out);
    emitter_emit(emitter);
    emitter_delete(emitter);
    fclose(out);
    parser_delete(parser);
    lex_delete(lexer);
    fclose(file);
    system("gcc -o test/math.exe test/math.s builtin/builtin.o");
}