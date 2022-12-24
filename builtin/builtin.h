#ifndef __SGCLL_BUILTIN_H
#define __SGCLL_BUILTIN_H

typedef unsigned long size_t;

#define __builtin_abs(x) ((x) < 0 ? -(x) : (x))
#define __builtin_fmod(x, y) (x - y * (int) (x / y))

// only use this if you know what you're doing!!
#define __builtin_varadic NULL; \
    __asm__("   leaq 32(%rbp), %rax\n" \
            "   movq %rax, -8(%rbp)")

int __builtin_i64_dec_itos(long long n, char* buffer);
int __builtin_f64_dec_ftos(double d, char* buffer, int precision);
int __builtin_string_length(char* str);
void __builtin_i64_println(long long ll);
void __builtin_f64_println(double d);
void __builtin_string_println(char* str);
void* __builtin_alloc_bytes(size_t amount);
void* __builtin_dynamic_array(size_t length, size_t element_width);
void* __builtin_dynamic_ndim_array(size_t element_width, size_t dc, ...);
static void __builtin_delete_array_recur(void* array, size_t dc, unsigned long long* dimensions);
void __builtin_delete_array(void* array, size_t dc, ...);
size_t __builtin_array_size(void* array);
void __builtin_copy_memory(void* dest, const void* src, int length);

#endif