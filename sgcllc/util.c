#include <stdio.h>
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

char* isolate_filename(char* path)
{
    buffer_t* namebuf = buffer_init(30, 10);
    int k = 0;
    for (int i = strlen(path) - 1, started = false; i >= 0 && path[i] != '/' && path[i] != '\\'; i--)
    {
        if (!started)
        {
            if (path[i] == '.')
                started = true;
            continue;
        }
        buffer_append(namebuf, path[i]);
        k++;
    }
    char* name;
    if (namebuf->size)
    {
        buffer_append(namebuf, '\0');
        name = buffer_export(namebuf);
        for (int j = 0; j < k / 2; j++)
        {
            char temp = name[j];
            name[j] = name[k - j - 1];
            name[k - j - 1] = temp;
        }
    }
    else
        name = path;
    buffer_delete(namebuf);
    return name;
}

// I/O utils

void impl_writestr(char* str, FILE* out)
{
    if (!str)
    {
        fputc('\0', out);
        return;
    }
    fwrite(str, sizeof(char), strlen(str), out);
    fputc('\0', out);
}

char impl_read(FILE* in)
{
    if (feof(in))
        errorc("corrupted file (line %i)", __LINE__);
    char c;
    fread(&c, sizeof(char), 1, in);
    return c;
}

short impl_readi16(FILE* in)
{
    if (feof(in))
        errorc("corrupted file (line %i)", __LINE__);
    short s;
    fread(&s, sizeof(short), 1, in);
    return s;
}

int impl_readi32(FILE* in)
{
    if (feof(in))
        errorc("corrupted file (line %i)", __LINE__);
    int i;
    fread(&i, sizeof(int), 1, in);
    return i;
}

char* impl_readstr(FILE* in)
{
    if (feof(in))
        errorc("corrupted file (line %i)", __LINE__);
    buffer_t* buffer = buffer_init(1024, 64);
    for (char c = fgetc(in); c; c = fgetc(in))
    {
        buffer_append(buffer, c);
        if (feof(in)) errorc("corrupted file (line %i)", __LINE__);
    }
    buffer_append(buffer, '\0');
    if (buffer->size == 1)
        return NULL;
    return buffer_export(buffer);
}

bool fexists(char* path)
{
    FILE* exists = fopen(path, "rb");
    if (!exists || feof(exists) || ferror(exists))
    {
        fclose(exists);
        return false;
    }
    fclose(exists);
    return true;
}

// i'm lazy: https://stackoverflow.com/questions/3407012/rounding-up-to-the-nearest-multiple-of-a-number
int round_up(int num, int multiple)
{
    int remainder = num % multiple;
    return multiple && remainder ? num + multiple - remainder : num;
}