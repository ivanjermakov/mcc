#pragma once
#include "assert.h"
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
    STRING_PART,
    ESCAPE,
    HASH,
    SEMI,
    DQUOTE,
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
    OP_EQUAL,
} TokenType;
const char* token_literal[] = {
    0, 0, 0, 0, 0, "#", ";", "\"", "{", "}", "(", ")", ",", "*", "&", "if", "return", "+", "=",
};
size_t token_literal_size = sizeof token_literal / sizeof token_literal[0];
const char* token_name[] = {"NONE",    "IDENT",   "INT",     "STRING_PART", "ESCAPE",
                            "HASH",    "SEMI",    "DQUOTE",  "O_BRACE",     "C_BRACE",
                            "O_PAREN", "C_PAREN", "COMMA",   "ASTERISK",    "AMPERSAND",
                            "IF",      "RETURN",  "OP_PLUS", "OP_EQUAL"};

typedef struct {
    Span span;
    TokenType type;
} Token;

Token token_buf[1 << 10];
size_t token_size = 0;
size_t token_offset = 0;

bool inside_string = false;

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

typedef enum {
    REGISTER,
    IMMEDIATE,
    MEMORY,
} OperandTag;

typedef struct {
    uint8_t i;
    uint8_t size;
} OperandRegister;

typedef struct {
    int64_t value;
} OperandImmediate;

typedef enum {
    REL_RIP,
    REL_RBP,
    SYMBOL_LOCAL,
    SYMBOL_GLOBAL,
} AddressingMode;

typedef struct {
    AddressingMode mode;
    int64_t offset;
} OperandMemory;

typedef struct {
    OperandTag tag;
    union {
        OperandRegister reg;
        OperandImmediate immediate;
        OperandMemory memory;
    } o;
} Operand;

typedef struct {
    Span span;
    Operand operand;
    size_t index;
    ElfSymbolEntry entry;
} Symbol;

typedef struct {
    size_t symbols_start;
    int64_t bp_offset;
} Scope;

/**
 * Flat array of symbols that can be resolved by name
 */
Symbol symbols_buf[1 << 10];
size_t symbols_size = 0;
/**
 * `symbols_buf[stack[n]]` is the first symbol in stack scope `n`
 */
Scope stack[1 << 10];
size_t stack_size = 0;

void stack_push() {
    stack[stack_size++] = (Scope){
        .symbols_start = symbols_size,
        .bp_offset = 0,
    };
}

void stack_pop() {
    stack_size--;
    symbols_size = stack[stack_size].symbols_start;
}

size_t stack_scope_size(size_t i) {
    return (i == stack_size - 1 ? symbols_size : stack[i + 1].symbols_start) -
           stack[i].symbols_start;
}

int32_t span_cmp(Span a, Span b) {
    if (a.len != b.len) return a.len - b.len;
    return strncmp(&input_buf[a.start], &input_buf[b.start], a.len);
}

typedef enum {
    PC32 = 2,
    PLT32 = 4,
} ElfRelocationType;

typedef struct {
    Symbol* symbol;
    ElfRelocationType type;
    size_t offset;
} SymbolRelocation;

SymbolRelocation global_relocations_buf[1 << 10];
size_t global_relocations_size = 0;

ElfRelocationEntry local_relocations_buf[1 << 10];
size_t local_relocations_size = 0;
