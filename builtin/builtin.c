#include <windows.h>

#define __BUILTIN_DEBUG

#define __builtin_abs(x) ((x) < 0 ? -(x) : (x))
#define __builtin_fmod(x, y) (x - y * (int) (x / y))

#define __builtin_varadic NULL; \
    __asm__("   leaq 32(%rbp), %rax\n" \
            "   movq %rax, -8(%rbp)")


int __builtin_i32_dec_itos(int n, char* buffer)
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

int __builtin_f32_dec_ftos(float f, char* buffer, int precision)
{
    int ipart = (int) f;
    int i = __builtin_i32_dec_itos(ipart, buffer);
    buffer[i++] = '.';
    float dec = (f - ipart) * 10.0f;
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

void __builtin_i32_println(int i)
{
    char buffer[34];
    int c = __builtin_i32_dec_itos(i, buffer);
    buffer[c] = '\n';
    buffer[c + 1] = '\0';
    WriteFile(GetStdHandle((DWORD) -11), buffer, c + 1, 0, NULL);
}

void __builtin_f32_println(float f)
{
    char buffer[80];
    int c = __builtin_f32_dec_ftos(f, buffer, 6);
    buffer[c] = '\n';
    buffer[c + 1] = '\0';
    WriteFile(GetStdHandle((DWORD) -11), buffer, c + 1, 0, NULL);
}

void __builtin_string_println(char* str)
{
    WriteFile(GetStdHandle((DWORD) -11), str, __builtin_string_length(str), 0, NULL);
    WriteFile(GetStdHandle((DWORD) -11), "\n", 1, 0, NULL);
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

double __builtin_dd_fmod(double x, double y)
{
    return __builtin_fmod(x, y);
}

double __builtin_ds_fmod(double x, float y)
{
    return __builtin_fmod(x, y);
}

double __builtin_sd_fmod(float x, double y)
{
    return __builtin_fmod(x, y);
}

float __builtin_ss_fmod(float x,  float y)
{
    return __builtin_fmod(x, y);
}

void __builtin_init()
{
    int i = 0;
    __asm__("stmxcsr -4(%rbp)\n"
	"   andl $0xF3FF, -4(%rbp)\n"
	"   orl $0x6000, -4(%rbp)\n"
	"   ldmxcsr -4(%rbp)\n");
}