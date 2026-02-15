#include "core.h"
#include "emit.h"

Expr visit_unary();
Operator visit_op();

typedef struct {
} ExprTokenOp;

typedef struct {
    bool is_operator;
    union {
        Operator op;
        Operand operand;
    } e;
} ExprToken;

/**
 * @see https://en.wikipedia.org/wiki/Shunting_yard_algorithm
 */
bool shunting_yard(ExprToken expr_stack[], size_t* expr_stack_size) {
    Operator op_stack[1 << 10];
    size_t op_stack_size = 0;

    while (token_buf[token_pos].type != SEMI && token_buf[token_pos].type != COMMA &&
           token_buf[token_pos].type != C_PAREN) {
        ExprToken expr_token = {};
        expr_token.is_operator =
            token_buf[token_pos].type >= OP_PLUS && token_buf[token_pos].type <= OP_EQUAL;
        if (expr_token.is_operator) {
            Operator op = visit_op();
            if (op == 0) return false;
            expr_token.e.op = op;
            while (
                op_stack_size > 0 &&
                (operator_precedence[op_stack[op_stack_size - 1]] > operator_precedence[op] ||
                 (operator_associativity[op] == ASSOC_LEFT &&
                  operator_precedence[op_stack[op_stack_size - 1]] == operator_precedence[op]))) {
                expr_stack[(*expr_stack_size)++] = (ExprToken){
                    .is_operator = true,
                    .e = {.op = op_stack[--(op_stack_size)]},
                };
            }
            op_stack[(op_stack_size)++] = op;
        } else {
            Expr unary = visit_unary();
            if (!unary.ok) return false;
            expr_token.e.operand = unary.operand;
            expr_stack[(*expr_stack_size)++] = expr_token;
        }
    }
    while (op_stack_size > 0) {
        expr_stack[(*expr_stack_size)++] = (ExprToken){
            .is_operator = true,
            .e = {.op = op_stack[--op_stack_size]},
        };
    }
    return true;
}

// expr = unary (op unary)*
Expr visit_expr() {
    ExprToken expr_stack[1 << 10];
    size_t expr_stack_size = 0;
    size_t expr_registers_busy_pre = expr_registers_busy;

    bool ok = shunting_yard(expr_stack, &expr_stack_size);
    if (!ok) return (Expr){};

    assert(expr_stack_size > 0);
    size_t expr_pos = 0;
    Operand eval_stack[1 << 10] = {};
    size_t eval_stack_size = 0;
    while (expr_pos < expr_stack_size) {
        if (expr_stack[expr_pos].is_operator) {
            Operator op = expr_stack[expr_pos++].e.op;
            assert(eval_stack_size >= 2);
            Operand o1 = eval_stack[eval_stack_size--];
            Operand o2 = eval_stack[eval_stack_size];
            switch (op) {
                case OP_ADD: asm_add(o1, o2); break;
                default: {
                    fprintf(stderr, "TODO binary op\n");
                    return (Expr){};
                }
            }
        } else {
            Operand o = expr_stack[expr_pos++].e.operand;
            if (eval_stack_size == 0) {
                asm_mov(expr_registers[expr_registers_busy++], o);
                eval_stack[eval_stack_size++] = expr_registers[expr_registers_busy];
            } else {
                eval_stack[eval_stack_size++] = o;
            }
        }
    }

    expr_registers_busy--;
    assert(expr_registers_busy_pre == expr_registers_busy);
    return (Expr){.ok = true, .operand = expr_registers[expr_registers_busy]};
}
