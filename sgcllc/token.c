#include <stdlib.h>

#include "sgcllc.h"

token_t* content_token_init(token_type type, char* content, int offset, int row, int col)
{
    token_t* token = calloc(1, sizeof(token_t));
    token->type = type;
    token->content = content;
    token->loc = calloc(1, sizeof(location_t));
    token->loc->offset = offset;
    token->loc->row = row;
    token->loc->col = col;
    return token;
}

token_t* id_token_init(token_type type, int id, int offset, int row, int col)
{
    token_t* token = calloc(1, sizeof(token_t));
    token->type = type;
    token->id = id;
    token->loc = calloc(1, sizeof(location_t));
    token->loc->offset = offset;
    token->loc->row = row;
    token->loc->col = col;
    return token;
}

void token_delete(token_t* token)
{
    free(token->loc);
    free(token);
}