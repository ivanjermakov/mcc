.PHONY: build

build:
	clang src/mcc.c -Wall -Wextra -g -o build/mcc
