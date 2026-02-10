#include "core.h"
#include "lex.h"
#include "parse.h"

int32_t main(int32_t argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "missing file argument\n");
        return 1;
    }
    char* source_path = argv[1];
    FILE* source_file = fopen(source_path, "r");
    input_size = fread(&input_buf, 1, sizeof input_buf, source_file);

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

    return 0;
}
