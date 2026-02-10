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

/**
 * @see https://refspecs.linuxfoundation.org/elf/gabi4+/ch4.eheader.html
 * @see https://en.wikipedia.org/wiki/Executable_and_Linkable_Format#ELF_header
 */
typedef struct {
    uint8_t ei_magic[4];
    uint8_t ei_class;
    uint8_t ei_data;
    uint8_t ei_version;
    uint8_t abi;
    uint8_t pad[7];
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint64_t entry;
    uint64_t phoff;
    uint64_t shoff;
    uint32_t flags;
    uint16_t ehsize;
    uint16_t phentsize;
    uint16_t phnum;
    uint16_t shentsize;
    uint16_t shnum;
    uint16_t shstrndx;
} ElfHeader;

ElfHeader elf_header = {
    .ei_magic = {0x7F, 0x45, 0x4C, 0x46},
    .ei_class = 0x02,
    .ei_data = 0x01,
    .ei_version = 0x01,
    .type = 0x01,
    .machine = 0x3e,
    .version = 0x01,
    .ehsize = 0x40,
    .shentsize = 0x40,
};

/**
 * @see https://en.wikipedia.org/wiki/Executable_and_Linkable_Format#Section_header
 */
typedef struct {
    uint32_t name;
    uint32_t type;
    uint64_t flags;
    uint64_t addr;
    uint64_t offset;
    uint64_t size;
    uint32_t link;
    uint32_t info;
    uint64_t addralign;
    uint64_t entsize;
} ElfSectionHeader;

void asm_mov(Operand a, Operand b) {
    fprintf(stderr, "TODO asm_mov\n");
    section_text_buf[text_size++] = 0x90;
}

void asm_push(Operand a) {
    if (a.tag == REGISTER && a.o.reg.size == 64) {
        section_text_buf[text_size++] = 0x50 + a.o.reg.i;
        return;
    }
    fprintf(stderr, "TODO asm_push\n");
    section_text_buf[text_size++] = 0x90;
}

void asm_pop(Operand a) {
    if (a.tag == REGISTER && a.o.reg.size == 64) {
        section_text_buf[text_size++] = 0x58 + a.o.reg.i;
        return;
    }
    fprintf(stderr, "TODO asm_pop\n");
    section_text_buf[text_size++] = 0x90;
}
