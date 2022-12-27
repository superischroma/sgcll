#include "../builtin/builtin.h"

void io_g_println_string(char* str)
{
    __builtin_string_println(str);
}

void io_g_println_i64(long long i)
{
    __builtin_i64_println(i, 10);
}

void io_g_println_f64(double d)
{
    __builtin_f64_println(d);
}