#include "core.h"
#include "emit.h"

Expr visit_operand();
Operator visit_op_infix();
Operator visit_op_prefix();
Operator visit_op_postfix();

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
 * expr  = unary (op_infix unary)*
 * unary = op_prefix* operand op_postfix*
 *
 * @see https://en.wikipedia.org/wiki/Shunting_yard_algorithm
 */
bool shunting_yard(ExprToken expr_stack[], size_t* expr_stack_size) {
    Operator op_stack[1 << 10];
    size_t op_stack_size = 0;

    bool infix_postfix_time = false;
    while (token_buf[token_pos].type != SEMI && token_buf[token_pos].type != COMMA &&
           token_buf[token_pos].type != C_PAREN && token_buf[token_pos].type != C_BRACKET) {
        ExprToken expr_token = {};
        if (infix_postfix_time) {
            Operator op = visit_op_postfix();
            if (op.tag == 0) {
                op = visit_op_infix();
                if (op.tag == 0) return false;
                infix_postfix_time = false;
            }
            expr_token.is_operator = true;
            expr_token.e.op = op;
        } else {
            Operator op = visit_op_prefix();
            if (op.tag != 0) {
                expr_token.is_operator = true;
                expr_token.e.op = op;
            } else {
                Expr operand = visit_operand();
                if (!operand.ok) return false;
                expr_token.e.operand = operand.operand;
                expr_stack[(*expr_stack_size)++] = expr_token;
                infix_postfix_time = true;
            }
        }

        if (expr_token.is_operator) {
            Operator op = expr_token.e.op;
            while (op_stack_size > 0 && (operator_precedence[op_stack[op_stack_size - 1].tag] >
                                             operator_precedence[op.tag] ||
                                         (operator_associativity[op.tag] == ASSOC_LEFT &&
                                          operator_precedence[op_stack[op_stack_size - 1].tag] ==
                                              operator_precedence[op.tag]))) {
                expr_stack[(*expr_stack_size)++] = (ExprToken){
                    .is_operator = true,
                    .e = {.op = op_stack[--op_stack_size]},
                };
            }
            op_stack[op_stack_size++] = op;
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

// expr = unary (op_infix unary)*
Expr visit_expr() {
    ExprToken expr_stack[1 << 10];
    size_t expr_stack_size = 0;
    size_t expr_registers_busy_pre = expr_registers_busy;

    bool ok = shunting_yard(expr_stack, &expr_stack_size);
    if (!ok) return (Expr){};
    bool need_tmp_register = expr_stack_size > 1;

    assert(expr_stack_size > 0);
    size_t expr_pos = 0;
    Operand eval_stack[1 << 10] = {};
    size_t eval_stack_size = 0;
    while (expr_pos < expr_stack_size) {
        if (expr_stack[expr_pos].is_operator) {
            Operator op = expr_stack[expr_pos++].e.op;
            if (op.type == INFIX) {
                assert(eval_stack_size >= 2);
                Operand o1 = eval_stack[eval_stack_size--];
                Operand o2 = eval_stack[eval_stack_size];
                switch (op.tag) {
                    case OP_ADD: {
                        asm_add(o1, o2);
                        break;
                    }
                    case OP_LE: {
                        asm_cmp(o1, o2);
                        asm_mov(o1, immediate(0));
                        asm_setle(o1);
                        break;
                    }
                    case OP_LT: {
                        asm_cmp(o1, o2);
                        asm_mov(o1, immediate(0));
                        asm_setl(o1);
                        break;
                    }
                    case OP_ASSIGN: {
                        asm_mov(o1, o2);
                        break;
                    }
                    default: {
                        fprintf(stderr, "TODO binary op %d\n", op.tag);
                        return (Expr){};
                    }
                }
            } else if (op.type == PREFIX) {
                fprintf(stderr, "TODO prefix op %d\n", op.tag);
                return (Expr){};
            } else {
                Operand o = eval_stack[eval_stack_size];
                switch (op.tag) {
                    case OP_INDEX: {
                        Operand idx = op.operand;
                        {
                            Operand ptr = expr_registers[expr_registers_busy++];
                            asm_lea(ptr, o);
                            asm_add(ptr, idx);
                            asm_mov(o, ptr);
                            expr_registers_busy--;
                        }
                        goto c;
                    }
                    case OP_INCREMENT: {
                        {
                            Operand tmp = expr_registers[expr_registers_busy++];
                            asm_mov(tmp, o);
                            asm_add(tmp, immediate(1));
                            asm_mov(o, tmp);
                            expr_registers_busy--;
                        }
                        goto c;
                    }
                    default: {
                        fprintf(stderr, "TODO postfix op %d\n", op.tag);
                        return (Expr){};
                    }
                }
            }
        } else {
            Operand o = expr_stack[expr_pos++].e.operand;
            if (eval_stack_size == 0 && need_tmp_register) {
                asm_mov(expr_registers[expr_registers_busy++], o);
                eval_stack[eval_stack_size++] = expr_registers[expr_registers_busy];
            } else {
                eval_stack[eval_stack_size++] = o;
            }
        }
    c:;
    }

    Expr res = {.ok = true};
    if (need_tmp_register) {
        expr_registers_busy--;
        res.operand = expr_registers[expr_registers_busy];
    } else {
        res.operand = eval_stack[0];
    }
    assert(expr_registers_busy_pre == expr_registers_busy);
    return res;
}
