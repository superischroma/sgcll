#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "sgcllc.h"

// source: https://github.com/rui314/8cc/blob/master/map.c
static unsigned int hash(char *p)
{
    unsigned int r = 2166136261;
    for (; *p; p++)
    {
        r ^= *p;
        r *= 16777619;
    }
    return r;
}

map_t* map_init(map_t* parent, int capacity)
{
    map_t* map = calloc(1, sizeof(map_t));
    map->key = calloc(capacity, sizeof(char*));
    map->value = calloc(capacity, sizeof(void*));
    map->size = 0;
    map->capacity = capacity;
    map->parent = parent;
    return map;
}

static void map_chk_rehash(map_t* map)
{
    if (map->size < map->capacity * 0.35)
        return;
    char** nkey = calloc(map->capacity * 2, sizeof(char*));
    void** nvalue = calloc(map->capacity * 2, sizeof(void*));
    for (int j = 0; j < map->capacity; j++)
    {
        if (map->key[j] == NULL)
            continue;
        for (int i = hash(map->key[j]) % (map->capacity * 2);; i++)
        {
            if (i >= map->capacity) i = 0;
            if (nkey[i] != NULL)
                continue;
            nkey[i] = map->key[j];
            nvalue[i] = map->value[j];
            break;
        }
    }
    map->capacity *= 2;
    free(map->key);
    map->key = nkey;
    free(map->value);
    map->value = nvalue;
}

void* map_put(map_t* map, char* k, void* v)
{
    map_chk_rehash(map);
    for (int i = hash(k) % map->capacity;; i++)
    {
        if (i >= map->capacity) i = 0;
        if (map->key[i] != NULL && !strcmp(map->key[i], k))
        {
            map->value[i] = v;
            break;
        }
        if (map->key[i] != NULL)
            continue;
        map->key[i] = k;
        map->value[i] = v;
        map->size++;
        break;
    }
    return v;
}

void* map_get_local(map_t* map, char* k)
{
    for (int i = hash(k) % map->capacity; map->key[i] != NULL; i++)
    {
        if (i >= map->capacity) i = 0;
        if (!strcmp(map->key[i], k))
            return map->value[i];
    }
    return NULL;
}

void* map_get(map_t* map, char* k)
{
    void* val = map_get_local(map, k);
    if (val)
        return val;
    if (!map->parent)
        return NULL;
    return map_get(map->parent, k);
}

// does not deallocate memory at K
bool map_erase(map_t* map, char* k)
{
    for (int i = hash(k) % map->capacity; map->key[i] != NULL; i++)
    {
        if (i >= map->capacity) i = 0;
        if (!strcmp(map->key[i], k))
        {
            map->key[i] = map->value[i] = NULL;
            return true;
        }
    }
    return false;
}

void map_delete(map_t* map)
{
    if (!map) return;
    free(map->key);
    free(map->value);
    free(map);
}