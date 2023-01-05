gcc -o libsgcll/io_lowlvl.o -c libsgcll/io.c
sgcllc libsgcll/io.sgcll

gcc -o libsgcll/string_lowlvl.o -c libsgcll/string.c
sgcllc libsgcll/string.sgcll

gcc -o libsgcll/math_lowlvl.o -c libsgcll/math.c
sgcllc libsgcll/math.sgcll

ar rcs libsgcll/libsgcll.a libsgcll/io.o libsgcll/io_lowlvl.o libsgcll/string.o libsgcll/string_lowlvl.o libsgcll/math.o libsgcll/math_lowlvl.o