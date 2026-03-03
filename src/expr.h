#include "core.h"
#include "emit.h"

Operand stack_alloc(size_t size);
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
        size_t token_pos;
    };
    bool emitted;
    Operand operand;
} ExprToken;

void operand_emit(ExprToken* o) {
    if (!o->emitted) {
        o->emitted = true;
        size_t token_pos_bk = token_pos;
        token_pos = o->token_pos;
        o->operand = visit_operand().operand;
        token_pos = token_pos_bk;
    }
}

/**
 * expr  = unary (op_infix unary)*
 * unary = op_prefix* operand op_postfix*
 *
 * @see https://en.wikipedia.org/wiki/Shunting_yard_algorithm
 */
bool shunting_yard(ExprToken out_queue[], size_t* out_queue_size) {
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
            expr_token.op = op;
        } else {
            Operator op = visit_op_prefix();
            if (op.tag != 0) {
                expr_token.is_operator = true;
                expr_token.op = op;
            } else {
                // postpone operand emit to have more flexibility in visit_expr
                // visit to advance token_pos, record operand_pos to "replay" emit later
                expr_token.token_pos = token_pos;

                size_t text_pos = text_size;
                Expr operand = visit_operand();
                text_size = text_pos;

                if (!operand.ok) return false;
                out_queue[(*out_queue_size)++] = expr_token;
                infix_postfix_time = true;
            }
        }

        if (expr_token.is_operator) {
            Operator op = expr_token.op;
            while (op_stack_size > 0 && (operator_precedence[op_stack[op_stack_size - 1].tag] <
                                             operator_precedence[op.tag] ||
                                         (operator_precedence[op_stack[op_stack_size - 1].tag] ==
                                              operator_precedence[op.tag] &&
                                          operator_associativity[op.tag] == ASSOC_LEFT))) {
                out_queue[(*out_queue_size)++] = (ExprToken){
                    .is_operator = true,
                    .op = op_stack[--op_stack_size],
                };
            }
            op_stack[op_stack_size++] = op;
        }
    }

    while (op_stack_size > 0) {
        out_queue[(*out_queue_size)++] = (ExprToken){
            .is_operator = true,
            .op = op_stack[--op_stack_size],
        };
    }
    return true;
}

Expr visit_expr_(ExprToken expr_stack[], size_t* pos) {
    Expr expr = {};
    ExprToken e = expr_stack[(*pos)++];
    if (e.is_operator) {
        Operator op = e.op;

        if (op.type == INFIX) {
            // some operators require operands to be emitted left-to-right
            size_t o2_pos = *pos;
            size_t text_pos = text_size;
            visit_expr_(expr_stack, pos);
            text_size = text_pos;

            Operand o1 = visit_expr_(expr_stack, pos).operand;
            size_t end_pos = *pos;

            size_t sc_je_pos;
            if (op.tag == OP_AND) {
                asm_cmp(o1.rvalue, immediate(0));
                sc_je_pos = text_size;
                asm_canary(6);
            }

            *pos = o2_pos;
            Operand o2 = visit_expr_(expr_stack, pos).operand;
            *pos = end_pos;

            Operand out = {.rvalue = expr_registers[expr_registers_busy++]};
            expr.operand = out;

            switch (op.tag) {
                case OP_ADD: {
                    asm_mov(out.rvalue, o2.rvalue);
                    asm_add(out.rvalue, o1.rvalue);
                    break;
                }
                case OP_LE: {
                    asm_cmp(o1.rvalue, o2.rvalue);
                    asm_mov(out.rvalue, immediate(0));
                    asm_setle(out.rvalue);
                    break;
                }
                case OP_LT: {
                    asm_cmp(o1.rvalue, o2.rvalue);
                    asm_mov(out.rvalue, immediate(0));
                    asm_setl(out.rvalue);
                    break;
                }
                case OP_GE: {
                    asm_cmp(o1.rvalue, o2.rvalue);
                    asm_mov(out.rvalue, immediate(0));
                    asm_setge(out.rvalue);
                    break;
                }
                case OP_GT: {
                    asm_cmp(o1.rvalue, o2.rvalue);
                    asm_mov(out.rvalue, immediate(0));
                    asm_setg(out.rvalue);
                    o1 = out;
                    break;
                }
                case OP_REMAINDER: {
                    asm_push(RAX);
                    asm_push(RDX);

                    asm_mov(RAX, o1.rvalue);
                    asm_mov(RDX, immediate(0));
                    asm_idiv(o2.rvalue);
                    Operand tmp_stack = stack_alloc(8);
                    asm_mov(tmp_stack.rvalue, RDX);

                    asm_pop(RDX);
                    asm_pop(RAX);

                    expr.operand = tmp_stack;
                    break;
                }
                case OP_AND: {
                    asm_cmp(o1.rvalue, immediate(0));
                    asm_mov(out.rvalue, immediate(0));
                    asm_setne(out.rvalue);

                    asm_cmp(o2.rvalue, immediate(0));
                    asm_setne(out.rvalue);

                    size_t text_size_bak = text_size;
                    text_size = sc_je_pos;
                    asm_je(text_size_bak - sc_je_pos - 6);
                    text_size = text_size_bak;

                    break;
                }
                case OP_OR: {
                    fprintf(stderr, "TODO op_or\n");
                    break;
                }
                case OP_EQ: {
                    asm_cmp(o1.rvalue, o2.rvalue);
                    asm_mov(out.rvalue, immediate(0));
                    asm_sete(out.rvalue);
                    break;
                }
                case OP_NEQ: {
                    fprintf(stderr, "TODO op_neq\n");
                    break;
                }
                case OP_ASSIGN: {
                    if (o1.lvalue.tag == 0) {
                        fprintf(stderr, "not an lvalue\n");
                        assert(false);
                        return (Expr){};
                    }
                    asm_mov(o1.lvalue, o2.rvalue);
                    expr.operand = o2;
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
            Operand o = visit_expr_(expr_stack, pos).operand;

            switch (op.tag) {
                case OP_INDEX: {
                    Operand idx = op.operand;

                    Operand_ ptr = expr_registers[expr_registers_busy++];
                    asm_mov(ptr, idx.rvalue);
                    asm_add(ptr, o.rvalue);

                    Operand_ ptr_ind = ptr;
                    // TODO: actual item size
                    ptr_ind.reg.size = 8;
                    ptr_ind.reg.indirect = true;
                    Operand res = {
                        .rvalue = ptr,
                        .lvalue = ptr_ind,
                    };

                    expr.operand = res;
                    break;
                }
                case OP_INCREMENT: {
                    Operand_ tmp = expr_registers[expr_registers_busy++];
                    asm_mov(tmp, o.rvalue);
                    asm_add(tmp, immediate(1));
                    asm_mov(o.rvalue, tmp);
                    expr_registers_busy--;
                    break;
                }
                default: {
                    fprintf(stderr, "TODO postfix op %d\n", op.tag);
                    return (Expr){};
                }
            }
        }
    } else {
        operand_emit(&e);
        expr.operand = e.operand;
    }

    expr.ok = true;
    return expr;
}

// expr = unary (op_infix unary)*
Expr visit_expr() {
    ExprToken expr_stack[1 << 10];
    size_t expr_stack_size = 0;

    bool ok = shunting_yard(expr_stack, &expr_stack_size);
    if (!ok) return (Expr){};

    for (size_t i = 0; i < expr_stack_size / 2; i++) {
        ExprToken temp = expr_stack[i];
        size_t with = expr_stack_size - 1 - i;
        expr_stack[i] = expr_stack[with];
        expr_stack[with] = temp;
    }

    size_t size = 0;
    Expr expr = visit_expr_(expr_stack, &size);
    assert(size == expr_stack_size);
    return expr;
}
