#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "sgcllc.h"

vector_t* vector_init(int capacity, int alloc_delta)
{
    vector_t* vec = calloc(1, sizeof(vector_t));
    vec->data = malloc(sizeof(void*) * capacity);
    vec->size = 0;
    vec->capacity = capacity;
    vec->alloc_delta = alloc_delta;
    return vec;
}

vector_t* vector_qinit(int count, ...)
{
    vector_t* vec = calloc(1, sizeof(vector_t));
    vec->data = malloc(sizeof(void*) * count);
    vec->size = vec->capacity = count;
    vec->alloc_delta = DEFAULT_ALLOC_DELTA;
    va_list args;
    va_start(args, count);
    for (int i = 0; i < count; i++)
        vector_set(vec, i, va_arg(args, void*));
    va_end(args);
    return vec;
}

void* vector_push(vector_t* vec, void* element)
{
    if (vec->size >= vec->capacity)
        vec->data = realloc(vec->data, sizeof(void*) * (vec->capacity += vec->alloc_delta));
    vec->data[vec->size++] = element;
    return element;
}

void* vector_pop(vector_t* vec)
{
    void* element = vec->data[--vec->size];
    if (vec->size < vec->capacity / 2)
        vec->data = realloc(vec->data, sizeof(void*) * (vec->capacity /= 2));
    return element;
}

void* vector_get(vector_t* vec, int index)
{
    if (index < 0 || index >= vec->size)
        return NULL;
    return vec->data[index];
}

void* vector_set(vector_t* vec, int index, void* element)
{
    if (index < 0 || index >= vec->size)
        return NULL;
    return vec->data[index] = element;
}

void* vector_top(vector_t* vec)
{
    if (vec->size <= 0)
        return NULL;
    return vec->data[vec->size - 1];
}

void vector_clear(vector_t* vec, int capacity)
{
    if (capacity >= 0)
        vec->data = realloc(vec->data, sizeof(void*) * (vec->capacity = capacity));
    vec->size = 0;
}

void vector_concat(vector_t* vec, vector_t* other)
{
    if (vec->size + other->size >= vec->capacity)
        vec->data = realloc(vec->data, sizeof(void*) * (vec->capacity += other->size));
    memcpy(vec->data + vec->size, other->data, sizeof(void*) * other->size);
    vec->size += other->size;
}

bool vector_check_bounds(vector_t* vec, int index)
{
    return index >= 0 && index < vec->size;
}

void vector_delete(vector_t* vec)
{
    free(vec->data);
    free(vec);
}