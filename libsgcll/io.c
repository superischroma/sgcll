#include "../libsgcllc/libsgcllc.h"

void io_g_println_string(char* str)
{
    __libsgcllc_string_println(str);
}

void io_g_println_i64(long long i)
{
    __libsgcllc_i64_println(i, 10);
}

void io_g_println_f64(double d)
{
    __libsgcllc_f64_println(d);
}