#include <stdlib.h>

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

void vector_delete(vector_t* vec)
{
    free(vec->data);
    free(vec);
}