#pragma once
#include "assert.h"
#include "elf.h"
#include "errno.h"
#include "stdbool.h"
#include "stdint.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "sys/param.h"

typedef struct {
    size_t start;
    size_t len;
} Span;

typedef enum {
    NONE = 0,
    IDENT,
    INT,
    STRING_PART,
    CHAR,
    ESCAPE,
    HASH,
    SEMI,
    QUOTE,
    DQUOTE,
    O_BRACE,
    C_BRACE,
    O_PAREN,
    C_PAREN,
    O_BRACKET,
    C_BRACKET,
    COMMA,
    ASTERISK,
    SLASH,
    AMPERSAND,
    IF,
    ELSE,
    WHILE,
    RETURN,
    PLUS,
    MINUS,
    EQUALS,
    O_ANGLE,
    C_ANGLE,
    EXCL,
    PERIOD,
    PERCENT,
} TokenType;

const char* token_literal[] = {
    NULL,     // NONE
    NULL,     // IDENT
    NULL,     // INT
    NULL,     // STRING
    NULL,     // CHAR
    NULL,     // ESCAPE
    "#",      // HASH
    ";",      // SEMI
    "'",      // QUOTE
    "\"",     // DQUOTE
    "{",      // O_BRACE
    "}",      // C_BRACE
    "(",      // O_PAREN
    ")",      // C_PAREN
    "[",      // O_BRACKET
    "]",      // C_BRACKET
    ",",      // COMMA
    "*",      // ASTERISK
    "/",      // SLASH
    "&",      // AMPERSAND
    "if",     // IF
    "else",   // ELSE
    "while",  // WHILE
    "return", // RETURN
    "+",      // PLUS
    "-",      // MINUS
    "=",      // EQUALS
    "<",      // O_ANGLE
    ">",      // C_ANGLE
    "!",      // EXCL
    ".",      // PERIOD
    "%",      // PERCENT
};
size_t token_literal_size = sizeof token_literal / sizeof token_literal[0];

const char* token_name[] = {
    "NONE",  "IDENT",    "INT",     "STRING_PART", "CHAR",    "ESCAPE",  "HASH",      "SEMI",
    "QUOTE", "DQUOTE",   "O_BRACE", "C_BRACE",     "O_PAREN", "C_PAREN", "O_BRACKET", "C_BRACKET",
    "COMMA", "ASTERISK", "SLASH",   "AMPERSAND",   "IF",      "ELSE",    "WHILE",     "RETURN",
    "PLUS",  "MINUS",    "EQUALS",  "O_ANGLE",     "C_ANGLE", "EXCL",    "PERIOD",    "PERCENT"};

typedef struct {
    Span span;
    TokenType type;
} Token;

typedef enum {
    REGISTER = 1,
    IMMEDIATE,
    MEMORY,
} OperandTag;

typedef struct {
    uint8_t i;
    uint8_t size;
    // in asm instructions, register is treated as effective address
    bool indirect;
} OperandRegister;

typedef struct {
    int64_t value;
} OperandImmediate;

typedef enum {
    R_PC32 = 0x2,
    R_PLT32 = 0x4,
    R_GOTPCREL = 0x9,
} ElfRelocationType;

typedef enum {
    REL_RIP = 1,
    REL_RBP,
    SYMBOL_LOCAL,
    SYMBOL_GLOBAL,
} AddressingMode;

typedef struct Symbol Symbol;

typedef struct {
    AddressingMode mode;
    int64_t offset;
    /**
     * In case mode == SYMBOL_GLOBAL
     */
    Symbol* symbol;
    /**
     * In case mode == SYMBOL_LOCAL || mode == SYMBOL_GLOBAL
     */
    ElfRelocationType relocation;
} OperandMemory;

typedef struct {
    OperandTag tag;
    union {
        OperandRegister reg;
        OperandImmediate immediate;
        OperandMemory memory;
    };
} Operand_;

typedef struct {
    Operand_ lvalue;
    Operand_ rvalue;
} Operand;

typedef struct {
    bool ok;
    Operand operand;
} Expr;

typedef enum {
    INFIX,
    PREFIX,
    POSTFIX,
} OperatorType;

typedef enum {
    OP_ADD = 1,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_EQ,
    OP_NEQ,
    OP_GT,
    OP_GE,
    OP_LT,
    OP_LE,
    OP_INDEX,
    OP_INCREMENT,
    OP_ASSIGN,
    OP_REMAINDER,
    OP_AND,
    OP_OR,
    OP_NOT,
    OP_ADDRESS_OF,
    OP_DEREFERENCE,
    OP_NEGATE,
} OperatorTag;

typedef struct {
    OperatorType type;
    OperatorTag tag;
    // result of OP_INDEX
    Operand operand;
} Operator;

uint8_t operator_precedence[] = {
    0,
    4,  // OP_ADD
    4,  // OP_SUB
    3,  // OP_MUL
    3,  // OP_DIV
    7,  // OP_EQ
    7,  // OP_NEQ
    6,  // OP_GT
    6,  // OP_GE
    6,  // OP_LT
    6,  // OP_LE
    1,  // OP_INDEX
    2,  // OP_INCREMENT
    14, // OP_ASSIGN
    3,  // OP_REMAINDER
    11, // OP_AND
    12, // OP_OR
    2,  // OP_NOT
    2,  // OP_ADDRESS_OF
    2,  // OP_DEREFERENCE
    2,  // OP_NEGATE
};

typedef enum {
    ASSOC_LEFT,
    ASSOC_RIGHT,
} Associativity;

uint8_t operator_associativity[] = {
    0,
    ASSOC_LEFT,  // OP_ADD
    ASSOC_LEFT,  // OP_SUB
    ASSOC_LEFT,  // OP_MUL
    ASSOC_LEFT,  // OP_DIV
    ASSOC_LEFT,  // OP_EQ
    ASSOC_LEFT,  // OP_NEQ
    ASSOC_LEFT,  // OP_GT
    ASSOC_LEFT,  // OP_GE
    ASSOC_LEFT,  // OP_LT
    ASSOC_LEFT,  // OP_LE
    ASSOC_LEFT,  // OP_INDEX
    ASSOC_RIGHT, // OP_INCREMENT
    ASSOC_RIGHT, // OP_ASSIGN
    ASSOC_LEFT,  // OP_REMAINDER
    ASSOC_LEFT,  // OP_AND
    ASSOC_LEFT,  // OP_OR
    ASSOC_RIGHT, // OP_NOT
    ASSOC_RIGHT, // OP_ADDRESS_OF
    ASSOC_RIGHT, // OP_DEREFERENCE
    ASSOC_RIGHT, // OP_NEGATE
};

struct Symbol {
    Span span;
    Operand operand;
    size_t size;
    size_t count;
    size_t item_size;
    size_t index;
    ElfSymbolEntry entry;
};

typedef struct {
    size_t symbols_start;
    int64_t bp_offset;
} Scope;

typedef struct {
    Symbol* symbol;
    ElfRelocationType type;
    size_t offset;
} SymbolRelocation;

typedef struct {
    /**
     * Source code, compiler input
     */
    char input[1 << 10];
    /**
     * Length of `input`
     */
    size_t input_len;

    bool inside_string;
    bool inside_char;

    /**
     * Lexer's current position in `input`
     */
    size_t token_offset;

    /**
     * Array of tokens, result of tokenization
     */
    Token tokens[1 << 10];
    /**
     * Total length of `tokens`
     */
    size_t tokens_len;

    /**
     * Parser's current position in `tokens`
     */
    size_t token_pos;

    /**
     * .text ELF file section
     */
    uint8_t text[1 << 10];
    size_t text_len;
    /**
     * .rodata ELF file section
     */
    uint8_t rodata[1 << 10];
    size_t rodata_len;
    /**
     * .strtab ELF file section
     */
    uint8_t symbol_names[1 << 10];
    size_t symbol_names_len;
    /**
     * .symtab ELF file section
     */
    ElfSymbolEntry symbols_local[1 << 10];
    size_t symbols_local_len;

    /**
     * Flat array of symbols that can be resolved by name
     */
    Symbol symbols[1 << 10];
    size_t symbols_len;
    /**
     * `symbols_buf[stack[n]]` is the first symbol in stack scope `n`
     */
    Scope stack[1 << 10];
    size_t stack_len;

    /**
     * RSP offset in a current function
     */
    int32_t stack_offset;
    SymbolRelocation global_relocations[1 << 10];
    size_t global_relocations_len;

    ElfRelocationEntry local_relocations[1 << 10];
    size_t local_relocations_len;

    size_t expr_registers_busy;
} Context;

Context ctx = {
    .symbol_names_len = 1,
    .symbols_local_len = 1,
};

Operand_ immediate(int64_t value) {
    return (Operand_){.tag = IMMEDIATE, .immediate = {.value = value}};
}

void stack_push() {
    Scope last = ctx.stack[ctx.stack_len];
    ctx.stack[ctx.stack_len++] = (Scope){
        .symbols_start = ctx.symbols_len,
        .bp_offset = last.bp_offset,
    };
}

void stack_pop() {
    ctx.stack_len--;
    ctx.symbols_len = ctx.stack[ctx.stack_len].symbols_start;
}

size_t stack_scope_size(size_t i) {
    return (i == ctx.stack_len - 1 ? ctx.symbols_len : ctx.stack[i + 1].symbols_start) -
           ctx.stack[i].symbols_start;
}

int32_t span_cmp(Span a, Span b) {
    if (a.len != b.len) return a.len - b.len;
    return strncmp(&ctx.input[a.start], &ctx.input[b.start], a.len);
}
