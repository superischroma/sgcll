#include <stdlib.h>
#include <string.h>

#include "sgcllc.h"

buffer_t* buffer_init(int capacity, int alloc_delta)
{
    buffer_t* buffer = calloc(1, sizeof(buffer_t));
    buffer->data = malloc(capacity);
    buffer->size = 0;
    buffer->capacity = capacity;
    buffer->alloc_delta = alloc_delta;
    return buffer;
}

char buffer_append(buffer_t* buffer, char c)
{
    if (buffer->size >= buffer->capacity)
        buffer->data = realloc(buffer->data, buffer->capacity += buffer->alloc_delta);
    buffer->data[buffer->size++] = c;
    return c;
}

char* buffer_export(buffer_t* buffer)
{
    char* export = malloc(buffer->size);
    memcpy(export, buffer->data, buffer->size);
    return export;
}

void buffer_delete(buffer_t* buffer)
{
    free(buffer->data);
    free(buffer);
}