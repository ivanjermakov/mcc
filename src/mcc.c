#include "core.h"
#include "lex.h"
#include "parse.h"

int32_t main(int32_t argc, char* argv[]) {
    if (argc < 3) {
        fprintf(stderr, "usage:\n  mcc source out\n");
        return 1;
    }
    char* source_path = argv[1];
    FILE* source_file = fopen(source_path, "r");
    input_size = fread(&input_buf, 1, sizeof input_buf, source_file);
    fclose(source_file);

    Token token;
    while (true) {
        token = next_token();
        if (token.type == NONE) break;
        token_buf[token_size++] = token;
    }
    for (size_t i = 0; i < token_size; i++) {
        Token token = token_buf[i];
        printf("%3zu %-10s'", i, token_name[token.type]);
        fwrite(&input_buf[token.span.start], 1, token.span.len, stdout);
        printf("'\n");
    }

    bool ok = visit_program();
    if (!ok) {
        fprintf(stderr, "error parsing program\n");
        return 1;
    }

    uint8_t sections_buf[1 << 10] = {0};
    size_t sections_size = 0;

    uint64_t section_text_offset = sizeof elf_header + sections_size;
    memcpy(&sections_buf[sections_size], &section_text_buf, text_size);
    sections_size += text_size;

    uint64_t section_rodata_offset = sizeof elf_header + sections_size;
    memcpy(&sections_buf[sections_size], &section_rodata_buf, rodata_size);
    sections_size += rodata_size;

    uint64_t section_shstrtab_offset = sizeof elf_header + sections_size;
    char* section_names[3] = {".text", ".rodata", ".shstrtab"};
    uint32_t section_shstrtab_offsets[3] = {0};
    uint8_t section_shstrtab_buf[1 << 10] = {0};
    size_t shstrtab_size = 0;
    for (size_t i = 0; i < sizeof section_names / sizeof section_names[0]; i++) {
        section_shstrtab_offsets[i] = shstrtab_size;
        memcpy(&section_shstrtab_buf[shstrtab_size], section_names[i], strlen(section_names[i]));
        shstrtab_size += strlen(section_names[i]) + 1;
    }
    memcpy(&sections_buf[sections_size], &section_shstrtab_buf, shstrtab_size);
    sections_size += shstrtab_size;

    uint64_t section_header_table_offset = sizeof elf_header + sections_size;
    ElfSectionHeader null = {0};
    memcpy(&sections_buf[sections_size], &null, sizeof null);
    sections_size += sizeof null;

    ElfSectionHeader text = {
        .name = section_shstrtab_offsets[0],
        .type = 1,
        .flags = 0x02 | 0x04,
        .addr = section_text_offset,
        .offset = section_text_offset,
        .size = text_size
    };
    memcpy(&sections_buf[sections_size], &text, sizeof text);
    sections_size += sizeof text;

    ElfSectionHeader rodata = {
        .name = section_shstrtab_offsets[1],
        .type = 1,
        .flags = 0x02,
        .addr = section_rodata_offset,
        .offset = section_rodata_offset,
        .size = rodata_size
    };
    memcpy(&sections_buf[sections_size], &rodata, sizeof rodata);
    sections_size += sizeof rodata;

    ElfSectionHeader shstrtab = {
        .name = section_shstrtab_offsets[2],
        .type = 3,
        .addr = section_shstrtab_offset,
        .offset = section_shstrtab_offset,
        .size = shstrtab_size
    };
    memcpy(&sections_buf[sections_size], &shstrtab, sizeof shstrtab);
    sections_size += sizeof shstrtab;

    elf_header.shoff = section_header_table_offset;
    elf_header.shnum = 4;
    elf_header.shstrndx = 3;

    char* out_path = argv[2];
    FILE* out_file = fopen(out_path, "w");
    fwrite(&elf_header, 1, sizeof elf_header, out_file);
    fwrite(sections_buf, 1, sections_size, out_file);
    fclose(out_file);

    return 0;
}
