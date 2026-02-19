#pragma once
#include "core.h"

bool is_printable(char c) {
    return c >= 32 && c <= 127;
}

bool is_alpha(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

bool is_ident(char c, size_t len) {
    return is_alpha(c) || c == '_' || (len > 0 && is_digit(c));
}

Token next_token() {
    Token token = {};
    token.span.start = token_offset;

    if (input_buf[token_offset] == '"') {
        inside_string = !inside_string;
        token.span.len = 1;
        token.type = DQUOTE;
        token_offset += token.span.len;
        return token;
    }
    if (inside_string) {
        size_t str_len = 0;
        if (input_buf[token_offset] == '\\') {
            // TODO: wrap str_len++ into advance() to check for EOF
            uint8_t c = input_buf[token_offset];
            if (is_digit(c) || c == 'x' || c == 'u' || c == 'U') {
                fprintf(stderr, "TODO byte escape sequence\n");
                return token;
            }
            token.span.len = 2;
            token.type = ESCAPE;
            token_offset += token.span.len;
            return token;
        }
        while (token_offset < input_size) {
            if (input_buf[token_offset + str_len] == '"' ||
                input_buf[token_offset + str_len] == '\\') {
                token.span.len = str_len;
                token.type = STRING_PART;
                token_offset += token.span.len;
                return token;
            }
            str_len++;
        }
        fprintf(stderr, "non-terminated string at %zu\n", token_offset);
        return token;
    }
    if (input_buf[token_offset] == '/' && input_buf[token_offset + 1] == '/') {
        while (token_offset < input_size && input_buf[token_offset] != '\n') {
            token_offset++;
        }
        return next_token();
    }

    while (token_offset < input_size &&
           (!is_printable(input_buf[token_offset]) || input_buf[token_offset] == ' ')) {
        token_offset++;
    }
    if (token_offset >= input_size) return token;
    token.span.start = token_offset;

    for (size_t i = 0; i < token_literal_size; i++) {
        const char* t = token_literal[i];
        if (t == NULL) continue;
        if (strncmp(&input_buf[token_offset], t, strlen(t)) == 0) {
            token.span.len = strlen(t);
            token.type = (TokenType)i;
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

    printf("unknown token '%c' at %zu\n", input_buf[token_offset], token_offset);
    return token;
}
