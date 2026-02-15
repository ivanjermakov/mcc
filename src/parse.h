#pragma once
#include "core.h"
#include "emit.h"

Symbol* symbol_find(Span span) {
    if (stack_size == 0) return NULL;
    for (int64_t scope = stack_size - 1; scope >= 0; scope--) {
        size_t scope_size = stack_scope_size(scope);
        for (size_t i = 0; i < scope_size; i++) {
            Symbol* s = &symbols_buf[stack[scope].symbols_start + i];
            if (span_cmp(span, s->span) == 0) {
                return s;
            }
        }
    }
    fprintf(stderr, "symbol \"%.*s\" not found at %zu\n", (int)span.len, &input_buf[span.start],
            span.start);
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

size_t symbol_add_rodata(char* name, size_t name_size, size_t pos, size_t size) {
    memcpy(&symbol_names_buf[symbol_names_size], name, name_size);
    size_t idx = symbols_local_size;
    symbols_local_buf[symbols_local_size++] = (ElfSymbolEntry){
        .name = (uint32_t)symbol_names_size, .info = 1, .shndx = 2, .value = pos, .size = size};
    symbol_names_size += name_size + 1;
    return idx;
}

typedef struct {
    bool ok;
    Operand operand;
} Expr;

Expr visit_call();

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
    size_t symbol_name_size = sprintf(symbol_name_buf, ".str%zu", symbols_local_size);

    size_t symbol_idx = symbol_add_rodata(symbol_name_buf, symbol_name_size, rodata_size, str_len);
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
    token_pos++;
    return true;
}

// unary = call | IDENT | INT | string | "&" IDENT | "(" expr ")"
Expr visit_unary() {
    Expr expr = {};
    Token t = token_buf[token_pos];
    if (t.type == IDENT && token_buf[token_pos + 1].type == O_PAREN) {
        Expr call = visit_call();
        if (!call.ok) return expr;
        expr.operand = call.operand;
    } else if (t.type == IDENT) {
        Ident ident = visit_ident();
        if (!ident.ok) return expr;
        Symbol* symbol = symbol_find(ident.span);
        if (symbol == NULL) return expr;
        expr.operand = symbol->operand;
    } else if (t.type == INT) {
        Int i = visit_int();
        if (!i.ok) return expr;
        expr.operand = (Operand){.tag = IMMEDIATE, .o = {.immediate = {.value = i.value}}};
    } else if (t.type == DQUOTE) {
        String string = visit_string();
        if (!string.ok) return expr;
        expr.operand = string.operand;
    } else if (t.type == AMPERSAND) {
        fprintf(stderr, "TODO %s\n", token_name[t.type]);
        return expr;
    } else if (t.type == O_PAREN) {
        fprintf(stderr, "TODO %s\n", token_name[t.type]);
        return expr;
    } else {
        fprintf(stderr, "TODO %s\n", token_name[t.type]);
        return expr;
    }
    expr.ok = true;
    return expr;
}

// expr = unary (op unary)*
Expr visit_expr() {
    Expr expr = {};
    Expr unary = visit_unary();
    if (!unary.ok) return expr;
    while (token_buf[token_pos].type >= OP_PLUS && token_buf[token_pos].type <= OP_EQUAL) {
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

// define = type IDENT ("=" expr)? ";"
Expr visit_define() {
    Expr define = {};
    Type type = visit_type();
    if (!type.ok) return define;
    Ident name = visit_ident();
    if (!name.ok) return define;

    Scope* scope = &stack[stack_size];
    scope->bp_offset += 8;
    define.operand = (Operand){
        .tag = MEMORY,
        .o = {.memory = {.mode = MODE_RBP, .offset = scope->bp_offset}},
    };
    symbols_buf[symbols_size++] = (Symbol){
        .span = name.span,
        .operand = define.operand,
    };

    if (token_buf[token_pos].type == OP_EQUAL) {
        token_pos++;
        Expr expr = visit_expr();
        if (!expr.ok) return define;
        asm_mov(define.operand, expr.operand);
    }
    token_pos++;

    define.ok = true;
    return define;
}

// if = "if" "(" expr ")" block ";"
bool visit_if() {
    fprintf(stderr, "TODO if\n");
    return false;
}

// call = IDENT "(" expr ("," expr)* ")"
Expr visit_call() {
    Expr call = {};

    Ident name = visit_ident();
    if (!name.ok) return call;
    Symbol* symbol = symbol_find(name.span);
    if (symbol == NULL) {
        fprintf(stderr, "name not found\n");
        fprintf(stderr, "\n");
        return call;
    }

    token_pos++;
    size_t arg_idx = 0;
    while (token_pos < token_size) {
        Expr expr = visit_expr();
        if (!expr.ok) return call;
        if (token_buf[token_pos].type == SEMI) token_pos++;

        asm_lea(argument_registers[arg_idx], expr.operand);
        arg_idx++;
        if (token_buf[token_pos].type == COMMA) token_pos++;
        if (token_buf[token_pos].type == C_PAREN) break;
    }
    token_pos++;

    if (symbol->entry.name != 0) {
        asm_call_global(symbol);
    } else {
        fprintf(stderr, "TODO asm call\n");
        return call;
    }

    call.operand = RAX;
    call.ok = true;
    return call;
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

// statement = if | (expr ";") | return | define
bool visit_statement() {
    if (token_buf[token_pos].type == IF) {
        bool ok = visit_if();
        if (!ok) return ok;
    } else if (token_buf[token_pos].type == RETURN) {
        bool ok = visit_return();
        if (!ok) return ok;
    } else if (token_buf[token_pos + 2].type == SEMI || token_buf[token_pos + 3].type == SEMI ||
               token_buf[token_pos + 2].type == OP_EQUAL ||
               token_buf[token_pos + 3].type == OP_EQUAL) {
        Expr define = visit_define();
        if (!define.ok) return define.ok;
    } else {
        Expr expr = visit_expr();
        if (!expr.ok) return expr.ok;
        token_pos++;
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
    Operand operand;
} Param;
Param visit_param(size_t param_index) {
    Param param = {};
    param.type = visit_type();
    if (!param.type.ok) return param;
    param.name = visit_ident();
    if (!param.name.ok) return param;

    Scope* scope = &stack[stack_size];
    scope->bp_offset += 8;
    param.operand = (Operand){
        .tag = MEMORY,
        .o = {.memory = {.mode = MODE_RBP, .offset = scope->bp_offset}},
    };
    symbols_buf[symbols_size++] = (Symbol){
        .span = param.name.span,
        .operand = param.operand,
    };
    asm_mov(param.operand, argument_registers[param_index]);

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

    size_t param_token_pos = token_pos;
    while (token_pos < token_size &&
           !(token_buf[token_pos].type == SEMI || token_buf[token_pos].type == C_PAREN)) {
        token_pos++;
    }
    token_pos++;

    if (token_buf[token_pos].type == SEMI) {
        symbol_add_global(name.span, symbol_pos, false);
        token_pos++;
    } else if (token_buf[token_pos].type == O_BRACE) {
        symbol_add_global(name.span, symbol_pos, true);
        stack_push();

        asm_push(RBP);
        asm_mov(RBP, RSP);

        token_pos = param_token_pos;
        token_pos++;
        size_t param_index = 0;
        while (token_buf[token_pos].type != C_PAREN) {
            Param param = visit_param(param_index);
            if (!param.ok) return param.ok;
            if (token_buf[token_pos].type == COMMA) token_pos++;
            param_index++;
        }
        token_pos++;

        bool ok = visit_block();
        if (!ok) return ok;

        asm_pop(RBP);
        asm_ret();

        stack_pop();
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
    return false;
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
