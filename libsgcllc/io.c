#include <windows.h>

#include "libsgcllc.h"

#define POSITIVE_INFINITY 1.0 / 0.0
#define NEGATIVE_INFINITY -1.0 / 0.0
#define NAN 0.0 / 0.0

void __libsgcllc_fputchar(void* file, char c)
{
    WriteFile(file, &c, 1, 0, NULL);
}

void __libsgcllc_fputs(void* file, char* str)
{
    WriteFile(file, str, __libsgcllc_string_length(str), 0, NULL);
}

void __libsgcllc_fprintf(void* file, char* fmt, ...)
{
    unsigned long long* args = variadic(fmt);
    char miscbuffer[100];
    #define fp_case(type, arg) \
        type arg = *((type*) (args + i++)); \
        if (arg == POSITIVE_INFINITY) \
            __libsgcllc_fputs(file, "infinity"); \
        else if (arg == NEGATIVE_INFINITY) \
            __libsgcllc_fputs(file, "-infinity"); \
        else if (arg == NAN) \
            __libsgcllc_fputs(file, "NaN"); \
        else \
        { \
            __libsgcllc_ftos(arg, miscbuffer, 30); \
            __libsgcllc_fputs(file, miscbuffer); \
        }
    for (int i = 0; *fmt; ++fmt)
    {
        if (*fmt == '%')
        {
            switch (*++fmt)
            {
                case 's':
                    __libsgcllc_fputs(file, (char*) args[i++]);
                    break;
                case 'i':
                    __libsgcllc_itos((int) args[i++], miscbuffer, 10);
                    __libsgcllc_fputs(file, miscbuffer);
                    break;
                case 'l':
                    __libsgcllc_itos((long long) args[i++], miscbuffer, 10);
                    __libsgcllc_fputs(file, miscbuffer);
                    break;
                case 'c':
                    __libsgcllc_fputchar(file, (char) args[i++]);
                    break;
                case 'f':
                    fp_case(float, arg0);
                    break;
                case 'd':
                    fp_case(double, arg1);
                    break;
                case 'p':
                    __libsgcllc_itos((uintptr_t) args[i++], miscbuffer, 16);
                    __libsgcllc_fputs(file, miscbuffer);
                    break;
                default:
                    __libsgcllc_fputchar(file, '%');
                    __libsgcllc_fputchar(file, *fmt);
                    break;
            }
            continue;
        }
        __libsgcllc_fputchar(file, *fmt);
    }
}

void* __libsgcllc_stdstream(int descriptor)
{
    return GetStdHandle((DWORD) descriptor);
}