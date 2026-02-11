.PHONY: build

build:
	clang src/mcc.c -Wall -Wextra -g -o build/mcc

compile_hello: build
	build/mcc test/hello.c build/hello.o && objdump -xsd -M intel build/hello.o

link_hello: compile_hello
	ld /usr/lib64/crt1.o /usr/lib64/crti.o /usr/lib64/crtn.o build/hello.o -lc -dynamic-linker /usr/lib64/ld-linux-x86-64.so.2 -o build/hello

run_hello: link_hello
	build/hello
