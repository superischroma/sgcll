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
            __libsgcllc_fprintf(__libsgcllc_stdstream(stdout), "[builtin debug] deallocated memory at 0x");
        else
            __libsgcllc_fprintf(__libsgcllc_stdstream(stdout), "[builtin debug] failed to deallocate memory at 0x");
        __libsgcllc_fprintf(__libsgcllc_stdstream(stdout), "%p\n", mem);
        #endif
        node = next;
    }
}