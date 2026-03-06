#pragma once
#include "core.h"

bool is_printable(char c) {
    return c >= 32 && c <= 126;
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
    token.span.start = ctx.token_offset;
    if (ctx.token_offset >= ctx.input_len) return token;

    if (ctx.input[ctx.token_offset] == '"' && !ctx.inside_char) {
        ctx.inside_string = !ctx.inside_string;
        token.span.len = 1;
        token.type = DQUOTE;
        ctx.token_offset += token.span.len;
        return token;
    }
    if (ctx.input[ctx.token_offset] == '\'' && !ctx.inside_string) {
        ctx.inside_char = !ctx.inside_char;
        token.span.len = 1;
        token.type = QUOTE;
        ctx.token_offset += token.span.len;
        return token;
    }
    if (ctx.inside_string || ctx.inside_char) {
        if (ctx.input[ctx.token_offset] == '\\') {
            // TODO: wrap str_len++ into advance() to check for EOF
            uint8_t c = ctx.input[ctx.token_offset];
            if (is_digit(c) || c == 'x' || c == 'u' || c == 'U') {
                fprintf(stderr, "TODO byte escape sequence\n");
                return token;
            }
            token.span.len = 2;
            token.type = ESCAPE;
            ctx.token_offset += token.span.len;
            return token;
        }
    }
    if (ctx.inside_string) {
        size_t str_len = 0;
        while (ctx.token_offset < ctx.input_len) {
            if (ctx.input[ctx.token_offset + str_len] == '"' ||
                ctx.input[ctx.token_offset + str_len] == '\\') {
                token.span.len = str_len;
                token.type = STRING_PART;
                ctx.token_offset += token.span.len;
                return token;
            }
            str_len++;
        }
        fprintf(stderr, "non-terminated string at %zu\n", ctx.token_offset);
        return token;
    }
    if (ctx.inside_char) {
        token.span.len = 1;
        token.type = CHAR;
        ctx.token_offset += token.span.len;
        return token;
    }

    if (ctx.input[ctx.token_offset] == '/' && ctx.input[ctx.token_offset + 1] == '/') {
        while (ctx.token_offset < ctx.input_len && ctx.input[ctx.token_offset] != '\n') {
            ctx.token_offset++;
        }
        return next_token();
    }

    size_t skip = 0;
    if (ctx.token_offset < ctx.input_len &&
        (!is_printable(ctx.input[ctx.token_offset]) || ctx.input[ctx.token_offset] == ' ')) {
        skip++;
    }
    ctx.token_offset += skip;
    if (skip > 0) return next_token();

    for (size_t i = 0; i < token_literal_size; i++) {
        const char* t = token_literal[i];
        if (t == NULL) continue;
        if (strncmp(&ctx.input[ctx.token_offset], t, strlen(t)) == 0) {
            token.span.len = strlen(t);
            token.type = (TokenType)i;
            ctx.token_offset += token.span.len;
            return token;
        }
    }

    size_t ident_len = 0;
    while (ctx.token_offset < ctx.input_len &&
           is_ident(ctx.input[ctx.token_offset + ident_len], ident_len)) {
        ident_len++;
    }
    if (ident_len > 0) {
        token.span.len = ident_len;
        token.type = IDENT;
        ctx.token_offset += token.span.len;
        return token;
    }

    size_t int_len = 0;
    if (is_digit(ctx.input[ctx.token_offset])) {
        while (ctx.token_offset < ctx.input_len &&
               is_digit(ctx.input[ctx.token_offset + int_len])) {
            int_len++;
        }
        token.span.len = int_len;
        token.type = INT;
        ctx.token_offset += token.span.len;
        return token;
    }

    printf("unknown token '%c' at %zu\n", ctx.input[ctx.token_offset], ctx.token_offset);
    return token;
}
