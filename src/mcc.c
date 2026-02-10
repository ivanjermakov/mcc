#include "core.h"
#include "lex.h"
#include "parse.h"

int32_t main(int32_t argc, char* argv[]) {
    if (argc < 3) {
        fprintf(stderr, "missing file arguments\n");
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
    uint8_t section_null[0x40] = {0};
    memcpy(&sections_buf[sections_size], &section_null, sizeof section_null);
    sections_size += sizeof section_null;

    uint8_t section_text[0x40] = {0};
    memcpy(&section_text[0x00], &section_shstrtab_offsets[0], 4);
    section_text[0x04] = 1;
    section_text[0x08] = 0x02 | 0x04;
    memcpy(&section_text[0x10], &section_text_offset, 8);
    memcpy(&section_text[0x18], &section_text_offset, 8);
    memcpy(&section_text[0x20], &text_size, 8);
    memcpy(&sections_buf[sections_size], &section_text, sizeof section_text);
    sections_size += sizeof section_text;

    uint8_t section_rodata[0x40] = {0};
    memcpy(&section_rodata[0x00], &section_shstrtab_offsets[1], 4);
    section_rodata[0x04] = 1;
    section_rodata[0x08] = 0x02;
    memcpy(&section_rodata[0x10], &section_rodata_offset, 8);
    memcpy(&section_rodata[0x18], &section_rodata_offset, 8);
    memcpy(&section_rodata[0x20], &rodata_size, 8);
    memcpy(&sections_buf[sections_size], &section_rodata, sizeof section_rodata);
    sections_size += sizeof section_rodata;

    uint8_t section_shstrtab[0x40] = {0};
    memcpy(&section_shstrtab[0x00], &section_shstrtab_offsets[2], 4);
    section_shstrtab[0x04] = 3;
    memcpy(&section_shstrtab[0x10], &section_shstrtab_offset, 8);
    memcpy(&section_shstrtab[0x18], &section_shstrtab_offset, 8);
    memcpy(&section_shstrtab[0x20], &shstrtab_size, 8);
    memcpy(&sections_buf[sections_size], &section_shstrtab, sizeof section_shstrtab);
    sections_size += sizeof section_shstrtab;

    // TODO: complete ELF
    memcpy(&elf_header[0x28], &section_header_table_offset, 8);
    uint16_t header_count = 4;
    memcpy(&elf_header[0x3C], &header_count, 2);
    uint16_t shstr_index = 3;
    memcpy(&elf_header[0x3E], &shstr_index, 2);

    char* out_path = argv[2];
    FILE* out_file = fopen(out_path, "w");
    fwrite(&elf_header, 1, sizeof elf_header, out_file);
    fwrite(sections_buf, 1, sections_size, out_file);
    fclose(out_file);

    return 0;
}
