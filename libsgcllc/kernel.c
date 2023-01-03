#include <windows.h>

#include "libsgcllc.h"

gc_node_t* gc_root = NULL;

void __libsgcllc_init()
{
    _mm_setcsr((_mm_getcsr() & 0xF3FF) | 0x6000); // set rounding mode
}

void __libsgcllc_gc_finalize()
{
    for (gc_node_t* node = gc_root; node;)
    {
        gc_node_t* next = node->next;
        void* mem = node->mem;
        BOOL result = __libsgcllc_delete_bytes_no_gc(node->mem);
        __libsgcllc_delete_bytes_no_gc(node);
        #ifdef __libsgcllc_DEBUG
        if (result)
            __libsgcllc_string_print("[builtin debug] deallocated memory at 0x");
        else
            __libsgcllc_string_print("[builtin debug] failed to deallocate memory at 0x");
        __libsgcllc_i64_println((UINT_PTR) mem, 16);
        #endif
        node = next;
    }
}