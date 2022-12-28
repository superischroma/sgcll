#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#include "sgcllc.h"

#define RESET 0
#define BRIGHT 1
#define DIM 2
#define UNDERLINE 3
#define BLINK 4
#define REVERSE	7
#define HIDDEN 8

#define BLACK 0
#define RED 1
#define GREEN 2
#define YELLOW 3
#define BLUE 4
#define MAGENTA 5
#define CYAN 6
#define	WHITE 7

#define chgcolor(file, fg, bg, attr) fprintf(file, "%c[%d;%d;%dm", 0x1B, attr, fg + 30, bg + 40)
#define resetcolor(file) chgcolor(file, WHITE, BLACK, RESET)

void errorf(int row, int col, const char* what, char* fmt, va_list args)
{
    fprintf(stderr, "sgcllc: ");
    chgcolor(stderr, RED, BLACK, BRIGHT);
    fprintf(stderr, "%s error ", what);
    resetcolor(stderr);
    fprintf(stderr, "{");
    chgcolor(stderr, CYAN, BLACK, DIM);
    fprintf(stderr, "%i", row);
    chgcolor(stderr, WHITE, BLACK, DIM);
    fprintf(stderr, "|");
    chgcolor(stderr, CYAN, BLACK, DIM);
    fprintf(stderr, "%i", col);
    resetcolor(stderr);
    fprintf(stderr, "}: ");
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

void errore(int row, int col, char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    errorf(row, col, "emitter", fmt, args);
    va_end(args);
    exit(EXIT_FAILURE);
}

void errorc(char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    errorf(0, 0, "compiler", fmt, args);
    va_end(args);
    exit(EXIT_FAILURE);
}

void warnf(int row, int col, char* fmt, ...)
{
    printf("sgcllc: ");
    chgcolor(stdout, YELLOW, BLACK, DIM);
    printf("warning ");
    resetcolor(stdout);
    fprintf(stdout, "{");
    chgcolor(stdout, CYAN, BLACK, DIM);
    fprintf(stdout, "%i", row);
    chgcolor(stdout, WHITE, BLACK, DIM);
    fprintf(stdout, "|");
    chgcolor(stdout, CYAN, BLACK, DIM);
    fprintf(stdout, "%i", col);
    resetcolor(stdout);
    printf("}: ");
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
}

void debugf(char* fmt, ...)
{
    #ifdef SGCLLC_DEBUG
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    #endif
}