#pragma once
#include "core.h"
#include "emit.h"
#include "expr.h"

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
    return &symbols_buf[symbols_size - 1];
}

Symbol* symbol_add_stack(Span name_span, size_t size, size_t count) {
    Scope* scope = &stack[stack_size];
    scope->bp_offset -= size;
    Operand operand = {
        .tag = MEMORY,
        .memory = {.mode = REL_RBP, .offset = scope->bp_offset},
    };
    symbols_buf[symbols_size++] = (Symbol){
        .span = name_span,
        .operand = operand,
        .size = size * count,
        .item_size = size,
    };
    return &symbols_buf[symbols_size - 1];
}

size_t symbol_add_rodata(char* name, size_t name_size, size_t pos, size_t size) {
    memcpy(&symbol_names_buf[symbol_names_size], name, name_size);
    size_t idx = symbols_local_size;
    symbols_local_buf[symbols_local_size++] = (ElfSymbolEntry){
        .name = (uint32_t)symbol_names_size, .info = 1, .shndx = 2, .value = pos, .size = size};
    symbol_names_size += name_size + 1;
    return idx;
}

Expr visit_call();
bool visit_block();
bool visit_var_def();
Expr visit_index();

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
Expr visit_string() {
    Expr string = {};

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
        .memory = {.mode = SYMBOL_LOCAL, .offset = symbol_idx},
    };
    memcpy(&rodata_buf[rodata_size], &str_buf, str_len);
    rodata_size += str_len + 1;

    string.ok = true;
    return string;
}

Expr visit_char() {
    Expr c = {};
    token_pos++;
    // TODO: escape
    c.operand = immediate(input_buf[token_buf[token_pos].span.start]);
    token_pos++;
    token_pos++;
    c.ok = true;
    return c;
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

// op = "=" | "+" | "-" | "&" | "|" | "^" | "<<" | ">>" | "==" | "!=" | "<" | ">" | "<=" | ">="
Operator visit_op_infix() {
    Operator op = {.type = INFIX};
    switch (token_buf[token_pos].type) {
        case EQUALS: {
            token_pos++;
            if (token_buf[token_pos].type == EQUALS) {
                op.tag = OP_EQ;
                return op;
            }
            if (token_buf[token_pos].type == EXCL) {
                op.tag = OP_NEQ;
                return op;
            }
            op.tag = OP_ASSIGN;
            return op;
        }
        case PLUS: {
            token_pos++;
            op.tag = OP_ADD;
            return op;
        }
        case O_ANGLE: {
            token_pos++;
            if (token_buf[token_pos].type == EQUALS) {
                token_pos++;
                op.tag = OP_LE;
            } else {
                op.tag = OP_LT;
            }
            return op;
        }
        case C_ANGLE: {
            token_pos++;
            if (token_buf[token_pos].type == EQUALS) {
                token_pos++;
                op.tag = OP_GE;
            } else {
                op.tag = OP_GT;
            }
            return op;
        }
        default:
            fprintf(stderr, "unexpected token when parsing op_infix at %zu: %s\n",
                    token_buf[token_pos].span.start, token_name[token_buf[token_pos].type]);
            return op;
    }
}

// op_prefix = "++" | "--" | "+" | "-" | "&" | "*"
Operator visit_op_prefix() {
    Operator op = {.type = PREFIX};
    switch (token_buf[token_pos].type) {
        default: {
        }
    }
    return op;
}

// op_postfix = "++" | "--" | index
Operator visit_op_postfix() {
    Operator op = {.type = POSTFIX};
    switch (token_buf[token_pos].type) {
        case O_BRACKET: {
            Expr expr = visit_index();
            if (!expr.ok) break;
            op.tag = OP_INDEX;
            op.operand = expr.operand;
            break;
        }
        case PLUS: {
            if (token_buf[token_pos + 1].type == PLUS) {
                token_pos += 2;
                op.tag = OP_INCREMENT;
                break;
            }
        }
        default: {
        }
    }
    return op;
}

// operand = IDENT | call | INT | string | char | "(" expr ")"
Expr visit_operand() {
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
        if (symbol->size > 0) {
            Operand tmp = expr_registers[expr_registers_busy++];
            asm_lea(tmp, symbol->operand);
            expr.operand = tmp;
        } else {
            expr.operand = symbol->operand;
        }
    } else if (t.type == INT) {
        Int i = visit_int();
        if (!i.ok) return expr;
        expr.operand = immediate(i.value);
    } else if (t.type == DQUOTE) {
        Expr string = visit_string();
        if (!string.ok) return expr;
        expr.operand = string.operand;
    } else if (t.type == QUOTE) {
        Expr c = visit_char();
        if (!c.ok) return expr;
        expr.operand = c.operand;
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

// if = "if" "(" expr ")" block
bool visit_if() {
    fprintf(stderr, "TODO if\n");
    return false;
}

// while = "while" "(" expr ")" block
bool visit_while() {
    token_pos += 2;
    Expr expr = visit_expr();
    if (!expr.ok) return false;
    token_pos++;

    size_t loop_pos = text_size;
    asm_cmp(expr.operand, immediate(0));
    size_t je_pos = text_size;
    asm_canary(3);

    bool ok = visit_block();
    if (!ok) return false;

    size_t text_size_bak = text_size;
    asm_je(text_size - je_pos);
    text_size = text_size_bak;

    asm_jmp(text_size - loop_pos);

    return true;
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

        asm_mov(argument_registers[arg_idx], expr.operand);
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

// statement = if | while | (expr ";") | return | var_def
bool visit_statement() {
    if (token_buf[token_pos].type == IF) {
        bool ok = visit_if();
        if (!ok) return false;
    } else if (token_buf[token_pos].type == WHILE) {
        bool ok = visit_while();
        if (!ok) return false;
    } else if (token_buf[token_pos].type == RETURN) {
        bool ok = visit_return();
        if (!ok) return false;
    } else {
        size_t token_pos_old = token_pos;
        if (visit_type().ok) {
            if (visit_ident().ok) {
                if (token_buf[token_pos].type == SEMI || token_buf[token_pos].type == EQUALS ||
                    token_buf[token_pos].type == O_BRACKET) {
                    token_pos = token_pos_old;
                    bool ok = visit_var_def();
                    if (!ok) return false;
                    return true;
                }
            }
        }
        token_pos = token_pos_old;

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
    scope->bp_offset -= 8;
    param.operand = (Operand){
        .tag = MEMORY,
        .memory = {.mode = REL_RBP, .offset = scope->bp_offset},
    };
    symbols_buf[symbols_size++] = (Symbol){
        .span = param.name.span,
        .operand = param.operand,
    };
    asm_mov(param.operand, argument_registers[param_index]);

    param.ok = true;
    return param;
}

// func_def = type IDENT "(" param ("," param)* ")" (";" | block)
bool visit_func_def() {
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
        fprintf(stderr, "unexpected token when parsing func_def at %zu: %s\n",
                token_buf[token_pos].span.start, token_name[token_buf[token_pos].type]);
        return false;
    }

    return true;
}

// index = "[" expr "]"
Expr visit_index() {
    token_pos++;
    Expr i = visit_expr();
    if (!i.ok) return (Expr){};
    token_pos++;
    return i;
}

// array_size = "[" INT "]"
Int visit_array_size() {
    token_pos++;
    Int i = visit_int();
    if (!i.ok) return (Int){};
    token_pos++;
    return i;
}

// var_def = type IDENT array_size? ("=" expr)? ";"
bool visit_var_def() {
    Type type = visit_type();
    if (!type.ok) return false;
    Ident name = visit_ident();
    if (!name.ok) return false;

    int64_t array_size = 0;
    if (token_buf[token_pos].type == O_BRACKET) {
        Int size = visit_array_size();
        if (!size.ok) return false;
        array_size = size.value;
    }

    Expr expr = {};
    if (token_buf[token_pos].type == EQUALS) {
        token_pos++;
        expr = visit_expr();
        if (!expr.ok) return false;
    }

    if (stack_size == 1) {
        symbol_add_global(name.span, text_size, true);
        if (expr.ok) {
            fprintf(stderr, "TODO global assign\n");
        }
    } else {
        Symbol* symbol = symbol_add_stack(name.span, 8, array_size);
        if (expr.ok) {
            asm_mov(symbol->operand, expr.operand);
        }
    }
    token_pos++;

    return true;
}

// program = (var_def | func_def)+
bool visit_program() {
    stack_push();
    while (token_pos < token_size) {
        // TODO: dry-parse type to figure out correct offset
        if (token_buf[token_pos + 2].type == O_PAREN || token_buf[token_pos + 3].type == O_PAREN) {
            bool ok = visit_func_def();
            if (!ok) return ok;
        } else if (token_buf[token_pos + 2].type == SEMI) {
            bool ok = visit_var_def();
            if (!ok) return ok;
        } else {
            fprintf(stderr, "unexpected token when parsing program at %zu: %s\n",
                    token_buf[token_pos].span.start, token_name[token_buf[token_pos].type]);
            return false;
        }
    }
    return true;
}
