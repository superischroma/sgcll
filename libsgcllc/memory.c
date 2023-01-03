#include <windows.h>

#include "libsgcllc.h"

void* __libsgcllc_alloc_bytes(size_t amount)
{
    void* mem = HeapAlloc(GetProcessHeap(), 0, amount);
    gc_node_t* node = HeapAlloc(GetProcessHeap(), 0, sizeof(gc_node_t));
    node->mem = mem;
    node->next = gc_root;
    gc_root = node;
    return mem;
}

BOOL __libsgcllc_delete_bytes_no_gc(void* mem)
{
    return HeapFree(GetProcessHeap(), 0, mem);
}

void* __libsgcllc_dynamic_array(size_t length, size_t element_width)
{
    char* array = __libsgcllc_alloc_bytes(length * element_width + 1);
    array[0] = element_width;
    return array + 1;
}

static void* __libsgcllc_dynamic_ndim_array_recur(size_t element_width, size_t dc, unsigned long long* dimensions)
{
    void** array = __libsgcllc_dynamic_array(*dimensions, dc == 1 ? element_width : 8);
    if (dc > 1)
    {
        for (int i = 0; i < *dimensions; i++)
            array[i] = __libsgcllc_dynamic_ndim_array_recur(element_width, dc - 1, dimensions + 1);
    }
    return array;
}

void* __libsgcllc_dynamic_ndim_array(size_t element_width, size_t dc, ...)
{
    unsigned long long* args = __libsgcllc_varadic;
    return __libsgcllc_dynamic_ndim_array_recur(element_width, dc, args);
}

static void __libsgcllc_delete_array_recur(void* array, size_t dc, unsigned long long* dimensions)
{
    if (dc > 1)
    {
        for (int i = 0; i < *dimensions; i++)
            __libsgcllc_delete_array_recur(((void**) array)[i], dc - 1, dimensions + 1);
    }
    BOOL result = __libsgcllc_delete_bytes_no_gc((char*) array - 1);
    #ifdef __libsgcllc_DEBUG
    if (result)
        __libsgcllc_string_println("[builtin debug] deallocated array");
    else
        __libsgcllc_string_println("[builtin debug] failed to deallocate array");
    #endif
}

void __libsgcllc_delete_array(void* array, size_t dc, ...)
{
    unsigned long long* args = __libsgcllc_varadic;
    __libsgcllc_delete_array_recur(array, dc, args);
}

size_t __libsgcllc_array_size(void* array)
{
    return HeapSize(GetProcessHeap(), 0, array - 1) / *((char*) array - 1);
}

size_t __libsgcllc_blueprint_size(void* obj)
{
    return HeapSize(GetProcessHeap(), 0, obj);
}

void __libsgcllc_copy_memory(void* dest, const void* src, int length)
{
    CopyMemory(dest, src, length);
}