#pragma once
#include "core.h"
#include "emit.h"

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
        add_symbol(*name.span, symbol_pos, false);
        token_pos++;
    } else if (token_buf[token_pos].type == O_BRACE) {
        add_symbol(*name.span, symbol_pos, true);
        asm_push(RBP);

        bool ok = visit_block();
        if (!ok) return ok;

        asm_pop(RBP);
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
