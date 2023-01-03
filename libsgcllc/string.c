#include <windows.h>

#include "libsgcllc.h"

int __libsgcllc_itos(long long n, char* buffer, size_t radix)
{
    int i, sign;

    if ((sign = n) < 0)
        n = -n;
    i = 0;
    do
    {
        int a = abs(n % radix);
        if (a > 9)
            a += '7';
        else
            a += '0';
        buffer[i++] = a;
    }
    while (n /= radix);
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

int __libsgcllc_ftos(double d, char* buffer, int precision)
{
    int ipart = (int) d;
    double fpart = d - ipart;
    int i = __libsgcllc_itos(ipart, buffer, 10);
    if (fpart != (int) fpart)
        buffer[i++] = '.';
    for (; fpart != (int) fpart && precision > 0; fpart -= (int) fpart, precision--)
    {
        fpart *= 10.0;
        buffer[i++] = abs((int) fpart) + '0';
    }
    buffer[i] = '\0';
    return i;
}

int __libsgcllc_string_length(char* str)
{
    int i = 0;
    for (; str[i]; i++);
    return i;
}