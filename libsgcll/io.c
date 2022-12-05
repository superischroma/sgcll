#include <windows.h>

void print0(char c)
{
    WriteFile(GetStdHandle((DWORD) -11), &c, 1, 0, NULL);
}