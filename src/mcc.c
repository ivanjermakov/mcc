#include "stdbool.h"
#include "stdint.h"
#include "stdio.h"
#include "string.h"

struct Span {
    size_t start;
    size_t len;
};

enum TokenType {
    NONE = 0,
    IDENT,
    INT,
    STRING,
    HASH,
    SEMICOLON,
    O_BRACE,
    C_BRACE,
    O_PAREN,
    C_PAREN,
    COMMA,
    ASTERISK,
};
char* token_literal[] = {
    0, 0, 0, 0, "#", ";", "{", "}", "(", ")", ",", "*",
};
size_t token_literal_size = sizeof token_literal / sizeof token_literal[0];
char* token_name[] = {
    "NONE",    "IDENT",   "INT",     "STRING",  "HASH",  "SEMICOLON",
    "O_BRACE", "C_BRACE", "O_PAREN", "C_PAREN", "COMMA", "ASTERISK",
};

struct Token {
    struct Span span;
    enum TokenType type;
};

char input_buf[1 << 10];
size_t input_size = 0;

struct Token token_buf[1 << 10];
size_t token_size = 0;
size_t token_offset = 0;

bool is_printable(char c) {
    return c >= 32 && c <= 127;
}

bool is_alpha(char c) {
    return c >= 'A' && c <= 'z';
}

bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

bool is_ident(char c, size_t len) {
    return is_alpha(c) || c == '_' || (len > 0 && is_digit(c));
}

struct Token next_token() {
    struct Token token = {0};
    while (token_offset < input_size &&
           (!is_printable(input_buf[token_offset]) || input_buf[token_offset] == ' ')) {
        token_offset++;
    }
    if (token_offset >= input_size) return token;

    token.span.start = token_offset;
    for (size_t i = 0; i < token_literal_size; i++) {
        if (token_literal[i] == 0) continue;
        if (strncmp(&input_buf[token_offset], token_literal[i], strlen(token_literal[i])) == 0) {
            token.span.len = strlen(token_literal[i]);
            token.type = i;
            token_offset += token.span.len;
            return token;
        }
    }

    size_t ident_len = 0;
    while (token_offset < input_size && is_ident(input_buf[token_offset + ident_len], ident_len)) {
        ident_len++;
    }
    if (ident_len > 0) {
        token.span.len = ident_len;
        token.type = IDENT;
        token_offset += token.span.len;
        return token;
    }

    size_t int_len = 0;
    if (is_digit(input_buf[token_offset])) {
        while (token_offset < input_size && is_digit(input_buf[token_offset + int_len])) {
            int_len++;
        }
        token.span.len = int_len;
        token.type = INT;
        token_offset += token.span.len;
        return token;
    }

    // TODO: escape sequences
    size_t str_len = 0;
    if (input_buf[token_offset] == '"') {
        while (token_offset < input_size) {
            if (str_len > 0 && input_buf[token_offset + str_len] == '"') {
                str_len++;
                token.span.len = str_len;
                token.type = STRING;
                token_offset += token.span.len;
                return token;
            }
            str_len++;
        }
        printf("non-terminated string at %zu\n", token_offset);
        return token;
    }

    printf("unknown token '%c' at %zu\n", input_buf[token_offset], token_offset);
    return token;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "missing file argument");
        return 1;
    }
    char* source_path = argv[1];
    FILE* source_file = fopen(source_path, "r");
    input_size = fread(&input_buf, 1, sizeof input_buf, source_file);

    struct Token token;
    while (true) {
        token = next_token();
        if (token.type == NONE) break;
        printf("%-10s'", token_name[token.type]);
        fwrite(&input_buf[token.span.start], 1, token.span.len, stdout);
        printf("'\n");
    }

    return 0;
}
