#pragma once
#include "core.h"

const Operand_ RAX = {.tag = REGISTER, .reg = {.i = 0, .size = 64}};
const Operand_ RCX = {.tag = REGISTER, .reg = {.i = 1, .size = 64}};
const Operand_ RDX = {.tag = REGISTER, .reg = {.i = 2, .size = 64}};
const Operand_ RBX = {.tag = REGISTER, .reg = {.i = 3, .size = 64}};
const Operand_ RSP = {.tag = REGISTER, .reg = {.i = 4, .size = 64}};
const Operand_ RBP = {.tag = REGISTER, .reg = {.i = 5, .size = 64}};
const Operand_ RSI = {.tag = REGISTER, .reg = {.i = 6, .size = 64}};
const Operand_ RDI = {.tag = REGISTER, .reg = {.i = 7, .size = 64}};
const Operand_ R8 = {.tag = REGISTER, .reg = {.i = 8, .size = 64}};
const Operand_ R9 = {.tag = REGISTER, .reg = {.i = 9, .size = 64}};
const Operand_ R10 = {.tag = REGISTER, .reg = {.i = 10, .size = 64}};
const Operand_ R11 = {.tag = REGISTER, .reg = {.i = 11, .size = 64}};
const Operand_ R12 = {.tag = REGISTER, .reg = {.i = 12, .size = 64}};
const Operand_ R13 = {.tag = REGISTER, .reg = {.i = 13, .size = 64}};
const Operand_ R14 = {.tag = REGISTER, .reg = {.i = 14, .size = 64}};
const Operand_ R15 = {.tag = REGISTER, .reg = {.i = 15, .size = 64}};

Operand_ expr_registers[] = {RAX, RCX, RDX, RBX, RSI, RDI, R8, R9, R10, R11, R12, R13, R14, R15};
size_t expr_registers_size = sizeof expr_registers / sizeof expr_registers[0];
Operand_ argument_registers[] = {RDI, RSI, RDX, RCX, R8, R9};
Operand_ non_volatile_registers[] = {RBX, RSI, RDI, RBP, R8, R9, R10, R11, R12, R13, R14, R15};
size_t non_volatile_registers_size =
    sizeof non_volatile_registers / sizeof non_volatile_registers[0];

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
    ctx.local_relocations[ctx.local_relocations_len++] = (ElfRelocationEntry){
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

uint8_t modrm(ModrmMod mod, uint8_t reg, ModrmRm rm) {
    assert(mod <= 0b11);
    assert(reg <= 0b111);
    assert(rm <= 0b111);
    return (uint8_t)((mod << 6) | (reg << 3) | rm);
}

/**
 * @see https://en.wikipedia.org/wiki/REX_prefix#Instruction_encoding
 */
uint8_t rex(bool w, bool r, bool x, bool b) {
    return (1 << 6) | (w << 3) | (r << 2) | (x << 1) | b;
}

bool immediate_fits_i32(OperandImmediate i) {
    return i.value >= INT32_MIN && i.value <= INT32_MAX;
}

void asm_nop() {
    ctx.text[ctx.text_len++] = 0x90;
}

void asm_canary(size_t size) {
    memset(&ctx.text[ctx.text_len], 0xAA, size);
    ctx.text_len += size;
}

void asm_pad(size_t size) {
    memset(&ctx.text[ctx.text_len], 0x90, size);
    ctx.text_len += size;
}

void asm_lea(Operand_ a, Operand_ b) {
    if (b.tag == REGISTER) {
        fprintf(stderr, "asm_lea _ r\n");
        assert(false);
        return;
    }
    if (a.tag == REGISTER && b.tag == MEMORY && b.memory.mode == SYMBOL_LOCAL) {
        ctx.text[ctx.text_len++] = rex(true, a.reg.i >= 8, false, false);
        ctx.text[ctx.text_len++] = 0x8D;
        ctx.text[ctx.text_len++] = modrm(0, a.reg.i & 0b111, RM_DI);
        relocation_add_local(PC32, b.memory.offset, ctx.text_len);
        asm_canary(4);
        return;
    }
    if (a.tag == REGISTER && b.tag == MEMORY && b.memory.mode == REL_RBP) {
        ctx.text[ctx.text_len++] = rex(true, a.reg.i >= 8, false, false);
        ctx.text[ctx.text_len++] = 0x8D;
        ctx.text[ctx.text_len++] = modrm(MOD_INDIRECT_DISP32, a.reg.i & 0b111, RM_DI);
        memcpy(&ctx.text[ctx.text_len], &b.memory.offset, 4);
        ctx.text_len += 4;
        return;
    }
    fprintf(stderr, "TODO asm_lea %d %d\n", a.tag, b.tag);
    asm_nop();
}

void asm_mov(Operand_ a, Operand_ b) {
    if (a.tag == IMMEDIATE) {
        fprintf(stderr, "asm_mov mov _\n");
        assert(false);
        return;
    }
    if (a.tag == REGISTER && b.tag == REGISTER) {
        if (a.reg.i == b.reg.i && a.reg.indirect == false && b.reg.indirect == false) return;
        ctx.text[ctx.text_len++] = rex(true, b.reg.i >= 8, false, a.reg.i >= 8);
        if (a.reg.indirect) {
            if (a.reg.size == 8) {
                ctx.text[ctx.text_len++] = 0x88;
                ctx.text[ctx.text_len++] = modrm(MOD_INDIRECT, b.reg.i & 0b111, a.reg.i & 0b111);
                return;
            }
            ctx.text[ctx.text_len++] = 0x89;
            ctx.text[ctx.text_len++] = modrm(MOD_INDIRECT, b.reg.i & 0b111, a.reg.i & 0b111);
            return;
        }
        if (b.reg.indirect) {
            ctx.text[ctx.text_len++] = 0x8B;
            ctx.text[ctx.text_len++] = modrm(MOD_INDIRECT, a.reg.i & 0b111, b.reg.i & 0b111);
            return;
        }
        ctx.text[ctx.text_len++] = 0x89;
        ctx.text[ctx.text_len++] = modrm(MOD_REGISTER, b.reg.i & 0b111, a.reg.i & 0b111);
        return;
    }
    if (a.tag == REGISTER && b.tag == IMMEDIATE) {
        if (a.reg.indirect) {
            Operand_ tmp = expr_registers[ctx.expr_registers_busy];
            asm_mov(tmp, b);
            asm_mov(a, tmp);
            return;
        }
        if (immediate_fits_i32(b.immediate)) {
            ctx.text[ctx.text_len++] = rex(true, false, false, a.reg.i >= 8);
            ctx.text[ctx.text_len++] = 0xC7;
            ctx.text[ctx.text_len++] = modrm(MOD_REGISTER, 0, a.reg.i & 0b111);
            memcpy(&ctx.text[ctx.text_len], &b.immediate.value, 4);
            ctx.text_len += 4;
            return;
        }
        ctx.text[ctx.text_len++] = rex(true, false, false, a.reg.i >= 8);
        ctx.text[ctx.text_len++] = 0xB8 + (a.reg.i & 0b111);
        memcpy(&ctx.text[ctx.text_len], &b.immediate.value, 8);
        ctx.text_len += 8;
        return;
    }
    if (a.tag == REGISTER && b.tag == MEMORY && b.memory.mode == SYMBOL_LOCAL) {
        asm_lea(a, b);
        return;
    }
    if (a.tag == REGISTER && b.tag == MEMORY && b.memory.mode == REL_RBP) {
        ctx.text[ctx.text_len++] = rex(true, a.reg.i >= 8, false, false);
        ctx.text[ctx.text_len++] = 0x8B;
        ctx.text[ctx.text_len++] = modrm(MOD_INDIRECT_DISP32, a.reg.i & 0b111, RM_DI);
        memcpy(&ctx.text[ctx.text_len], &b.memory.offset, 4);
        ctx.text_len += 4;
        return;
    }
    if (a.tag == MEMORY && a.memory.mode == REL_RBP && b.tag == REGISTER) {
        ctx.text[ctx.text_len++] = rex(true, a.reg.i >= 8, false, false);
        ctx.text[ctx.text_len++] = 0x89;
        ctx.text[ctx.text_len++] = modrm(MOD_INDIRECT_DISP32, b.reg.i & 0b111, RM_DI);
        memcpy(&ctx.text[ctx.text_len], &a.memory.offset, 4);
        ctx.text_len += 4;
        return;
    }
    if (a.tag == MEMORY && a.memory.mode == REL_RBP && b.tag == IMMEDIATE) {
        Operand_ tmp = expr_registers[ctx.expr_registers_busy];
        asm_mov(tmp, b);
        asm_mov(a, tmp);
        return;
    }
    if (a.tag == MEMORY && b.tag == MEMORY) {
        Operand_ tmp = expr_registers[ctx.expr_registers_busy];
        asm_mov(tmp, b);
        asm_mov(a, tmp);
        return;
    }
    fprintf(stderr, "TODO asm_mov %d %d\n", a.tag, b.tag);
    asm_nop();
}

void asm_add(Operand_ a, Operand_ b) {
    if (a.tag == REGISTER && b.tag == REGISTER) {
        ctx.text[ctx.text_len++] = rex(true, a.reg.i >= 8, false, b.reg.i >= 8);
        ctx.text[ctx.text_len++] = 0x03;
        ctx.text[ctx.text_len++] = modrm(MOD_REGISTER, a.reg.i & 0b111, b.reg.i & 0b111);
        return;
    }
    if ((a.tag == REGISTER && b.tag == IMMEDIATE && !immediate_fits_i32(b.immediate)) ||
        (a.tag == MEMORY && b.tag == MEMORY)) {
        Operand_ tmp = expr_registers[ctx.expr_registers_busy];
        asm_mov(tmp, b);
        asm_add(a, tmp);
        return;
    }
    if (a.tag == REGISTER && b.tag == IMMEDIATE) {
        if (a.reg.indirect) {
            Operand_ tmp = expr_registers[ctx.expr_registers_busy];
            asm_mov(tmp, b);
            asm_add(a, tmp);
            return;
        }
        ctx.text[ctx.text_len++] = rex(true, a.reg.i >= 8, false, false);
        ctx.text[ctx.text_len++] = 0x81;
        ctx.text[ctx.text_len++] = modrm(MOD_REGISTER, 0, a.reg.i & 0b111);
        int32_t i = b.immediate.value;
        memcpy(&ctx.text[ctx.text_len], &i, 4);
        ctx.text_len += 4;
        return;
    }
    if (a.tag == REGISTER && b.tag == MEMORY && b.memory.mode == REL_RBP) {
        ctx.text[ctx.text_len++] = rex(true, a.reg.i >= 8, false, false);
        ctx.text[ctx.text_len++] = 0x03;
        ctx.text[ctx.text_len++] = modrm(MOD_INDIRECT_DISP32, a.reg.i & 0b111, RM_DI);
        memcpy(&ctx.text[ctx.text_len], &b.memory.offset, 4);
        ctx.text_len += 4;
        return;
    }
    if (a.tag == MEMORY && a.memory.mode == REL_RBP && b.tag == REGISTER) {
        ctx.text[ctx.text_len++] = rex(true, b.reg.i >= 8, false, false);
        ctx.text[ctx.text_len++] = 0x01;
        ctx.text[ctx.text_len++] = modrm(MOD_INDIRECT_DISP32, b.reg.i & 0b111, RM_DI);
        memcpy(&ctx.text[ctx.text_len], &a.memory.offset, 4);
        ctx.text_len += 4;
        return;
    }
    fprintf(stderr, "TODO asm_add %d %d\n", a.tag, b.tag);
    asm_nop();
}

void asm_sub(Operand_ a, Operand_ b) {
    if (a.tag == REGISTER && b.tag == REGISTER) {
        ctx.text[ctx.text_len++] = rex(true, a.reg.i >= 8, false, b.reg.i >= 8);
        ctx.text[ctx.text_len++] = 0x2B;
        ctx.text[ctx.text_len++] = modrm(MOD_REGISTER, a.reg.i & 0b111, b.reg.i);
        return;
    }
    if ((a.tag == REGISTER && b.tag == IMMEDIATE && !immediate_fits_i32(b.immediate)) ||
        (a.tag == MEMORY && b.tag == MEMORY)) {
        Operand_ tmp = expr_registers[ctx.expr_registers_busy];
        asm_mov(tmp, b);
        asm_sub(a, tmp);
        return;
    }
    if (a.tag == REGISTER && b.tag == IMMEDIATE) {
        if (a.reg.indirect) {
            Operand_ tmp = expr_registers[ctx.expr_registers_busy];
            asm_mov(tmp, b);
            asm_sub(a, tmp);
            return;
        }
        ctx.text[ctx.text_len++] = rex(true, a.reg.i >= 8, false, false);
        ctx.text[ctx.text_len++] = 0x81;
        ctx.text[ctx.text_len++] = modrm(MOD_REGISTER, 5, a.reg.i & 0b111);
        int32_t i = b.immediate.value;
        memcpy(&ctx.text[ctx.text_len], &i, 4);
        ctx.text_len += 4;
        return;
    }
    if (a.tag == MEMORY && a.memory.mode == REL_RBP && b.tag == REGISTER) {
        ctx.text[ctx.text_len++] = rex(true, b.reg.i >= 8, false, false);
        ctx.text[ctx.text_len++] = 0x29;
        ctx.text[ctx.text_len++] = modrm(MOD_INDIRECT_DISP32, b.reg.i & 0b111, RM_DI);
        memcpy(&ctx.text[ctx.text_len], &a.memory.offset, 4);
        ctx.text_len += 4;
        return;
    }
    fprintf(stderr, "TODO asm_sub %d %d\n", a.tag, b.tag);
    asm_nop();
}

void asm_push(Operand_ a) {
    if (a.tag == REGISTER) {
        if (a.reg.i >= 8) {
            ctx.text[ctx.text_len++] = rex(false, false, false, true);
        }
        ctx.text[ctx.text_len++] = 0x50 + (a.reg.i & 0b111);
        ctx.stack_offset -= 8;
        return;
    }
    fprintf(stderr, "TODO asm_push\n");
    asm_nop();
}

void asm_pop(Operand_ a) {
    if (a.tag == REGISTER) {
        if (a.reg.i >= 8) {
            ctx.text[ctx.text_len++] = rex(false, false, false, true);
        }
        ctx.text[ctx.text_len++] = 0x58 + (a.reg.i & 0b111);
        ctx.stack_offset += 8;
        return;
    }
    fprintf(stderr, "TODO asm_pop\n");
    asm_nop();
}

void asm_ret() {
    ctx.text[ctx.text_len++] = 0xC3;
}

void asm_call_global(Symbol* symbol) {
    // correct stack alignment because call implicitly pushes 8 bytes onto the stack
    asm_sub(RSP, immediate(8));

    ctx.text[ctx.text_len++] = 0xE8;
    ctx.global_relocations[ctx.global_relocations_len++] = (SymbolRelocation){
        .symbol = symbol,
        .type = PLT32,
        .offset = ctx.text_len,
    };
    asm_canary(4);

    asm_add(RSP, immediate(8));
}

void asm_je(int32_t rel) {
    ctx.text[ctx.text_len++] = 0x0F;
    ctx.text[ctx.text_len++] = 0x84;
    memcpy(&ctx.text[ctx.text_len], &rel, 4);
    ctx.text_len += 4;
}

void asm_jne(int32_t rel) {
    ctx.text[ctx.text_len++] = 0x0F;
    ctx.text[ctx.text_len++] = 0x85;
    memcpy(&ctx.text[ctx.text_len], &rel, 4);
    ctx.text_len += 4;
}

void asm_jmp(int32_t rel) {
    ctx.text[ctx.text_len++] = 0xE9;
    memcpy(&ctx.text[ctx.text_len], &rel, 4);
    ctx.text_len += 4;
}

void asm_cmp(Operand_ a, Operand_ b) {
    Operand_ tmp = b;
    if (a.tag == IMMEDIATE) {
        tmp = expr_registers[ctx.expr_registers_busy++];
        asm_mov(tmp, a);
        asm_cmp(tmp, b);
        ctx.expr_registers_busy--;
        return;
    }
    if (b.tag != REGISTER) {
        tmp = expr_registers[ctx.expr_registers_busy];
        asm_mov(tmp, b);
    }
    if (a.tag == REGISTER) {
        ctx.text[ctx.text_len++] = rex(true, tmp.reg.i >= 8, false, a.reg.i >= 8);
        ctx.text[ctx.text_len++] = 0x3B;
        ctx.text[ctx.text_len++] = modrm(MOD_REGISTER, tmp.reg.i & 0b111, a.reg.i & 0b111);
        return;
    }
    if (a.tag == MEMORY && a.memory.mode == REL_RBP) {
        ctx.text[ctx.text_len++] = rex(true, tmp.reg.i >= 8, false, false);
        ctx.text[ctx.text_len++] = 0x39;
        ctx.text[ctx.text_len++] = modrm(MOD_INDIRECT_DISP32, tmp.reg.i & 0b111, RM_DI);
        memcpy(&ctx.text[ctx.text_len], &a.memory.offset, 4);
        ctx.text_len += 4;
        return;
    }
    fprintf(stderr, "TODO asm_cmp %d %d\n", a.tag, b.tag);
    asm_nop();
}

void asm_test(Operand_ a, Operand_ b) {
    ctx.text[ctx.text_len++] = rex(true, a.reg.i >= 8, false, b.reg.i >= 8);
    ctx.text[ctx.text_len++] = 0x85;
    ctx.text[ctx.text_len++] = modrm(MOD_REGISTER, a.reg.i & 0b111, b.reg.i & 0b111);
}

void asm_setx(Operand_ a, uint8_t opcode) {
    if (a.reg.i >= 8) {
        ctx.text[ctx.text_len++] = rex(false, false, false, true);
    }
    if (a.tag == REGISTER) {
        ctx.text[ctx.text_len++] = 0x0F;
        ctx.text[ctx.text_len++] = opcode;
        ctx.text[ctx.text_len++] = modrm(MOD_REGISTER, 0, a.reg.i & 0b111);
        return;
    }
    if (a.tag == MEMORY && a.memory.mode == REL_RBP) {
        ctx.text[ctx.text_len++] = 0x0F;
        ctx.text[ctx.text_len++] = opcode;
        ctx.text[ctx.text_len++] = modrm(MOD_INDIRECT_DISP32, 0, RM_DI);
        memcpy(&ctx.text[ctx.text_len], &a.memory.offset, 4);
        ctx.text_len += 4;
        return;
    }
    fprintf(stderr, "TODO asm_set* %d\n", a.tag);
    asm_nop();
}

void asm_sete(Operand_ a) {
    return asm_setx(a, 0x94);
}

void asm_setne(Operand_ a) {
    return asm_setx(a, 0x95);
}

void asm_setle(Operand_ a) {
    return asm_setx(a, 0x9E);
}

void asm_setl(Operand_ a) {
    return asm_setx(a, 0x9C);
}

void asm_setge(Operand_ a) {
    return asm_setx(a, 0x9D);
}

void asm_setg(Operand_ a) {
    return asm_setx(a, 0x9F);
}

void asm_idiv(Operand_ a) {
    if (a.tag == IMMEDIATE) {
        Operand_ tmp = expr_registers[ctx.expr_registers_busy];
        asm_mov(tmp, a);
        asm_idiv(tmp);
        return;
    }
    if (a.tag == REGISTER) {
        ctx.text[ctx.text_len++] = rex(true, false, false, a.reg.i >= 8);
        ctx.text[ctx.text_len++] = 0xF7;
        ctx.text[ctx.text_len++] = modrm(MOD_REGISTER, 7, a.reg.i & 0b111);
        return;
    }
    fprintf(stderr, "TODO asm_idiv %d\n", a.tag);
    asm_nop();
}

void write_elf(FILE* out_file) {
    uint8_t sections[1 << 14] = {0};
    size_t sections_len = 0;

    uint64_t section_text_offset = sizeof elf_header + sections_len;
    memcpy(&sections[sections_len], &ctx.text, ctx.text_len);
    sections_len += ctx.text_len;

    uint64_t section_rodata_offset = sizeof elf_header + sections_len;
    memcpy(&sections[sections_len], &ctx.rodata, ctx.rodata_len);
    sections_len += ctx.rodata_len;

    uint64_t section_shstrtab_offset = sizeof elf_header + sections_len;
    char* section_names[7] = {".null",   ".text",   ".rodata",  ".shstrtab",
                              ".strtab", ".symtab", ".rel.text"};
    uint32_t shstrtab_offsets[7] = {0};
    uint8_t shstrtab[1 << 10] = {0};
    size_t shstrtab_len = 0;
    for (size_t i = 0; i < sizeof section_names / sizeof section_names[0]; i++) {
        shstrtab_offsets[i] = shstrtab_len;
        memcpy(&shstrtab[shstrtab_len], section_names[i], strlen(section_names[i]));
        shstrtab_len += strlen(section_names[i]) + 1;
    }
    memcpy(&sections[sections_len], &shstrtab, shstrtab_len);
    sections_len += shstrtab_len;

    uint64_t section_strtab_offset = sizeof elf_header + sections_len;
    memcpy(&sections[sections_len], &ctx.symbol_names, ctx.symbol_names_len);
    sections_len += ctx.symbol_names_len;

    uint64_t section_symtab_offset = sizeof elf_header + sections_len;
    memcpy(&sections[sections_len], &ctx.symbols_local,
           sizeof(ElfSymbolEntry) * ctx.symbols_local_len);
    sections_len += sizeof(ElfSymbolEntry) * ctx.symbols_local_len;

    size_t global_scope_size = stack_scope_size(0);
    for (size_t i = 0; i < global_scope_size; i++) {
        Symbol* symbol = &ctx.symbols[i];
        symbol->index = ctx.symbols_local_len + i;
        ElfSymbolEntry entry = symbol->entry;
        memcpy(&sections[sections_len], &entry, sizeof entry);
        sections_len += sizeof entry;
    }

    uint64_t section_reltext_offset = sizeof elf_header + sections_len;

    memcpy(&sections[sections_len], &ctx.local_relocations,
           sizeof(ElfRelocationEntry) * ctx.local_relocations_len);
    sections_len += sizeof(ElfRelocationEntry) * ctx.local_relocations_len;

    for (size_t i = 0; i < ctx.global_relocations_len; i++) {
        SymbolRelocation sym = ctx.global_relocations[i];
        ElfRelocationEntry entry = (ElfRelocationEntry){
            .offset = sym.offset,
            .type = sym.type,
            .sym = sym.symbol->index,
            // TODO: explain
            .addend = -4,

        };
        memcpy(&sections[sections_len], &entry, sizeof entry);
        sections_len += sizeof entry;
    }

    uint64_t section_header_table_offset = sizeof elf_header + sections_len;
    ElfSectionHeader null = {0};
    memcpy(&sections[sections_len], &null, sizeof null);
    sections_len += sizeof null;

    ElfSectionHeader section_text = {
        .name = shstrtab_offsets[1],
        .type = 1,
        .flags = 0x02 | 0x04,
        .addr = 0,
        .offset = section_text_offset,
        .size = ctx.text_len,
    };
    memcpy(&sections[sections_len], &section_text, sizeof section_text);
    sections_len += sizeof section_text;

    ElfSectionHeader section_rodata = {
        .name = shstrtab_offsets[2],
        .type = 1,
        .flags = 0x02 | 0x10 | 0x20,
        .offset = section_rodata_offset,
        .size = ctx.rodata_len,
    };
    memcpy(&sections[sections_len], &section_rodata, sizeof section_rodata);
    sections_len += sizeof section_rodata;

    ElfSectionHeader section_shstrtab = {
        .name = shstrtab_offsets[3],
        .type = 3,
        .offset = section_shstrtab_offset,
        .size = shstrtab_len,
    };
    memcpy(&sections[sections_len], &section_shstrtab, sizeof section_shstrtab);
    sections_len += sizeof section_shstrtab;

    ElfSectionHeader section_strtab = {
        .name = shstrtab_offsets[4],
        .type = 3,
        .offset = section_strtab_offset,
        .size = ctx.symbol_names_len,
    };
    memcpy(&sections[sections_len], &section_strtab, sizeof section_strtab);
    sections_len += sizeof section_strtab;

    ElfSectionHeader section_symtab = {
        .name = shstrtab_offsets[5],
        .type = 2,
        .offset = section_symtab_offset,
        .size = sizeof(ElfSymbolEntry) * (ctx.symbols_local_len + global_scope_size),
        .link = 4,                     // index of .strtab section
        .info = ctx.symbols_local_len, // index of the first non-local symbol
        .entsize = sizeof(ElfSymbolEntry),
    };
    memcpy(&sections[sections_len], &section_symtab, sizeof section_symtab);
    sections_len += sizeof section_symtab;

    ElfSectionHeader section_reltext = {
        .name = shstrtab_offsets[6],
        .type = 4,
        .offset = section_reltext_offset,
        .size =
            sizeof(ElfRelocationEntry) * (ctx.local_relocations_len + ctx.global_relocations_len),
        .link = 5, // index of a .symtab section
        .info = 1, // index of a .rel.text section
        .entsize = sizeof(ElfRelocationEntry),
    };
    memcpy(&sections[sections_len], &section_reltext, sizeof section_reltext);
    sections_len += sizeof section_reltext;

    assert(sections_len < sizeof sections);

    elf_header.shoff = section_header_table_offset;
    elf_header.shnum = 7;
    elf_header.shstrndx = 3;

    fwrite(&elf_header, 1, sizeof elf_header, out_file);
    fwrite(sections, 1, sections_len, out_file);
}
