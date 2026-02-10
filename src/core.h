#pragma once
#include "stdbool.h"
#include "stdint.h"
#include "stdio.h"
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
