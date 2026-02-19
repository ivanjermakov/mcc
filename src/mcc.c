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
    while (token_offset < input_size) {
        token = next_token();
        if (token.type == NONE) break;
        token_buf[token_size++] = token;
    }
    // for (size_t i = 0; i < token_size; i++) {
    //     Token token = token_buf[i];
    //     printf("%3zu %-10s'", i, token_name[token.type]);
    //     fwrite(&input_buf[token.span.start], 1, token.span.len, stdout);
    //     printf("'\n");
    // }

    bool ok = visit_program();
    if (!ok) {
        fprintf(stderr, "error parsing program\n");
        return 1;
    }
    assert(stack_size == 1);

    char* out_path = argv[2];
    FILE* out_file = fopen(out_path, "w");
    write_elf(out_file);
    fclose(out_file);

    return 0;
}
