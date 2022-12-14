// Much of the code for this project is heavily inspired by https://github.com/rui314/8cc

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sgcllc.h"

map_t* keywords;
map_t* builtins;

void set_up_keywords(void)
{
    keywords = map_init(NULL, 100);
    #define keyword(id, name, settings) map_put(keywords, name, (void*) id);
    #include "keywords.inc"
    #undef keyword
}

bool chk_extension(char* path, int len)
{
    char extension[] = ".sgcll";
    if (len < 6)
        return false;
    for (int i = sizeof(extension) - 2, j = 1; i >= 0; i--, j++)
    {
        printf("%c = %c", path[len - j], extension[i]);
        if (path[len - j] != extension[i])
            return false;
    }
    return true;
}

int main(int argc, char** argv)
{
    if (argc != 2)
        errorc("only one file input is supported currently");
    set_up_keywords();
    set_up_builtins();
    char* path = argv[1];
    int pathl = strlen(path);
    if (!chk_extension(path, pathl))
        errorc("input file does not have extension .sgcll");
    FILE* file = fopen(path, "r");
    lexer_t* lexer = lex_init(file);
    while (!lex_eof(lexer))
        lex_read_token(lexer);
    for (int i = 0; i < lexer->output->size; i++)
    {
        token_t* token = vector_get(lexer->output, i);
        if (token_has_content(token))
            printf("%s ", token->content);
        else
            printf("%i ", token->id);
    }
    parser_t* parser = parser_init(lexer);
    while (!parser_eof(parser))
        parser_read(parser);
    ast_print(parser->nfile);
    char* assembly = calloc(pathl, sizeof(char));
    strcpy(assembly, path);
    assembly[pathl - 4] = '\0';
    FILE* out = fopen(assembly, "w");
    emitter_t* emitter = emitter_init(parser, out);
    emitter_emit(emitter);
    emitter_delete(emitter);
    fclose(out);
    parser_delete(parser);
    lex_delete(lexer);
    fclose(file);
    char link[1024];
    sprintf(link, "gcc -o a.exe %s builtin/builtin.o", assembly);
    system(link);
    free(assembly);
}