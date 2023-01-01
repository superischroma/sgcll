gcc -o libsgcll/io_lowlvl.o -c libsgcll/io.c
sgcllc libsgcll/io.sgcll
gcc -o libsgcll/io.o -c libsgcll/io.s

gcc -o libsgcll/string_lowlvl.o -c libsgcll/string.c
sgcllc libsgcll/string.sgcll
gcc -o libsgcll/string.o -c libsgcll/string.s

sgcllc libsgcll/math.sgcll
gcc -o libsgcll/math.o -c libsgcll/math.s