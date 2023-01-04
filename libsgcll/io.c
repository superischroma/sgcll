#include "../libsgcllc/libsgcllc.h"

void io_g_println_string(char* str)
{
    __libsgcllc_fprintf(__libsgcllc_stdstream(stdout), "%s\n", str);
}

void io_g_println_i64(long long i)
{
    __libsgcllc_fprintf(__libsgcllc_stdstream(stdout), "%l\n", i);
}

void io_g_println_f64(double d)
{
    __libsgcllc_fprintf(__libsgcllc_stdstream(stdout), "%d\n", d);
}