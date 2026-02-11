#pragma once
#include "core.h"

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

typedef struct {
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

const Operand RAX = {.tag = REGISTER, .o = {.reg = {.i = 0, .size = 64}}};
const Operand RCX = {.tag = REGISTER, .o = {.reg = {.i = 1, .size = 64}}};
const Operand RDX = {.tag = REGISTER, .o = {.reg = {.i = 2, .size = 64}}};
const Operand RBX = {.tag = REGISTER, .o = {.reg = {.i = 3, .size = 64}}};
const Operand RSP = {.tag = REGISTER, .o = {.reg = {.i = 4, .size = 64}}};
const Operand RBP = {.tag = REGISTER, .o = {.reg = {.i = 5, .size = 64}}};
const Operand RSI = {.tag = REGISTER, .o = {.reg = {.i = 6, .size = 64}}};
const Operand RDI = {.tag = REGISTER, .o = {.reg = {.i = 7, .size = 64}}};

ElfHeader elf_header = {
    .ei_magic = {0x7F, 0x45, 0x4C, 0x46},
    .ei_class = 0x02,
    .ei_data = 0x01,
    .ei_version = 0x01,
    .type = 0x01,
    .machine = 0x3E,
    .version = 0x01,
    .ehsize = 0x40,
    .shentsize = 0x40,
};

void asm_nop() {
    text_buf[text_size++] = 0x90;
}

void asm_mov(Operand a, Operand b) {
    if (a.tag == REGISTER && b.tag == REGISTER) {
        text_buf[text_size++] = 0x48;
        text_buf[text_size++] = 0x89;
        text_buf[text_size++] = (3 << 6) | (b.o.reg.i << 3) | (a.o.reg.i);
        return;
    }
    if (a.tag == REGISTER && b.tag == IMMEDIATE) {
        text_buf[text_size++] = 0x48;
        text_buf[text_size++] = 0xB8 + a.o.reg.i;
        memcpy(&text_buf[text_size], &b.o.immediate.value, 8);
        text_size += 8;
        return;
    }
    fprintf(stderr, "TODO asm_mov\n");
    asm_nop();
}

void asm_lea(Operand a, Operand b) {
    fprintf(stderr, "TODO asm_lea\n");
    asm_nop();
}

void asm_push(Operand a) {
    if (a.tag == REGISTER && a.o.reg.size == 64) {
        text_buf[text_size++] = 0x50 + a.o.reg.i;
        return;
    }
    fprintf(stderr, "TODO asm_push\n");
    asm_nop();
}

void asm_pop(Operand a) {
    if (a.tag == REGISTER && a.o.reg.size == 64) {
        text_buf[text_size++] = 0x58 + a.o.reg.i;
        return;
    }
    fprintf(stderr, "TODO asm_pop\n");
    asm_nop();
}

void asm_ret() {
    text_buf[text_size++] = 0xC3;
}

typedef struct {
    bool impl;
} AddSymbolOptions;
size_t symbol_add_global(Span name, size_t pos, AddSymbolOptions opts) {
    memcpy(&symbol_names_buf[symbol_names_size], &input_buf[name.start], name.len);
    size_t idx = symbols_global_size;
    symbols_global_buf[symbols_global_size++] = (ElfSymbolEntry){
        .name = (uint32_t)symbol_names_size,
        .info = (uint8_t)((1 << 4) + (opts.impl ? 2 : 0)),
        .shndx = (uint16_t)(opts.impl ? 1 : 0),
        .value = pos,
    };
    symbol_names_size += name.len + 1;
    return idx;
}
