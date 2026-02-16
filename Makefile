.PHONY: build

build:
	clang src/mcc.c -std=c99 -Wall -Wextra -g -o build/mcc

compile_hello: build
	build/mcc test/hello.c build/hello.o

link_hello: compile_hello
	ld /usr/lib64/crt1.o /usr/lib64/crti.o /usr/lib64/crtn.o build/hello.o -lc -dynamic-linker /usr/lib64/ld-linux-x86-64.so.2 -o build/hello

run_hello: link_hello
	build/hello

inspect_hello:
	xxd -c 32 build/hello.o
	readelf -a build/hello.o
	objdump -d -M intel build/hello.o

compile_fn: build
	build/mcc test/fn.c build/fn.o

link_fn: compile_fn
	ld /usr/lib64/crt1.o /usr/lib64/crti.o /usr/lib64/crtn.o build/fn.o -lc -dynamic-linker /usr/lib64/ld-linux-x86-64.so.2 -o build/fn

run_fn: link_fn
	build/fn

inspect_fn:
	xxd -c 32 build/fn.o
	readelf -a build/fn.o
	objdump -d -M intel build/fn.o
