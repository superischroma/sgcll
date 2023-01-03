cd libsgcllc
gcc -c -o io.o io.c
gcc -c -o kernel.o kernel.c
gcc -c -o memory.o memory.c
gcc -c -o string.o string.c
ar rcs libsgcllc.a io.o kernel.o memory.o string.o
cd ..