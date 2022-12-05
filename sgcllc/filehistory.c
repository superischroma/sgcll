#include <stdlib.h>

#include "sgcllc.h"

static filehistory_element_t* filehistory_element_init(int c, filehistory_element_t* behind, filehistory_element_t* ahead)
{
    filehistory_element_t* fse = calloc(1, sizeof(filehistory_element_t));
    fse->c = c;
    fse->behind = behind;
    fse->ahead = ahead;
    return fse;
}

filehistory_t* filehistory_init()
{
    filehistory_t* fs = calloc(1, sizeof(filehistory_t));
    fs->front = fs->back = NULL;
    fs->size = 0;
    return fs;
}

int filehistory_enqueue(filehistory_t* fs, int c)
{
    fs->back = filehistory_element_init(c, NULL, fs->back);
    if (!fs->size)
        fs->front = fs->back;
    else
        fs->back->ahead->behind = fs->back;
    fs->size++;
    return c;
}

int filehistory_serve(filehistory_t* fs)
{
    if (!fs->front)
        return -2;
    filehistory_element_t* served = fs->front;
    int served_val = fs->front->c;
    fs->front = fs->front->behind;
    free(served);
    fs->size--;
    if (!fs->size)
        fs->back = NULL;
    return served_val;
}

void filehistory_delete(filehistory_t* fs)
{
    for (filehistory_element_t* fse = fs->front; fse != NULL; fse = fse->behind)
        free(fse);
    free(fs);
}