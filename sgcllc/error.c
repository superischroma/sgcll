#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#include "sgcllc.h"

void errorf(int row, int col, const char* what, char* fmt, va_list args)
{
    fprintf(stderr, "sgcllc: %s error (%i:%i): ", what, row, col);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
}

void errorl(lexer_t* lex, char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    errorf(lex->row, lex->col, "lexer", fmt, args);
    va_end(args);
    exit(EXIT_FAILURE);
}

void errorp(int row, int col, char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    errorf(row, col, "parser", fmt, args);
    va_end(args);
    exit(EXIT_FAILURE);
}