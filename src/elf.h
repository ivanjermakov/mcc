#pragma once
#include "stdint.h"

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

/**
 * @see https://refspecs.linuxbase.org/elf/gabi4+/ch4.symtab.html
 */
typedef struct {
    uint32_t name;
    uint8_t info;
    uint8_t other;
    uint16_t shndx;
    uint64_t value;
    uint64_t size;
} ElfSymbolEntry;
