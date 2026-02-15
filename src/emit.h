#pragma once
#include "core.h"

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

void relocation_add_local(ElfRelocationType type, size_t symbol_index, size_t offset) {
    local_relocations_buf[local_relocations_size++] = (ElfRelocationEntry){
        .offset = offset,
        .type = type,
        .sym = symbol_index,
        // TODO: explain
        .addend = -4,
    };
}

typedef enum {
    // [BX + SI]
    RM_BX_SI = 0,
    // [BX + DI]
    RM_BX_DI = 1,
    // [BP + SI]
    RM_BP_SI = 2,
    // [BP + DI]
    RM_BP_DI = 3,
    // [SI]
    RM_SI = 4,
    // [DI]
    RM_DI = 5,
    // [BP]   (mod=00 → disp32)
    RM_BP = 6,
    // [BX]
    RM_BX = 7
} ModrmRm;

typedef enum {
    // [rm]
    MOD_INDIRECT = 0b00,
    // [rm + disp8]
    MOD_INDIRECT_DISP8 = 0b01,
    // [rm + disp32]
    MOD_INDIRECT_DISP32 = 0b10,
    // register operand
    MOD_REGISTER = 0b11
} ModrmMod;

uint8_t modrm(ModrmMod mod, uint8_t reg, uint8_t rm) {
    assert(mod < 4);
    assert(reg < 8);
    assert(rm < 8);
    return (uint8_t)((mod << 6) | (reg << 3) | rm);
}

void asm_nop() {
    text_buf[text_size++] = 0x90;
}

void asm_mov(Operand a, Operand b) {
    if (a.tag == REGISTER && b.tag == REGISTER) {
        text_buf[text_size++] = 0x48;
        text_buf[text_size++] = 0x89;
        text_buf[text_size++] = modrm(MOD_REGISTER, b.o.reg.i, a.o.reg.i);
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
        text_buf[text_size++] = modrm(MOD_INDIRECT, a.o.reg.i, RM_DI);
        relocation_add_local(PC32, b.o.memory.offset, text_size);
        text_size += 4;
        return;
    }
    if (a.tag == MEMORY && a.o.memory.mode == REL_RBP && b.tag == REGISTER) {
        text_buf[text_size++] = 0x48;
        text_buf[text_size++] = 0x89;
        text_buf[text_size++] = modrm(MOD_INDIRECT_DISP8, b.o.reg.i, RM_DI);
        text_buf[text_size++] = (uint8_t)a.o.memory.offset;
        return;
    }
    if (a.tag == REGISTER && b.tag == MEMORY && b.o.memory.mode == REL_RBP) {
        text_buf[text_size++] = 0x48;
        text_buf[text_size++] = 0x8b;
        text_buf[text_size++] = modrm(MOD_INDIRECT_DISP8, a.o.reg.i, RM_DI);
        text_buf[text_size++] = (uint8_t)b.o.memory.offset;
        return;
    }
    fprintf(stderr, "TODO asm_mov\n");
    asm_nop();
}

void asm_lea(Operand a, Operand b) {
    if (a.tag == REGISTER && b.tag == MEMORY && b.o.memory.mode == SYMBOL_LOCAL) {
        text_buf[text_size++] = 0x48;
        text_buf[text_size++] = 0x8D;
        text_buf[text_size++] = modrm(0, a.o.reg.i, 5);
        relocation_add_local(PC32, b.o.memory.offset, text_size);
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

void asm_call_global(Symbol* symbol) {
    text_buf[text_size++] = 0xE8;
    global_relocations_buf[global_relocations_size++] = (SymbolRelocation){
        .symbol = symbol,
        .type = PLT32,
        .offset = text_size,
    };
    text_size += 4;
}

void write_elf(FILE* out_file) {
    uint8_t sections_buf[1 << 14] = {0};
    size_t sections_size = 0;

    uint64_t section_text_offset = sizeof elf_header + sections_size;
    memcpy(&sections_buf[sections_size], &text_buf, text_size);
    sections_size += text_size;

    uint64_t section_rodata_offset = sizeof elf_header + sections_size;
    memcpy(&sections_buf[sections_size], &rodata_buf, rodata_size);
    sections_size += rodata_size;

    uint64_t section_shstrtab_offset = sizeof elf_header + sections_size;
    char* section_names[7] = {".null",   ".text",   ".rodata",  ".shstrtab",
                              ".strtab", ".symtab", ".rel.text"};
    uint32_t shstrtab_offsets[7] = {0};
    uint8_t shstrtab_buf[1 << 10] = {0};
    size_t shstrtab_size = 0;
    for (size_t i = 0; i < sizeof section_names / sizeof section_names[0]; i++) {
        shstrtab_offsets[i] = shstrtab_size;
        memcpy(&shstrtab_buf[shstrtab_size], section_names[i], strlen(section_names[i]));
        shstrtab_size += strlen(section_names[i]) + 1;
    }
    memcpy(&sections_buf[sections_size], &shstrtab_buf, shstrtab_size);
    sections_size += shstrtab_size;

    uint64_t section_strtab_offset = sizeof elf_header + sections_size;
    memcpy(&sections_buf[sections_size], &symbol_names_buf, symbol_names_size);
    sections_size += symbol_names_size;

    uint64_t section_symtab_offset = sizeof elf_header + sections_size;
    memcpy(&sections_buf[sections_size], &symbols_local_buf,
           sizeof(ElfSymbolEntry) * symbols_local_size);
    sections_size += sizeof(ElfSymbolEntry) * symbols_local_size;

    size_t global_scope_size = stack_scope_size(0);
    for (size_t i = 0; i < global_scope_size; i++) {
        Symbol* symbol = &symbols_buf[i];
        symbol->index = symbols_local_size + i;
        ElfSymbolEntry entry = symbol->entry;
        memcpy(&sections_buf[sections_size], &entry, sizeof entry);
        sections_size += sizeof entry;
    }

    uint64_t section_reltext_offset = sizeof elf_header + sections_size;

    memcpy(&sections_buf[sections_size], &local_relocations_buf,
           sizeof(ElfRelocationEntry) * local_relocations_size);
    sections_size += sizeof(ElfRelocationEntry) * local_relocations_size;

    for (size_t i = 0; i < global_relocations_size; i++) {
        SymbolRelocation sym = global_relocations_buf[i];
        ElfRelocationEntry entry = (ElfRelocationEntry){
            .offset = sym.offset,
            .type = sym.type,
            .sym = sym.symbol->index,
            // TODO: explain
            .addend = -4,

        };
        memcpy(&sections_buf[sections_size], &entry, sizeof entry);
        sections_size += sizeof entry;
    }

    uint64_t section_header_table_offset = sizeof elf_header + sections_size;
    ElfSectionHeader null = {0};
    memcpy(&sections_buf[sections_size], &null, sizeof null);
    sections_size += sizeof null;

    ElfSectionHeader text = {
        .name = shstrtab_offsets[1],
        .type = 1,
        .flags = 0x02 | 0x04,
        .addr = 0,
        .offset = section_text_offset,
        .size = text_size,
    };
    memcpy(&sections_buf[sections_size], &text, sizeof text);
    sections_size += sizeof text;

    ElfSectionHeader rodata = {
        .name = shstrtab_offsets[2],
        .type = 1,
        .flags = 0x02 | 0x10 | 0x20,
        .offset = section_rodata_offset,
        .size = rodata_size,
    };
    memcpy(&sections_buf[sections_size], &rodata, sizeof rodata);
    sections_size += sizeof rodata;

    ElfSectionHeader shstrtab = {
        .name = shstrtab_offsets[3],
        .type = 3,
        .offset = section_shstrtab_offset,
        .size = shstrtab_size,
    };
    memcpy(&sections_buf[sections_size], &shstrtab, sizeof shstrtab);
    sections_size += sizeof shstrtab;

    ElfSectionHeader strtab = {
        .name = shstrtab_offsets[4],
        .type = 3,
        .offset = section_strtab_offset,
        .size = symbol_names_size,
    };
    memcpy(&sections_buf[sections_size], &strtab, sizeof strtab);
    sections_size += sizeof strtab;

    ElfSectionHeader symtab = {
        .name = shstrtab_offsets[5],
        .type = 2,
        .offset = section_symtab_offset,
        .size = sizeof(ElfSymbolEntry) * (symbols_local_size + global_scope_size),
        .link = 4,                  // index of .strtab section
        .info = symbols_local_size, // index of the first non-local symbol
        .entsize = sizeof(ElfSymbolEntry),
    };
    memcpy(&sections_buf[sections_size], &symtab, sizeof symtab);
    sections_size += sizeof symtab;

    ElfSectionHeader reltext = {
        .name = shstrtab_offsets[6],
        .type = 4,
        .offset = section_reltext_offset,
        .size = sizeof(ElfRelocationEntry) * (local_relocations_size + global_relocations_size),
        .link = 5, // index of a .symtab section
        .info = 1, // index of a .rel.text section
        .entsize = sizeof(ElfRelocationEntry),
    };
    memcpy(&sections_buf[sections_size], &reltext, sizeof reltext);
    sections_size += sizeof reltext;

    elf_header.shoff = section_header_table_offset;
    elf_header.shnum = 7;
    elf_header.shstrndx = 3;

    fwrite(&elf_header, 1, sizeof elf_header, out_file);
    fwrite(sections_buf, 1, sections_size, out_file);
}
