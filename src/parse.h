#pragma once
#include "core.h"
#include "emit.h"

Symbol* symbol_find(Span span) {
    if (stack_size == 0) return NULL;
    for (size_t scope = stack_size - 1; scope >= 0; scope--) {
        size_t scope_size = stack_scope_size(scope);
        for (size_t i = 0; i < scope_size; i++) {
            Symbol* s = &symbols_buf[stack[scope] + i];
            if (span_cmp(span, s->span) == 0) {
                return s;
            }
        }
    }
    return NULL;
}

Symbol* symbol_add_global(Span name, size_t pos, bool impl) {
    memcpy(&symbol_names_buf[symbol_names_size], &input_buf[name.start], name.len);
    symbols_buf[symbols_size++] = (Symbol){
        .span = name,
        .entry =
            {
                .name = (uint32_t)symbol_names_size,
                .info = (uint8_t)((1 << 4) + (impl ? 2 : 0)),
                .shndx = (uint16_t)(impl ? 1 : 0),
                .value = pos,
            },
    };
    symbol_names_size += name.len + 1;
    return &symbols_buf[symbols_size];
}

size_t symbol_add_local(char* name, size_t pos, size_t size) {
    memcpy(&symbol_names_buf[symbol_names_size], name, strlen(name));
    size_t idx = symbols_local_size;
    symbols_local_buf[symbols_local_size++] = (ElfSymbolEntry){
        .name = (uint32_t)symbol_names_size, .info = 1, .shndx = 2, .value = pos, .size = size};
    symbol_names_size += strlen(name) + 1;
    return idx;
}

typedef struct {
    bool ok;
    Operand operand;
} Expr;

typedef struct {
    bool ok;
    Span* span;
    int64_t value;
} Int;
Int visit_int() {
    Span* span = &token_buf[token_pos].span;
    int64_t value = strtol(&input_buf[span->start], 0, 10);
    Int i = {.ok = true, .span = span, .value = value};
    token_pos++;
    return i;
}

// string = "\"" (STRING_PART | ESCAPE_SEQ)+ "\""
typedef struct {
    bool ok;
    Operand operand;
} String;
String visit_string() {
    String string = {};

    token_pos++;
    uint8_t str_buf[1 << 10] = {0};
    size_t str_len = 0;
    while (true) {
        Span span = token_buf[token_pos].span;
        if (token_buf[token_pos].type == DQUOTE) {
            break;
        } else if (token_buf[token_pos].type == STRING_PART) {
            memcpy(&str_buf[str_len], &input_buf[span.start], span.len);
            str_len += span.len;
            token_pos++;
        } else if (token_buf[token_pos].type == ESCAPE) {
            size_t start = token_buf[token_pos].span.start;
            uint8_t c = input_buf[start + 1];
            uint8_t b;
            switch (c) {
                case 'a': b = 0x07; break;
                case 'b': b = 0x08; break;
                case 'e': b = 0x1B; break;
                case 'f': b = 0x0C; break;
                case 'n': b = 0x0A; break;
                case 'r': b = 0x0D; break;
                case 't': b = 0x09; break;
                case 'v': b = 0x0B; break;
                case '\\': b = 0x5C; break;
                case '\'': b = 0x27; break;
                case '"': b = 0x22; break;
                default: {
                    fprintf(stderr, "TODO byte escape sequence\n");
                    return string;
                }
            }
            memcpy(&str_buf[str_len], &b, 1);
            str_len += 1;
            token_pos++;
        } else {
            fprintf(stderr, "unexpected token when parsing string at %zu: %s\n",
                    token_buf[token_pos].span.start, token_name[token_buf[token_pos].type]);
            return string;
        }
    }
    token_pos++;

    char symbol_name_buf[16] = {0};
    sprintf(symbol_name_buf, ".str%zu", symbols_local_size);

    size_t symbol_idx = symbol_add_local(symbol_name_buf, rodata_size, str_len);
    string.operand = (Operand){
        .tag = MEMORY,
        .o = {.memory = {.mode = SYMBOL_LOCAL, .offset = symbol_idx}},
    };
    memcpy(&rodata_buf[rodata_size], &str_buf, str_len);
    rodata_size += str_len + 1;

    string.ok = true;
    return string;
}

typedef struct {
    bool ok;
    Span span;
} Ident;
Ident visit_ident() {
    Ident ident = {.ok = true, .span = token_buf[token_pos].span};
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
    Type type = {};
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

// unary = IDENT | INT | string | "&" IDENT | "(" expr ")"
Expr visit_unary() {
    Expr expr = {};
    if (token_buf[token_pos].type == IDENT) {
        fprintf(stderr, "TODO\n");
        return expr;
    } else if (token_buf[token_pos].type == INT) {
        Int i = visit_int();
        if (!i.ok) return expr;
        expr.operand = (Operand){.tag = IMMEDIATE, .o = {.immediate = {.value = i.value}}};
    } else if (token_buf[token_pos].type == DQUOTE) {
        String string = visit_string();
        if (!string.ok) return expr;
        expr.operand = string.operand;
    } else if (token_buf[token_pos].type == AMPERSAND) {
        fprintf(stderr, "TODO\n");
        return expr;
    } else if (token_buf[token_pos].type == O_PAREN) {
        fprintf(stderr, "TODO\n");
        return expr;
    } else {
        fprintf(stderr, "TODO\n");
        return expr;
    }
    expr.ok = true;
    return expr;
}

// expr = unary (op unary)?
Expr visit_expr() {
    Expr expr = {};
    Expr unary = visit_unary();
    if (!unary.ok) return expr;
    while (token_buf[token_pos].type >= OP_PLUS && token_buf[token_pos].type <= OP_PLUS) {
        bool ok = visit_op();
        if (!ok) return expr;
        unary = visit_unary();
        if (!unary.ok) return expr;
    }
    expr.ok = true;
    // TODO: precedence parsing
    expr.operand = unary.operand;
    return expr;
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
    size_t arg_idx = 0;
    while (token_buf[token_pos].type != C_PAREN) {
        Expr expr = visit_expr();
        if (!expr.ok) return expr.ok;
        if (token_buf[token_pos].type == SEMI) token_pos++;

        asm_lea(argument_registers[arg_idx], expr.operand);
        arg_idx++;
    }
    token_pos++;
    token_pos++;

    Symbol* symbol = symbol_find(name.span);
    if (symbol == NULL) {
        fprintf(stderr, "name not found\n");
        fprintf(stderr, "\n");
        return false;
    }
    if (symbol->entry.name != 0) {
        asm_call_global(symbol);
    } else {
        fprintf(stderr, "TODO asm call\n");
        return false;
    }

    return true;
}

// return = "return" expr ";"
bool visit_return() {
    token_pos++;
    Expr expr = visit_expr();
    if (!expr.ok) return expr.ok;
    asm_mov(RAX, expr.operand);
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
    Param param = {};
    param.type = visit_type();
    if (!param.type.ok) return param;

    param.name = visit_ident();
    if (!param.name.ok) return param;

    param.ok = true;
    return param;
}

// func_decl = type IDENT "(" param ("," param)* ")" (";" | block)
bool visit_func_decl() {
    size_t symbol_pos = text_size;
    Ident type_ret = visit_ident();
    if (!type_ret.ok) return type_ret.ok;
    Ident name = visit_ident();
    if (!name.ok) return name.ok;

    token_pos++;
    while (token_buf[token_pos].type != C_PAREN) {
        Param param = visit_param();
        if (!param.ok) return param.ok;
        if (token_buf[token_pos].type == COMMA) token_pos++;
    }
    token_pos++;
    if (token_buf[token_pos].type == SEMI) {
        symbol_add_global(name.span, symbol_pos, false);

        token_pos++;
    } else if (token_buf[token_pos].type == O_BRACE) {
        symbol_add_global(name.span, symbol_pos, true);

        asm_push(RBP);
        asm_mov(RBP, RSP);

        bool ok = visit_block();
        if (!ok) return ok;

        asm_pop(RBP);
        asm_ret();
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
    stack_push();
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
