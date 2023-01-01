#include <windows.h>

#include "builtin.h"

#define __BUILTIN_DEBUG

gc_node_t* gc_root = NULL;

int __builtin_i64_dec_itos(long long n, char* buffer, size_t radix)
{
    int i, sign;

    if ((sign = n) < 0)
        n = -n;
    i = 0;
    do
    {
        int a = __builtin_abs(n % radix);
        if (a > 9)
            a += '7';
        else
            a += '0';
        buffer[i++] = a;
    }
    while (n /= radix);
    if (sign < 0)
        buffer[i++] = '-';
    buffer[i] = '\0';
    for (int j = 0; j < i / 2; j++)
    {
        char temp = buffer[j];
        buffer[j] = buffer[i - j - 1];
        buffer[i - j - 1] = temp;
    }
    return i;
}

int __builtin_f64_dec_ftos(double d, char* buffer, int precision)
{
    int ipart = (int) d;
    double fpart = d - ipart;
    int i = __builtin_i64_dec_itos(ipart, buffer, 10);
    if (fpart != (int) fpart)
        buffer[i++] = '.';
    for (; fpart != (int) fpart && precision > 0; fpart -= (int) fpart, precision--)
    {
        fpart *= 10.0;
        buffer[i++] = (int) fpart + '0';
    }
    buffer[i] = '\0';
    return i;
}

int __builtin_string_length(char* str)
{
    int i = 0;
    for (; str[i]; i++);
    return i;
}

void __builtin_i64_println(long long ll, size_t radix)
{
    char buffer[34];
    int c = __builtin_i64_dec_itos(ll, buffer, radix);
    buffer[c] = '\n';
    buffer[c + 1] = '\0';
    WriteFile(GetStdHandle((DWORD) -11), buffer, c + 1, 0, NULL);
}

void __builtin_f64_println(double d)
{
    char buffer[80];
    int c = __builtin_f64_dec_ftos(d, buffer, 6);
    buffer[c] = '\n';
    buffer[c + 1] = '\0';
    WriteFile(GetStdHandle((DWORD) -11), buffer, c + 1, 0, NULL);
}

void __builtin_string_print(char* str)
{
    WriteFile(GetStdHandle((DWORD) -11), str, __builtin_string_length(str), 0, NULL);
}

void __builtin_string_println(char* str)
{
    WriteFile(GetStdHandle((DWORD) -11), str, __builtin_string_length(str), 0, NULL);
    WriteFile(GetStdHandle((DWORD) -11), "\n", 1, 0, NULL);
}

void* __builtin_alloc_bytes(size_t amount)
{
    void* mem = HeapAlloc(GetProcessHeap(), 0, amount);
    gc_node_t* node = HeapAlloc(GetProcessHeap(), 0, sizeof(gc_node_t));
    node->mem = mem;
    node->next = gc_root;
    gc_root = node;
    return mem;
}

bool __builtin_delete_bytes_no_gc(void* mem)
{
    return HeapFree(GetProcessHeap(), 0, mem);
}

void* __builtin_dynamic_array(size_t length, size_t element_width)
{
    char* array = __builtin_alloc_bytes(length * element_width + 1);
    array[0] = element_width;
    return array + 1;
}

static void* __builtin_dynamic_ndim_array_recur(size_t element_width, size_t dc, unsigned long long* dimensions)
{
    void** array = __builtin_dynamic_array(*dimensions, dc == 1 ? element_width : 8);
    if (dc > 1)
    {
        for (int i = 0; i < *dimensions; i++)
            array[i] = __builtin_dynamic_ndim_array_recur(element_width, dc - 1, dimensions + 1);
    }
    return array;
}

void* __builtin_dynamic_ndim_array(size_t element_width, size_t dc, ...)
{
    unsigned long long* args = __builtin_varadic;
    return __builtin_dynamic_ndim_array_recur(element_width, dc, args);
}

static void __builtin_delete_array_recur(void* array, size_t dc, unsigned long long* dimensions)
{
    if (dc > 1)
    {
        for (int i = 0; i < *dimensions; i++)
            __builtin_delete_array_recur(((void**) array)[i], dc - 1, dimensions + 1);
    }
    bool result = __builtin_delete_bytes_no_gc((char*) array - 1);
    #ifdef __BUILTIN_DEBUG
    if (result)
        __builtin_string_println("[builtin debug] deallocated array");
    else
        __builtin_string_println("[builtin debug] failed to deallocate array");
    #endif
}

void __builtin_delete_array(void* array, size_t dc, ...)
{
    unsigned long long* args = __builtin_varadic;
    __builtin_delete_array_recur(array, dc, args);
}

size_t __builtin_array_size(void* array)
{
    return HeapSize(GetProcessHeap(), 0, array - 1) / *((char*) array - 1);
}

size_t __builtin_blueprint_size(void* obj)
{
    return HeapSize(GetProcessHeap(), 0, obj);
}

void __builtin_copy_memory(void* dest, const void* src, int length)
{
    CopyMemory(dest, src, length);
}

void __builtin_init()
{
    _mm_setcsr((_mm_getcsr() & 0xF3FF) | 0x6000); // set rounding mode
}

void __builtin_gc_finalize()
{
    for (gc_node_t* node = gc_root; node;)
    {
        gc_node_t* next = node->next;
        void* mem = node->mem;
        bool result = __builtin_delete_bytes_no_gc(node->mem);
        __builtin_delete_bytes_no_gc(node);
        #ifdef __BUILTIN_DEBUG
        if (result)
            __builtin_string_print("[builtin debug] deallocated memory at 0x");
        else
            __builtin_string_print("[builtin debug] failed to deallocate memory at 0x");
        __builtin_i64_println((UINT_PTR) mem, 16);
        #endif
        node = next;
    }
}