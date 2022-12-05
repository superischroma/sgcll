#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "sgcllc.h"

lexer_t* lex_init(FILE* file)
{
    lexer_t* lex = calloc(1, sizeof(lexer_t));
    lex->file = file;
    lex->history = filehistory_init();
    lex->row = 1;
    lex->col = 0;
    lex->offset = 0;
    lex->output = vector_init(50, 20);
    return lex;
}

static int lex_read(lexer_t* lex)
{
    int c;
    if (lex->history->size)
        c = filehistory_serve(lex->history);
    else
        c = fgetc(lex->file);
    if (c == '\n')
    {
        lex->row++;
        lex->col = 0;
    }
    else
        lex->col++;
    lex->offset++;
    return c;
}

static int lex_peek(lexer_t* lex)
{
    return filehistory_enqueue(lex->history, fgetc(lex->file));
}

bool lex_eof(lexer_t* lex)
{
    return feof(lex->file);
}

void lex_read_token(lexer_t* lex)
{
    int c = lex_read(lex);
    switch (c)
    {
        case 'a' ... 'z':
        case 'A' ... 'Z':
        {
            buffer_t* buffer = buffer_init(5, 5);
            buffer_append(buffer, c);
            while (1)
            {
                int c = lex_peek(lex);
                if (!is_alphanumeric(c) && c != '_')
                    break;
                buffer_append(buffer, (char) lex_read(lex));
            }
            buffer_append(buffer, '\0');
            vector_push(lex->output, content_token_init(TT_IDENTIFIER, buffer_export(buffer), lex->offset, lex->row, lex->col));
            buffer_delete(buffer);
            break;
        }
        case '0' ... '9':
        {
            buffer_t* buffer = buffer_init(5, 5);
            buffer_append(buffer, c);
            if (lex_peek(lex) == 'x')
            {
                buffer_append(buffer, 'x');
                lex_read(lex);
            }
            while (1)
            {
                int c = lex_peek(lex);
                if ((c < '0' || c > '9') && c != '.')
                    break;
                buffer_append(buffer, (char) lex_read(lex));
            }
            buffer_append(buffer, '\0');
            vector_push(lex->output, content_token_init(TT_NUMBER_LITERAL, buffer_export(buffer), lex->offset, lex->row, lex->col));
            buffer_delete(buffer);
            break;
        }
        case '"':
        {
            buffer_t* buffer = buffer_init(5, 5);
            buffer_append(buffer, c);
            for (int escaping = 0;;)
            {
                int c = lex_peek(lex);
                if (c == '"' && !escaping)
                {
                    buffer_append(buffer, (char) lex_read(lex));
                    break;
                }
                escaping = 0;
                if (c == '\\')
                    escaping = 1;
                buffer_append(buffer, (char) lex_read(lex));
            }
            buffer_append(buffer, '\0');
            vector_push(lex->output, content_token_init(TT_STRING_LITERAL, buffer_export(buffer), lex->offset, lex->row, lex->col));
            buffer_delete(buffer);
            break;
        }
        case '\'':
        {
            buffer_t* buffer = buffer_init(3, 0);
            buffer_append(buffer, c);
            buffer_append(buffer, (char) lex_read(lex));
            if (lex_peek(lex) != '\'')
                errorl(lex, "closing single quote expected directly after character constant");
            buffer_append(buffer, (char) lex_read(lex));
            buffer_append(buffer, '\0');
            vector_push(lex->output, content_token_init(TT_CHAR_LITERAL, buffer_export(buffer), lex->offset, lex->row, lex->col));
            buffer_delete(buffer);
            break;
        }
        case '(':
        case ')':
        case '{':
        case '}':
        case ';':
        {
            vector_push(lex->output, id_token_init(TT_PUNCTUATOR, c, lex->offset, lex->row, lex->col));
            break;
        }
        case ' ':
        case '\n':
        case '\t':
        case '\r':
        case EOF:
            break;
        default:
        {
            errorl(lex, "encountered invalid token");
            break;
        }
    }
}

void lex_delete(lexer_t* lex)
{
    filehistory_delete(lex->history);
    vector_delete(lex->output);
    free(lex);
}

token_t* lex_get(lexer_t* lex, int index)
{
    return (token_t*) vector_get(lex->output, index);
}