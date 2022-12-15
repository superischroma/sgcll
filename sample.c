extern void __builtin_f32_println(float);

double fmod(double x, double y)
{
    //i32 i = x / y;
    return x - y;// * i;
}

int main()
{
    __builtin_f32_println(fmod(5.0, 3.0));
}