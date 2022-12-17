#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "sgcllc.h"

lexer_t* lex_init(FILE* file)
{
    lexer_t* lex = calloc(1, sizeof(lexer_t));
    lex->file = file;
    lex->peek = '\0';
    lex->row = 1;
    lex->col = 0;
    lex->offset = 0;
    lex->output = vector_init(50, 20);
    return lex;
}

static int lex_read(lexer_t* lex)
{
    int c;
    if (lex->peek)
    {
        c = lex->peek;
        lex->peek = '\0';
    }
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
    return lex->peek ? lex->peek : (lex->peek = fgetc(lex->file));
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
        case '_':
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
            char* ident = buffer_export(buffer);
            int id = (intptr_t) map_get(keywords, ident);
            if (id)
                vector_push(lex->output, id_token_init(TT_KEYWORD, id, lex->offset, lex->row, lex->col));
            else
                vector_push(lex->output, content_token_init(TT_IDENTIFIER, ident, lex->offset, lex->row, lex->col));
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
            for (int signature = 0;;)
            {
                printf("current: %c\n", c);
                int c = lex_peek(lex);
                printf("next: %c\n", c);
                if ((c < '0' || c > '9') && c != '.')
                {
                    if (c == 'f' ||
                        c == 'F' ||
                        c == 'd' ||
                        c == 'D' ||
                        c == 'l' ||
                        c == 'L' ||
                        c == 'u' ||
                        c == 'U' ||
                        c == 's' ||
                        c == 'S' ||
                        c == 'b' ||
                        c == 'B')
                    {
                        signature++;
                        buffer_append(buffer, (char) lex_read(lex));
                        continue;
                    }
                    else
                    {
                        if (signature > 2)
                            errorl(lex, "type signature for number literal cannot contain more than 2 characters");
                        else if (signature == 2)
                        {
                            char first = buffer_get(buffer, buffer->size - 2);
                            if (first != 'u' && first != 'U')
                                errorl(lex, "the first type in 2-character type signature should be unsigned");
                            char second = buffer_get(buffer, buffer->size - 1);
                            if (second == 'u' || second == 'U')
                                errorl(lex, "the second type in 2-character type signature cannot be unsigned, instead try: %c%c", second, first);
                        }
                        break;
                    }
                }
                if (signature)
                    errorl(lex, "number cannot continue after type signature");
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
        case KW_LPAREN:
        case KW_RPAREN:
        case KW_RBRACE:
        case KW_LBRACK:
        case KW_RBRACK:
        case KW_SEMICOLON:
        case KW_COMMA:
        case OP_ASSIGN:
        {
            vector_push(lex->output, id_token_init(TT_KEYWORD, c, lex->offset, lex->row, lex->col));
            break;
        }
        case KW_LBRACE:
        {
            token_t* prev = vector_top(lex->output);
            if (prev != NULL && token_has_content(prev))
                c = OP_MAKE_SIZE;
            vector_push(lex->output, id_token_init(TT_KEYWORD, c, lex->offset, lex->row, lex->col));
            break;
        }
        case OP_ADD:
        case OP_SUB:
        case OP_MUL:
        case OP_MOD:
        {
            if (lex_peek(lex) == '=')
            {
                switch (c)
                {
                    case OP_ADD: c = OP_ASSIGN_ADD; break;
                    case OP_SUB: c = OP_ASSIGN_SUB; break;
                    case OP_MUL: c = OP_ASSIGN_MUL; break;
                    case OP_MOD: c = OP_ASSIGN_MOD; break;
                }
                lex_read(lex);
            }
            vector_push(lex->output, id_token_init(TT_KEYWORD, c, lex->offset, lex->row, lex->col));
            break;
        }
        case OP_DIV:
        {
            int next = lex_peek(lex);
            if (next == OP_DIV) // single-line comment
            {
                lex_read(lex);
                for (;;)
                {
                    int c = lex_read(lex);
                    if (c == '\n' || c == EOF)
                        break;
                }
                break;
            }
            if (next == OP_MUL)
            {
                lex_read(lex);
                for (;;)
                {
                    int c = lex_read(lex);
                    if (c == EOF)
                        errorl(lex, "unterminated block comment");
                    if (c == OP_MUL)
                    {
                        if (lex_peek(lex) == OP_DIV)
                        {
                            lex_read(lex);
                            break;
                        }
                    }
                }
                break;
            }
            if (lex_peek(lex) == '=')
            {
                c = OP_ASSIGN_DIV;
                lex_read(lex);
            }
            vector_push(lex->output, id_token_init(TT_KEYWORD, c, lex->offset, lex->row, lex->col));
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
    if (!lex) return;
    vector_delete(lex->output);
    free(lex);
}

token_t* lex_get(lexer_t* lex, int index)
{
    return (token_t*) vector_get(lex->output, index);
}