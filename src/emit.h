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

typedef enum {
    RIP,
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

const Operand RAX = {.tag = REGISTER, .o = {.reg = {.i = 0, .size = 64}}};
const Operand RCX = {.tag = REGISTER, .o = {.reg = {.i = 1, .size = 64}}};
const Operand RDX = {.tag = REGISTER, .o = {.reg = {.i = 2, .size = 64}}};
const Operand RBX = {.tag = REGISTER, .o = {.reg = {.i = 3, .size = 64}}};
const Operand RSP = {.tag = REGISTER, .o = {.reg = {.i = 4, .size = 64}}};
const Operand RBP = {.tag = REGISTER, .o = {.reg = {.i = 5, .size = 64}}};
const Operand RSI = {.tag = REGISTER, .o = {.reg = {.i = 6, .size = 64}}};
const Operand RDI = {.tag = REGISTER, .o = {.reg = {.i = 7, .size = 64}}};
const Operand R8 = {.tag = REGISTER, .o = {.reg = {.i = 8, .size = 64}}};
const Operand R9 = {.tag = REGISTER, .o = {.reg = {.i = 9, .size = 64}}};
const Operand R10 = {.tag = REGISTER, .o = {.reg = {.i = 10, .size = 64}}};
const Operand R11 = {.tag = REGISTER, .o = {.reg = {.i = 11, .size = 64}}};
const Operand R12 = {.tag = REGISTER, .o = {.reg = {.i = 12, .size = 64}}};
const Operand R13 = {.tag = REGISTER, .o = {.reg = {.i = 13, .size = 64}}};
const Operand R14 = {.tag = REGISTER, .o = {.reg = {.i = 14, .size = 64}}};
const Operand R15 = {.tag = REGISTER, .o = {.reg = {.i = 15, .size = 64}}};
Operand argument_registers[] = {RDI, RSI, RDX, RCX, R8, R9};

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

void relocation_add(OperandMemory operand, size_t offset) {
    relocations_buf[relocations_size++] = (ElfRelocationEntry){
        .offset = offset,
        .type = 2, // R_X86_64_PC32
        .sym = operand.offset,
        .addend = -4,
    };
}

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
    if (a.tag == REGISTER && b.tag == MEMORY && b.o.memory.mode == SYMBOL_LOCAL) {
        text_buf[text_size++] = 0x48;
        text_buf[text_size++] = 0x8B;
        text_buf[text_size++] = (uint8_t)((0 << 6) | (a.o.reg.i << 3) | 5);
        relocation_add(b.o.memory, text_size);
        text_size += 4;
        return;
    }
    fprintf(stderr, "TODO asm_mov\n");
    asm_nop();
}

void asm_lea(Operand a, Operand b) {
    if (a.tag == REGISTER && b.tag == MEMORY && b.o.memory.mode == SYMBOL_LOCAL) {
        text_buf[text_size++] = 0x48;
        text_buf[text_size++] = 0x8D;
        text_buf[text_size++] = (uint8_t)((0 << 6) | (a.o.reg.i << 3) | 5);
        relocation_add(b.o.memory, text_size);
        text_size += 4;
        return;
    }
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

size_t symbol_add_local(char* name, size_t pos, size_t size) {
    memcpy(&symbol_names_buf[symbol_names_size], name, strlen(name));
    size_t idx = symbols_local_size;
    symbols_local_buf[symbols_local_size++] = (ElfSymbolEntry){
        .name = (uint32_t)symbol_names_size, .info = 1, .shndx = 2, .value = pos, .size = size};
    symbol_names_size += strlen(name) + 1;
    return idx;
}
