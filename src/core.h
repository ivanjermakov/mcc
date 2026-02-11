#pragma once
#include "elf.h"
#include "stdbool.h"
#include "stdint.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

typedef struct {
    size_t start;
    size_t len;
} Span;

typedef enum {
    NONE = 0,
    IDENT,
    INT,
    STRING,
    HASH,
    SEMI,
    O_BRACE,
    C_BRACE,
    O_PAREN,
    C_PAREN,
    COMMA,
    ASTERISK,
    AMPERSAND,
    IF,
    RETURN,
    OP_PLUS,
} TokenType;
const char* token_literal[] = {
    0, 0, 0, 0, "#", ";", "{", "}", "(", ")", ",", "*", "&", "if", "return", "+",
};
size_t token_literal_size = sizeof token_literal / sizeof token_literal[0];
const char* token_name[] = {"NONE",      "IDENT",   "INT",     "STRING",  "HASH",  "SEMI",
                            "O_BRACE",   "C_BRACE", "O_PAREN", "C_PAREN", "COMMA", "ASTERISK",
                            "AMPERSAND", "IF",      "RETURN",  "OP_PLUS"};

typedef struct {
    Span span;
    TokenType type;
} Token;

Token token_buf[1 << 10];
size_t token_size = 0;
size_t token_offset = 0;

char input_buf[1 << 10];
size_t input_size = 0;

size_t token_pos = 0;

uint8_t text_buf[1 << 10];
size_t text_size = 0;
uint8_t rodata_buf[1 << 10];
size_t rodata_size = 0;
uint8_t symbol_names_buf[1 << 10];
size_t symbol_names_size = 1;
ElfSymbolEntry symbols_local_buf[1 << 10];
size_t symbols_local_size = 1;
ElfSymbolEntry symbols_global_buf[1 << 10];
size_t symbols_global_size = 0;
