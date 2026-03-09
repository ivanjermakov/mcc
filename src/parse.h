#pragma once
#include "core.h"
#include "emit.h"
#include "expr.h"

Operand stack_alloc(size_t size) {
    Scope* scope = &ctx.stack[ctx.stack_len];
    Operand_ o = {
        .tag = MEMORY,
        .memory = {.mode = REL_RBP, .offset = scope->bp_offset},
    };
    Operand operand = {.rvalue = o, .lvalue = o};
    scope->bp_offset -= size;
    return operand;
}

Symbol* symbol_find(Span span) {
    if (ctx.stack_len == 0) return NULL;
    for (int64_t scope = ctx.stack_len - 1; scope >= 0; scope--) {
        size_t scope_size = stack_scope_size(scope);
        for (size_t i = 0; i < scope_size; i++) {
            Symbol* s = &ctx.symbols[ctx.stack[scope].symbols_start + i];
            if (span_cmp(span, s->span) == 0) {
                return s;
            }
        }
    }
    fprintf(stderr, "symbol \"%.*s\" not found at %zu\n", (int)span.len, &ctx.input[span.start],
            ctx.token_pos);
    return NULL;
}

Symbol* symbol_add_global(Span name, size_t pos, bool impl) {
    memcpy(&ctx.symbol_names[ctx.symbol_names_len], &ctx.input[name.start], name.len);
    ctx.symbols[ctx.symbols_len++] = (Symbol){
        .span = name,
        .entry =
            {
                .name = (uint32_t)ctx.symbol_names_len,
                .info = (uint8_t)((1 << 4) + (impl ? 2 : 0)),
                .shndx = (uint16_t)(impl ? 1 : 0),
                .value = pos,
            },
    };
    ctx.symbol_names_len += name.len + 1;
    return &ctx.symbols[ctx.symbols_len - 1];
}

Symbol* symbol_add_stack(Span name_span, size_t size, size_t count) {
    size_t s = size * MAX(count, 1);
    ctx.symbols[ctx.symbols_len++] = (Symbol){
        .span = name_span,
        .operand = stack_alloc(s),
        .size = s,
        .count = count,
        .item_size = size,
    };
    return &ctx.symbols[ctx.symbols_len - 1];
}

size_t symbol_add_rodata(char* name, size_t name_size, size_t pos, size_t size) {
    memcpy(&ctx.symbol_names[ctx.symbol_names_len], name, name_size);
    size_t idx = ctx.symbols_local_len;
    ctx.symbols_local[ctx.symbols_local_len++] = (ElfSymbolEntry){
        .name = (uint32_t)ctx.symbol_names_len, .info = 1, .shndx = 2, .value = pos, .size = size};
    ctx.symbol_names_len += name_size + 1;
    return idx;
}

Expr visit_call();
bool visit_block();
bool visit_var_def();
Expr visit_index();
bool visit_statement();

typedef struct {
    bool ok;
    Span* span;
    int64_t value;
} Int;
Int visit_int() {
    Span* span = &ctx.tokens[ctx.token_pos].span;
    int64_t value = strtol(&ctx.input[span->start], NULL, 10);
    Int i = {.ok = true, .span = span, .value = value};
    ctx.token_pos++;
    return i;
}

// string = "\"" (STRING_PART | ESCAPE_SEQ)+ "\""
Expr visit_string() {
    Expr string = {};

    ctx.token_pos++;
    uint8_t str_buf[1 << 10] = {0};
    size_t str_len = 0;
    while (true) {
        Span span = ctx.tokens[ctx.token_pos].span;
        if (ctx.tokens[ctx.token_pos].type == DQUOTE) {
            break;
        } else if (ctx.tokens[ctx.token_pos].type == STRING_PART) {
            memcpy(&str_buf[str_len], &ctx.input[span.start], span.len);
            str_len += span.len;
            ctx.token_pos++;
        } else if (ctx.tokens[ctx.token_pos].type == ESCAPE) {
            size_t start = ctx.tokens[ctx.token_pos].span.start;
            uint8_t c = ctx.input[start + 1];
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
            ctx.token_pos++;
        } else {
            fprintf(stderr, "unexpected token when parsing string at %zu: %s\n",
                    ctx.tokens[ctx.token_pos].span.start,
                    token_name[ctx.tokens[ctx.token_pos].type]);
            return string;
        }
    }
    ctx.token_pos++;

    char symbol_name_buf[16] = {0};
    size_t symbol_name_size = sprintf(symbol_name_buf, ".str%zu", ctx.symbols_local_len);

    size_t symbol_idx =
        symbol_add_rodata(symbol_name_buf, symbol_name_size, ctx.rodata_len, str_len);
    string.operand = (Operand){
        .rvalue =
            {
                .tag = MEMORY,
                .memory = {.mode = SYMBOL_LOCAL, .offset = symbol_idx},
            },
    };
    memcpy(&ctx.rodata[ctx.rodata_len], &str_buf, str_len);
    ctx.rodata_len += str_len + 1;

    string.ok = true;
    return string;
}

Expr visit_char() {
    Expr c = {};
    ctx.token_pos++;
    // TODO: escape
    c.operand.rvalue = immediate(ctx.input[ctx.tokens[ctx.token_pos].span.start]);
    ctx.token_pos++;
    ctx.token_pos++;
    c.ok = true;
    return c;
}

typedef struct {
    bool ok;
    Span span;
} Ident;
Ident visit_ident() {
    Ident ident = {.ok = true, .span = ctx.tokens[ctx.token_pos].span};
    ctx.token_pos++;
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

    if (ctx.tokens[ctx.token_pos].type == ASTERISK) {
        ctx.token_pos++;
        type.ptr = true;
    }
    type.ok = true;
    return type;
}

// op_infix = "=" | "+" | "-" | "%" | "&" | "|" | "^" | "<<" | ">>" | "==" | "!=" | "<" | ">" | "<="
// | ">="
Operator visit_op_infix() {
    Operator op = {.type = INFIX};
    switch (ctx.tokens[ctx.token_pos].type) {
        case EQUALS: {
            ctx.token_pos++;
            if (ctx.tokens[ctx.token_pos].type == EQUALS) {
                ctx.token_pos++;
                op.tag = OP_EQ;
                return op;
            }
            if (ctx.tokens[ctx.token_pos].type == EXCL) {
                ctx.token_pos++;
                op.tag = OP_NEQ;
                return op;
            }
            op.tag = OP_ASSIGN;
            return op;
        }
        case PLUS: {
            ctx.token_pos++;
            op.tag = OP_ADD;
            return op;
        }
        case PERCENT: {
            ctx.token_pos++;
            op.tag = OP_REMAINDER;
            return op;
        }
        case AMPERSAND: {
            ctx.token_pos++;
            if (ctx.tokens[ctx.token_pos].type == AMPERSAND) {
                ctx.token_pos++;
                op.tag = OP_AND;
            } else {
                op.tag = OP_ADDRESS_OF;
            }
            return op;
        }
        case O_ANGLE: {
            ctx.token_pos++;
            if (ctx.tokens[ctx.token_pos].type == EQUALS) {
                ctx.token_pos++;
                op.tag = OP_LE;
            } else {
                op.tag = OP_LT;
            }
            return op;
        }
        case C_ANGLE: {
            ctx.token_pos++;
            if (ctx.tokens[ctx.token_pos].type == EQUALS) {
                ctx.token_pos++;
                op.tag = OP_GE;
            } else {
                op.tag = OP_GT;
            }
            return op;
        }
        default:
            fprintf(stderr, "unexpected token when parsing op_infix at %zu: %s\n",
                    ctx.tokens[ctx.token_pos].span.start,
                    token_name[ctx.tokens[ctx.token_pos].type]);
            return op;
    }
}

// op_prefix = "++" | "--" | "+" | "-" | "&" | "*" | "!"
Operator visit_op_prefix() {
    Operator op = {.type = PREFIX};
    switch (ctx.tokens[ctx.token_pos].type) {
        case AMPERSAND: {
            ctx.token_pos++;
            op.tag = OP_ADDRESS_OF;
            break;
        }
        case ASTERISK: {
            ctx.token_pos++;
            op.tag = OP_DEREFERENCE;
            break;
        }
        case EXCL: {
            ctx.token_pos++;
            op.tag = OP_NOT;
            break;
        }
        default: {
        }
    }
    return op;
}

// op_postfix = "++" | "--" | index
Operator visit_op_postfix() {
    Operator op = {.type = POSTFIX};
    switch (ctx.tokens[ctx.token_pos].type) {
        case O_BRACKET: {
            Expr expr = visit_index();
            if (!expr.ok) break;
            op.tag = OP_INDEX;
            op.operand = expr.operand;
            break;
        }
        case PLUS: {
            if (ctx.tokens[ctx.token_pos + 1].type == PLUS) {
                ctx.token_pos += 2;
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
    Token t = ctx.tokens[ctx.token_pos];
    if (t.type == IDENT && ctx.tokens[ctx.token_pos + 1].type == O_PAREN) {
        Expr call = visit_call();
        if (!call.ok) return expr;
        expr.operand = call.operand;
    } else if (t.type == IDENT) {
        Ident ident = visit_ident();
        if (!ident.ok) return expr;
        Symbol* symbol = symbol_find(ident.span);
        if (symbol == NULL) return expr;
        if (symbol->count > 0) {
            Operand tmp = {.rvalue = expr_registers[ctx.expr_registers_busy++],
                           .lvalue = symbol->operand.lvalue};
            asm_lea(tmp.rvalue, symbol->operand.rvalue);
            expr.operand = tmp;
        } else {
            expr.operand = symbol->operand;
        }
    } else if (t.type == INT) {
        Int i = visit_int();
        if (!i.ok) return expr;
        expr.operand.rvalue = immediate(i.value);
    } else if (t.type == DQUOTE) {
        Expr string = visit_string();
        if (!string.ok) return expr;
        expr.operand = string.operand;
    } else if (t.type == QUOTE) {
        Expr c = visit_char();
        if (!c.ok) return expr;
        expr.operand = c.operand;
    } else if (t.type == O_PAREN) {
        ctx.token_pos++;
        Expr sub_expr = visit_expr();
        if (!sub_expr.ok) return expr;
        ctx.token_pos++;
        expr.operand = sub_expr.operand;
    } else {
        fprintf(stderr, "TODO %s\n", token_name[t.type]);
        return expr;
    }
    expr.ok = true;
    return expr;
}

// if = "if" "(" expr ")" (block | statement) ("else" (block | statement))?
bool visit_if() {
    bool ok;
    ctx.token_pos += 2;

    Expr expr = visit_expr();
    if (!expr.ok) return false;
    ctx.token_pos++;

    asm_cmp(expr.operand.rvalue, immediate(0));
    size_t else_jmp_pos = ctx.text_len;
    asm_canary(6);

    if (ctx.tokens[ctx.token_pos].type == O_BRACE) {
        ok = visit_block();
    } else {
        ok = visit_statement();
    }
    if (!ok) return false;

    size_t end_jmp_pos;
    if (ctx.tokens[ctx.token_pos].type == ELSE) {
        end_jmp_pos = ctx.text_len;
        asm_canary(6);
    }

    size_t text_size_bak = ctx.text_len;
    ctx.text_len = else_jmp_pos;
    asm_je(text_size_bak - else_jmp_pos - 6);
    ctx.text_len = text_size_bak;

    if (ctx.tokens[ctx.token_pos].type == ELSE) {
        ctx.token_pos++;
        if (ctx.tokens[ctx.token_pos].type == O_BRACE) {
            ok = visit_block();
        } else {
            ok = visit_statement();
        }

        size_t text_size_bak = ctx.text_len;
        ctx.text_len = end_jmp_pos;
        asm_jmp(text_size_bak - end_jmp_pos - 5);
        ctx.text_len = text_size_bak;
    }
    if (!ok) return false;

    return true;
}

// while = "while" "(" expr ")" (block | statement)
bool visit_while() {
    int32_t loop_pos = ctx.text_len;

    ctx.token_pos += 2;
    Expr expr = visit_expr();
    if (!expr.ok) return false;
    ctx.token_pos++;

    asm_cmp(expr.operand.rvalue, immediate(0));
    size_t je_pos = ctx.text_len;
    asm_canary(6);

    if (ctx.tokens[ctx.token_pos].type == O_BRACE) {
        bool ok = visit_block();
        if (!ok) return false;
    } else {
        bool ok = visit_statement();
        if (!ok) return false;
    }

    asm_jmp(loop_pos - ctx.text_len - 5);

    size_t text_size_bak = ctx.text_len;
    ctx.text_len = je_pos;
    asm_je(text_size_bak - je_pos - 6);
    ctx.text_len = text_size_bak;

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

    ctx.token_pos++;
    int32_t arg_count = 0;
    while (ctx.token_pos < ctx.tokens_len && ctx.tokens[ctx.token_pos].type != C_PAREN) {
        Expr expr = visit_expr();
        if (!expr.ok) return call;
        if (ctx.tokens[ctx.token_pos].type == SEMI) ctx.token_pos++;

        asm_mov(argument_registers[arg_count], expr.operand.rvalue);
        arg_count++;
        if (ctx.tokens[ctx.token_pos].type == COMMA) ctx.token_pos++;
    }
    ctx.token_pos++;

    // TODO: backup RAX
    // TODO: only for variadic function calls
    // TODO: properly count vararg count by checking a matched fn def
    if (arg_count > 1) {
        asm_mov(RAX, immediate(arg_count - 1));
    }

    size_t misalign = ctx.stack_offset % 16;
    if (misalign != 0) asm_sub(RSP, immediate(misalign));

    if (symbol->entry.name != 0) {
        asm_call_global(symbol);
    } else {
        fprintf(stderr, "TODO asm call\n");
        return call;
    }

    if (misalign > 0) asm_add(RSP, immediate(misalign));

    call.operand.rvalue = RAX;
    call.ok = true;
    return call;
}

// return = "return" expr ";"
bool visit_return() {
    ctx.token_pos++;
    Expr expr = visit_expr();
    if (!expr.ok) return expr.ok;
    asm_mov(RAX, expr.operand.rvalue);
    ctx.token_pos++;
    return true;
}

// statement = if | while | (expr ";") | return | var_def
bool visit_statement() {
    assert(ctx.expr_registers_busy < 14);
    ctx.expr_registers_busy = 0;

    if (ctx.tokens[ctx.token_pos].type == IF) {
        bool ok = visit_if();
        if (!ok) return false;
    } else if (ctx.tokens[ctx.token_pos].type == WHILE) {
        bool ok = visit_while();
        if (!ok) return false;
    } else if (ctx.tokens[ctx.token_pos].type == RETURN) {
        bool ok = visit_return();
        if (!ok) return false;
    } else {
        Context ctx_old = ctx;
        if (visit_type().ok) {
            if (visit_ident().ok) {
                if (ctx.tokens[ctx.token_pos].type == SEMI ||
                    ctx.tokens[ctx.token_pos].type == EQUALS ||
                    ctx.tokens[ctx.token_pos].type == O_BRACKET) {
                    ctx = ctx_old;
                    bool ok = visit_var_def();
                    if (!ok) return false;
                    return true;
                }
            }
        }
        ctx = ctx_old;

        Expr expr = visit_expr();
        if (!expr.ok) return expr.ok;
        ctx.token_pos++;
    }
    return true;
}

// block = "{" statement* "}"
bool visit_block() {
    ctx.token_pos++;
    while (ctx.tokens[ctx.token_pos].type != C_BRACE) {
        bool ok = visit_statement();
        if (!ok) return ok;
    }
    ctx.token_pos++;
    return true;
}

// param = (type IDENT) | "..."
typedef struct {
    bool ok;
    bool spread;
    Type type;
    Ident name;
    Operand operand;
} Param;
Param visit_param(size_t param_index) {
    if (ctx.tokens[ctx.token_pos].type == PERIOD) {
        ctx.token_pos += 3;
        return (Param){
            .ok = true,
            .spread = true,
        };
    }

    Param param = {};
    param.type = visit_type();
    if (!param.type.ok) return param;
    param.name = visit_ident();
    if (!param.name.ok) return param;

    param.operand = stack_alloc(8);
    ctx.symbols[ctx.symbols_len++] = (Symbol){
        .span = param.name.span,
        .operand = param.operand,
    };
    asm_mov(param.operand.rvalue, argument_registers[param_index]);

    param.ok = true;
    return param;
}

// func_def = type IDENT "(" param ("," param)* ")" (";" | block)
bool visit_func_def() {
    size_t symbol_pos = ctx.text_len;
    Ident type_ret = visit_ident();
    if (!type_ret.ok) return type_ret.ok;
    Ident name = visit_ident();
    if (!name.ok) return name.ok;

    // look ahead to decide whether this is function declaration or impl
    size_t param_token_pos = ctx.token_pos;
    while (ctx.token_pos < ctx.tokens_len &&
           !(ctx.tokens[ctx.token_pos].type == SEMI || ctx.tokens[ctx.token_pos].type == C_PAREN)) {
        ctx.token_pos++;
    }
    ctx.token_pos++;

    // for push/pop
    size_t stack_prealloc = 1024;

    if (ctx.tokens[ctx.token_pos].type == SEMI) {
        symbol_add_global(name.span, symbol_pos, false);
        ctx.token_pos++;
    } else if (ctx.tokens[ctx.token_pos].type == O_BRACE) {
        symbol_add_global(name.span, symbol_pos, true);
        stack_push();

        ctx.stack_offset = -stack_prealloc;

        // TODO: only preserve used registers
        for (size_t i = 0; i < non_volatile_registers_size; i++) {
            asm_push(non_volatile_registers[i]);
        }

        asm_mov(RBP, RSP);
        asm_sub(RSP, immediate(stack_prealloc));

        ctx.token_pos = param_token_pos;
        ctx.token_pos++;
        size_t param_index = 0;
        while (ctx.tokens[ctx.token_pos].type != C_PAREN) {
            Param param = visit_param(param_index);
            if (!param.ok) return param.ok;
            if (ctx.tokens[ctx.token_pos].type == COMMA) ctx.token_pos++;
            param_index++;
        }
        ctx.token_pos++;

        bool ok = visit_block();
        if (!ok) return ok;

        asm_add(RSP, immediate(stack_prealloc));

        // TODO: only preserve used registers
        for (size_t i = 1; i <= non_volatile_registers_size; i++) {
            asm_pop(non_volatile_registers[non_volatile_registers_size - i]);
        }

        asm_ret();

        stack_pop();
    } else {
        fprintf(stderr, "unexpected token when parsing func_def at %zu: %s\n",
                ctx.tokens[ctx.token_pos].span.start, token_name[ctx.tokens[ctx.token_pos].type]);
        return false;
    }

    return true;
}

// index = "[" expr "]"
Expr visit_index() {
    ctx.token_pos++;
    Expr i = visit_expr();
    if (!i.ok) return (Expr){};
    ctx.token_pos++;
    return i;
}

// array_size = "[" INT "]"
Int visit_array_size() {
    ctx.token_pos++;
    Int i = visit_int();
    if (!i.ok) return (Int){};
    ctx.token_pos++;
    return i;
}

// var_def = type IDENT array_size? ("=" expr)? ";"
bool visit_var_def() {
    Type type = visit_type();
    if (!type.ok) return false;
    Ident name = visit_ident();
    if (!name.ok) return false;

    int64_t array_size = 0;
    if (ctx.tokens[ctx.token_pos].type == O_BRACKET) {
        Int size = visit_array_size();
        if (!size.ok) return false;
        array_size = size.value;
    }

    Expr expr = {};
    if (ctx.tokens[ctx.token_pos].type == EQUALS) {
        ctx.token_pos++;
        expr = visit_expr();
        if (!expr.ok) return false;
    }

    if (ctx.stack_len == 1) {
        symbol_add_global(name.span, ctx.text_len, true);
        if (expr.ok) {
            fprintf(stderr, "TODO global assign\n");
        }
    } else {
        // TODO: proper array item size
        Symbol* symbol = symbol_add_stack(name.span, array_size > 0 ? 1 : 8, array_size);
        if (expr.ok) {
            asm_mov(symbol->operand.rvalue, expr.operand.rvalue);
        }
    }
    ctx.token_pos++;

    return true;
}

// program = (var_def | func_def)+
bool visit_program() {
    bool ok;
    stack_push();
    while (ctx.token_pos < ctx.tokens_len) {
        Context ctx_old = ctx;
        visit_type();
        visit_ident();
        bool is_func_def = ctx.tokens[ctx.token_pos].type == O_PAREN;
        if (ctx.tokens[ctx.token_pos].type == O_BRACKET) visit_array_size();
        bool is_var_def =
            ctx.tokens[ctx.token_pos].type == EQUALS || ctx.tokens[ctx.token_pos].type == SEMI;
        ctx = ctx_old;

        if (is_func_def) {
            ok = visit_func_def();
            if (!ok) return false;
        } else if (is_var_def) {
            ok = visit_var_def();
            if (!ok) return false;
        } else {
            fprintf(stderr, "unexpected token when parsing program at %zu: %s\n",
                    ctx.tokens[ctx.token_pos].span.start,
                    token_name[ctx.tokens[ctx.token_pos].type]);
            return false;
        }
    }
    return true;
}
