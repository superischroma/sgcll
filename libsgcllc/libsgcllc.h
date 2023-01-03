#ifndef __LIBSGCLLC_H
#define __LIBSGCLLC_H

typedef unsigned long long size_t;
typedef int BOOL;

#define __libsgcllc_DEBUG

#define abs(x) ((x) < 0 ? -(x) : (x))
#define fmod(x, y) (x - y * (int) (x / y))

// only use this if you know what you're doing!!
#define __libsgcllc_varadic NULL; \
    __asm__("   leaq 32(%rbp), %rax\n" \
            "   movq %rax, -8(%rbp)")
    
typedef struct gc_node_t
{
    struct gc_node_t* next;
    void* mem;
} gc_node_t;

/* kernel.c */

extern gc_node_t* gc_root;

void __libsgcllc_init();
void __libsgcllc_gc_finalize();

/* io.c */

void __libsgcllc_i64_println(long long ll, size_t radix);
void __libsgcllc_f64_println(double d);
void __libsgcllc_string_print(char* str);
void __libsgcllc_string_println(char* str);

/* string.c */

int __libsgcllc_itos(long long n, char* buffer, size_t radix);
int __libsgcllc_ftos(double d, char* buffer, int precision);
int __libsgcllc_string_length(char* str);

/* memory.c */

void* __libsgcllc_alloc_bytes(size_t amount);
BOOL __libsgcllc_delete_bytes_no_gc(void* mem);
void* __libsgcllc_dynamic_array(size_t length, size_t element_width);
void* __libsgcllc_dynamic_ndim_array(size_t element_width, size_t dc, ...);
static void __libsgcllc_delete_array_recur(void* array, size_t dc, unsigned long long* dimensions);
void __libsgcllc_delete_array(void* array, size_t dc, ...);
size_t __libsgcllc_array_size(void* array);
void __libsgcllc_copy_memory(void* dest, const void* src, int length);
size_t __libsgcllc_blueprint_size(void* obj);

#endif