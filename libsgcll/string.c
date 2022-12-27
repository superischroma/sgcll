#include "../builtin/builtin.h"

char* string_g_concat_string_string(char* lhs, char* rhs)
{
    int lhs_length = __builtin_string_length(lhs),
        rhs_length = __builtin_string_length(rhs),
        length = lhs_length + rhs_length + 1;
    char* new = __builtin_alloc_bytes(length);
    __builtin_copy_memory(new, lhs, lhs_length);
    __builtin_copy_memory(new + lhs_length, rhs, rhs_length);
    new[lhs_length + rhs_length] = '\0';
    return new;
}

char* string_g_concat_string_i64(char* lhs, long long rhs)
{
    char buffer[34];
    __builtin_i64_dec_itos(rhs, buffer, 10);
    return string_g_concat_string_string(lhs, buffer);
}

char* string_g_concat_i64_string(long long lhs, char* rhs)
{
    char buffer[34];
    __builtin_i64_dec_itos(lhs, buffer, 10);
    return string_g_concat_string_string(buffer, rhs);
}