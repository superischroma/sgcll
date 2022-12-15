#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>

#include "sgcllc.h"

bool is_alphanumeric(int c)
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool isfloattype(datatype_type dtt)
{
    return dtt == DTT_F32 || dtt == DTT_F64;
}

bool token_has_content(token_t* token)
{
    return token != NULL && (token->type == TT_IDENTIFIER || token->type == TT_STRING_LITERAL || token->type == TT_NUMBER_LITERAL);
}

char* unwrap_string_literal(char* slit)
{
    int len = strlen(slit);
    char* unwrapped = malloc(len - 1);
    memcpy(unwrapped, slit + 1, len - 2);
    unwrapped[len - 2] = '\0';
    return unwrapped;
}

void indprintf(int indent, const char* fmt, ...)
{
    for (int i = 0; i < indent; i++)
        printf("  ");
    va_list args;
    va_start(args, fmt);
    vfprintf(stdout, fmt, args);
    va_end(args);
}

int itos(int n, char* buffer)
{
    int i, sign;

    if ((sign = n) < 0)
        n = -n;
    i = 0;
    do
        buffer[i++] = __builtin_abs(n % 10) + '0';
    while (n /= 10);
    if (sign < 0)
        buffer[i++] = '-';
    buffer[i] = '\0';
    for (int j = 0; j < i / 2; j++)
    {
        char temp = buffer[j];
        buffer[j] = buffer[i - j - 1];
        buffer[i - j - 1] = temp;
    }
    return i;
}

void systemf(const char* fmt, ...)
{
    char buffer[strlen(fmt) + 512];
    va_list args;
    va_start(args, fmt);
    vsprintf(buffer, fmt, args);
    va_end(args);
    system(buffer);
}