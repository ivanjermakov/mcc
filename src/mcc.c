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
    ctx.input_len = fread(&ctx.input, 1, sizeof ctx.input, source_file);
    fclose(source_file);

    Token token;
    while (true) {
        token = next_token();
        if (token.type == NONE) break;
        ctx.tokens[ctx.tokens_len++] = token;
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
    assert(ctx.stack_len == 1);

    char* out_path = argv[2];
    FILE* out_file = fopen(out_path, "w");
    write_elf(out_file);
    fclose(out_file);

    return 0;
}
