#ifndef __LIBSGCLLC_H
#define __LIBSGCLLC_H

typedef unsigned long long sz_t;
typedef unsigned long long uintptr_t;
typedef int BOOL;

#define __libsgcllc_DEBUG

#define stdin -10
#define stdout -11
#define stderr -12

#define abs(x) ((x) < 0 ? -(x) : (x))
#define fmod(x, y) (x - y * (int) (x / y))
#define variadic(arg) (unsigned long long*) (&(arg)) + 1
    
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

void __libsgcllc_fputchar(void* file, char c);
void __libsgcllc_fputs(void* file, char* str);
void __libsgcllc_fprintf(void* file, char* fmt, ...);
void* __libsgcllc_stdstream(int descriptor);

/* string.c */

int __libsgcllc_itos(long long n, char* buffer, sz_t radix);
int __libsgcllc_ftos(double d, char* buffer, int precision);
int __libsgcllc_string_length(char* str);

/* memory.c */

void* __libsgcllc_alloc_bytes(sz_t amount);
BOOL __libsgcllc_delete_bytes_no_gc(void* mem);
void* __libsgcllc_dynamic_array(sz_t length, sz_t element_width);
void* __libsgcllc_dynamic_ndim_array(sz_t element_width, sz_t dc, ...);
static void __libsgcllc_delete_array_recur(void* array, sz_t dc, unsigned long long* dimensions);
void __libsgcllc_delete_array(void* array, sz_t dc, ...);
sz_t __libsgcllc_array_size(void* array);
void __libsgcllc_copy_memory(void* dest, const void* src, int length);
sz_t __libsgcllc_blueprint_size(void* obj);

#endif