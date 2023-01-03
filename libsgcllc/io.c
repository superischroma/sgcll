#include <windows.h>

#include "libsgcllc.h"

void __libsgcllc_printf(char* fmt, ...)
{
    
}

void __libsgcllc_i64_println(long long ll, size_t radix)
{
    char buffer[34];
    int c = __libsgcllc_itos(ll, buffer, radix);
    buffer[c] = '\n';
    buffer[c + 1] = '\0';
    WriteFile(GetStdHandle((DWORD) -11), buffer, c + 1, 0, NULL);
}

void __libsgcllc_f64_println(double d)
{
    char buffer[80];
    int c = __libsgcllc_ftos(d, buffer, 6);
    buffer[c] = '\n';
    buffer[c + 1] = '\0';
    WriteFile(GetStdHandle((DWORD) -11), buffer, c + 1, 0, NULL);
}

void __libsgcllc_string_print(char* str)
{
    WriteFile(GetStdHandle((DWORD) -11), str, __libsgcllc_string_length(str), 0, NULL);
}

void __libsgcllc_string_println(char* str)
{
    WriteFile(GetStdHandle((DWORD) -11), str, __libsgcllc_string_length(str), 0, NULL);
    WriteFile(GetStdHandle((DWORD) -11), "\n", 1, 0, NULL);
}