#include "stdbool.h"
#include "stdint.h"
#include "stdio.h"
#include "string.h"

typedef struct {
    size_t start;
    size_t len;
} Span;

typedef enum {
    NONE = 0,
    IDENT,
    INT,
    STRING,
    HASH,
    SEMI,
    O_BRACE,
    C_BRACE,
    O_PAREN,
    C_PAREN,
    COMMA,
    ASTERISK,
    AMPERSAND,
    IF,
    RETURN,
    OP_PLUS,
} TokenType;
char* token_literal[] = {
    0, 0, 0, 0, "#", ";", "{", "}", "(", ")", ",", "*", "&", "if", "return", "+",
};
size_t token_literal_size = sizeof token_literal / sizeof token_literal[0];
char* token_name[] = {"NONE",      "IDENT",   "INT",     "STRING",  "HASH",  "SEMI",
                      "O_BRACE",   "C_BRACE", "O_PAREN", "C_PAREN", "COMMA", "ASTERISK",
                      "AMPERSAND", "IF",      "RETURN",  "OP_PLUS"};

typedef struct {
    Span span;
    TokenType type;
} Token;

char input_buf[1 << 10];
size_t input_size = 0;

Token token_buf[1 << 10];
size_t token_size = 0;
size_t token_offset = 0;

size_t token_pos = 0;

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

Token next_token() {
    Token token = {0};
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

typedef struct {
    bool ok;
    Span* span;
} Int;
Int visit_int() {
    Int i = {.ok = true, .span = &token_buf[token_pos].span};
    token_pos++;
    return i;
}

typedef struct {
    bool ok;
    Span* span;
} String;
String visit_string() {
    String string = {.ok = true, .span = &token_buf[token_pos].span};
    token_pos++;
    return string;
}

typedef struct {
    bool ok;
    Span* span;
} Ident;
Ident visit_ident() {
    Ident ident = {.ok = true, .span = &token_buf[token_pos].span};
    token_pos++;
    return ident;
}

// type = IDENT "*"?
typedef struct {
    bool ok;
    Ident name;
    bool ptr;
} Type;
Type visit_type() {
    Type type = {0};
    type.name = visit_ident();
    if (!type.name.ok) return type;

    if (token_buf[token_pos].type == ASTERISK) {
        token_pos++;
        type.ptr = true;
    }
    type.ok = true;
    return type;
}

// op = "+" | "-" | "&" | "|" | "^" | "<<" | ">>"
//    | "==" | "!=" | "<" | ">" | "<=" | ">="
bool visit_op() {
    fprintf(stderr, "TODO visit_op\n");
    return false;
}

// unary = IDENT | INT | STRING | "&" IDENT | "(" expr ")"
bool visit_unary() {
    if (token_buf[token_pos].type == IDENT) {
        fprintf(stderr, "TODO\n");
        return false;
    } else if (token_buf[token_pos].type == INT) {
        Int i = visit_int();
        if (!i.ok) return i.ok;
    } else if (token_buf[token_pos].type == STRING) {
        String string = visit_string();
        if (!string.ok) return string.ok;
    } else if (token_buf[token_pos].type == AMPERSAND) {
        fprintf(stderr, "TODO\n");
        return false;
    } else if (token_buf[token_pos].type == O_PAREN) {
        fprintf(stderr, "TODO\n");
        return false;
    } else {
        fprintf(stderr, "TODO\n");
        return false;
    }
    return true;
}

// expr = unary (op unary)?
bool visit_expr() {
    bool ok = visit_unary();
    if (!ok) return ok;
    while (token_buf[token_pos].type >= OP_PLUS && token_buf[token_pos].type <= OP_PLUS) {
        ok = visit_op();
        if (!ok) return ok;
        ok = visit_unary();
        if (!ok) return ok;
    }
    return true;
}

// if = "if" "(" expr ")" block ";"
bool visit_if() {
    fprintf(stderr, "TODO if\n");
    return false;
}

// call = IDENT "(" expr ("," expr)* ")" ";"
bool visit_call() {
    Ident name = visit_ident();
    if (!name.ok) return name.ok;
    token_pos++;
    while (token_buf[token_pos].type != C_PAREN) {
        bool ok = visit_expr();
        if (!ok) return ok;
        if (token_buf[token_pos].type == SEMI) token_pos++;
    }
    token_pos++;
    token_pos++;

    return true;
}

// return = "return" expr ";"
bool visit_return() {
    token_pos++;
    bool ok = visit_expr();
    if (!ok) return ok;
    token_pos++;
    return true;
}

// statement = if | call | return
bool visit_statement() {
    if (token_buf[token_pos].type == IF) {
        bool ok = visit_if();
        if (!ok) return ok;
    } else if (token_buf[token_pos].type == IDENT) {
        bool ok = visit_call();
        if (!ok) return ok;
    } else if (token_buf[token_pos].type == RETURN) {
        bool ok = visit_return();
        if (!ok) return ok;
    } else {
        fprintf(stderr, "unexpected token when parsing statement at %zu: %s\n",
                token_buf[token_pos].span.start, token_name[token_buf[token_pos].type]);
        return false;
    }
    return true;
}

// block = "{" statement* "}"
bool visit_block() {
    token_pos++;
    while (token_buf[token_pos].type != C_BRACE) {
        bool ok = visit_statement();
        if (!ok) return ok;
    }
    token_pos++;
    return true;
}

// param = type IDENT
typedef struct {
    bool ok;
    Type type;
    Ident name;
} Param;
Param visit_param() {
    Param param = {0};
    param.type = visit_type();
    if (!param.type.ok) return param;

    param.name = visit_ident();
    if (!param.name.ok) return param;

    param.ok = true;
    return param;
}

// func_decl = type IDENT "(" param ("," param)* ")" (";" | block)
bool visit_func_decl() {
    Ident type_ret = visit_ident();
    Ident name = visit_ident();
    (void)type_ret;
    (void)name;

    token_pos++;
    while (token_buf[token_pos].type != C_PAREN) {
        Param param = visit_param();
        if (!param.ok) return param.ok;
        if (token_buf[token_pos].type == COMMA) token_pos++;
    }
    token_pos++;
    if (token_buf[token_pos].type == SEMI) {
        token_pos++;
    } else if (token_buf[token_pos].type == O_BRACE) {
        bool ok = visit_block();
        if (!ok) return ok;
    } else {
        fprintf(stderr, "unexpected token when parsing func_decl at %zu: %s\n",
                token_buf[token_pos].span.start, token_name[token_buf[token_pos].type]);
        return false;
    }
    return true;
}

// var_decl = type IDENT ";"
bool visit_var_decl() {
    fprintf(stderr, "TODO var_decl\n");
    return true;
}

// program = (var_decl | func_decl)+
bool visit_program() {
    while (token_pos < token_size) {
        // TODO: dry-parse type to figure out correct offset
        if (token_buf[token_pos + 2].type == O_PAREN || token_buf[token_pos + 3].type == O_PAREN) {
            bool ok = visit_func_decl();
            if (!ok) return ok;
        } else if (token_buf[token_pos + 2].type == SEMI) {
            bool ok = visit_var_decl();
            if (!ok) return ok;
        } else {
            fprintf(stderr, "unexpected token when parsing program at %zu: %s\n",
                    token_buf[token_pos].span.start, token_name[token_buf[token_pos].type]);
            return false;
        }
    }
    return true;
}

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
