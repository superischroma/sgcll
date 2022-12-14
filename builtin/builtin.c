#include <windows.h>

#define __builtin_abs(x) ((x) < 0 ? -(x) : (x))

int __builtin_i32_dec_itos(int n, char* buffer)
{
    int i, sign;

    if ((sign = n) < 0)
        n = -n;
    i = 0;
    do
        buffer[i++] = __builtin_abs(n % 10) + '0';
    while (n /= 10);
    if (sign < 0)
        buffer[i++] = '-';
    buffer[i] = '\0';
    for (int j = 0; j < i / 2; j++)
    {
        char temp = buffer[j];
        buffer[j] = buffer[i - j - 1];
        buffer[i - j - 1] = temp;
    }
    return i;
}

int __builtin_f32_dec_ftos(float f, char* buffer, int precision)
{
    int ipart = (int) f;
    int i = __builtin_i32_dec_itos(ipart, buffer);
    buffer[i++] = '.';
    float dec = (f - ipart) * 10.0f;
    for (int j = 0; j < precision; i++, j++, dec = (dec * 10.0f) - ((int) dec) * 10)
        buffer[i] = (int) dec + '0';
    buffer[i] = '\0';
    return i;
}

void __builtin_i32_println(int i)
{
    char buffer[34];
    int c = __builtin_i32_dec_itos(i, buffer);
    buffer[c] = '\n';
    buffer[c + 1] = '\0';
    WriteFile(GetStdHandle((DWORD) -11), buffer, c + 1, 0, NULL);
}

void __builtin_f32_println(float f)
{
    char buffer[80];
    int c = __builtin_f32_dec_ftos(f, buffer, 6);
    buffer[c] = '\n';
    buffer[c + 1] = '\0';
    WriteFile(GetStdHandle((DWORD) -11), buffer, c + 1, 0, NULL);
}