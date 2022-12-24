#include <windows.h>

#include "builtin.h"

#define __BUILTIN_DEBUG

int __builtin_i64_dec_itos(long long n, char* buffer)
{
    int i, sign;

    if ((sign = n) < 0)
        n = -n;
    i = 0;
    do
        buffer[i++] = __builtin_abs(n % 10) + '0';
    while (n /= 10);
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
    int i = __builtin_i64_dec_itos(ipart, buffer);
    buffer[i++] = '.';
    double dec = (d - ipart) * 10.0f;
    for (int j = 0; j < precision; i++, j++, dec = (dec * 10.0f) - ((int) dec) * 10)
        buffer[i] = (int) dec + '0';
    buffer[i] = '\0';
    return i;
}

int __builtin_string_length(char* str)
{
    int i = 0;
    for (; str[i]; i++);
    return i;
}

void __builtin_i64_println(long long ll)
{
    char buffer[34];
    int c = __builtin_i64_dec_itos(ll, buffer);
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

void __builtin_string_println(char* str)
{
    WriteFile(GetStdHandle((DWORD) -11), str, __builtin_string_length(str), 0, NULL);
    WriteFile(GetStdHandle((DWORD) -11), "\n", 1, 0, NULL);
}

void* __builtin_alloc_bytes(size_t amount)
{
    return HeapAlloc(GetProcessHeap(), 0, amount);
}

void* __builtin_dynamic_array(size_t length, size_t element_width)
{
    char* array = HeapAlloc(GetProcessHeap(), 0, length * element_width + 1);
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
    WINBOOL result = HeapFree(GetProcessHeap(), 0, (char*) array - 1);
    #ifdef __BUILTIN_DEBUG
    if (result)
        __builtin_string_println("[builtin debug] deallocated array");
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

void __builtin_copy_memory(void* dest, const void* src, int length)
{
    CopyMemory(dest, src, length);
}

void __builtin_init()
{
    int i = 0;
    __asm__("stmxcsr -4(%rbp)\n"
	"   andl $0xF3FF, -4(%rbp)\n"
	"   orl $0x6000, -4(%rbp)\n"
	"   ldmxcsr -4(%rbp)\n");
}