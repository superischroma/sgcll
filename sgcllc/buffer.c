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

char* buffer_nstring(buffer_t* buffer, char* str, int len)
{
    if (buffer->size + len >= buffer->capacity)
        buffer->data = realloc(buffer->data, buffer->capacity += len);
    memcpy(buffer->data + buffer->size, str, len);
    buffer->size += len;
    return str;
}

char* buffer_string(buffer_t* buffer, char* str)
{
    return buffer_nstring(buffer, str, strlen(str));
}

long long buffer_int(buffer_t* buffer, long long ll)
{
    char ibuf[33];
    itos(ll, ibuf);
    buffer_string(buffer, ibuf);
    return ll;
}

char* buffer_export(buffer_t* buffer)
{
    char* export = malloc(buffer->size);
    memcpy(export, buffer->data, buffer->size);
    return export;
}

char buffer_get(buffer_t* buffer, int index)
{
    if (index < 0 || index >= buffer->size)
        return '\0';
    return buffer->data[index];
}

void buffer_delete(buffer_t* buffer)
{
    free(buffer->data);
    free(buffer);
}