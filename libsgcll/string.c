#include "../libsgcllc/libsgcllc.h"

char* string_g_concat_string_string(char* lhs, char* rhs)
{
    int lhs_length = __libsgcllc_string_length(lhs),
        rhs_length = __libsgcllc_string_length(rhs),
        length = lhs_length + rhs_length + 1;
    char* new = __libsgcllc_alloc_bytes(length);
    __libsgcllc_copy_memory(new, lhs, lhs_length);
    __libsgcllc_copy_memory(new + lhs_length, rhs, rhs_length);
    new[lhs_length + rhs_length] = '\0';
    return new;
}

char* string_g_concat_string_i64(char* lhs, long long rhs)
{
    char buffer[34];
    __libsgcllc_itos(rhs, buffer, 10);
    return string_g_concat_string_string(lhs, buffer);
}

char* string_g_concat_i64_string(long long lhs, char* rhs)
{
    char buffer[34];
    __libsgcllc_itos(lhs, buffer, 10);
    return string_g_concat_string_string(buffer, rhs);
}