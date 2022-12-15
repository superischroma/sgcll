extern void __builtin_string_println(char*);

void print(char* str)
{
    __builtin_string_println(str);
}