.PHONY: build
LDFLAGS = /usr/lib64/crt1.o /usr/lib64/crti.o /usr/lib64/crtn.o -lc -dynamic-linker /usr/lib64/ld-linux-x86-64.so.2 -z noexecstack
OBJDUMPFLAGS = -dw --visualize-jumps=color -M intel

build:
	mkdir -p build
	clang src/mcc.c -std=c99 -Wall -Wextra -g -o build/mcc

test: build
	./test.sh /usr/lib64

ci: build
	./test.sh /usr/lib/x86_64-linux-gnu

compile_hello: build
	build/mcc test/hello.c build/hello.o

link_hello: compile_hello
	ld $(LDFLAGS) build/hello.o -o build/hello

run_hello: link_hello
	build/hello

inspect_hello:
	objdump $(OBJDUMPFLAGS) build/hello.o

compile_fn: build
	build/mcc test/fn.c build/fn.o

link_fn: compile_fn
	ld $(LDFLAGS) build/fn.o -o build/fn

run_fn: link_fn
	build/fn

inspect_fn:
	objdump $(OBJDUMPFLAGS) build/fn.o

compile_ascii: build
	build/mcc test/ascii.c build/ascii.o

link_ascii: compile_ascii
	ld $(LDFLAGS) build/ascii.o -o build/ascii

run_ascii: link_ascii
	build/ascii

inspect_ascii:
	objdump $(OBJDUMPFLAGS) build/ascii.o

compile_fizzbuzz: build
	build/mcc test/fizzbuzz.c build/fizzbuzz.o

link_fizzbuzz: compile_fizzbuzz
	ld $(LDFLAGS) build/fizzbuzz.o -o build/fizzbuzz

run_fizzbuzz: link_fizzbuzz
	build/fizzbuzz

inspect_fizzbuzz:
	objdump $(OBJDUMPFLAGS) build/fizzbuzz.o
