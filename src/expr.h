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

// expr = unary (op_infix unary)*
Expr visit_expr() {
    ExprToken expr_stack[1 << 10];
    size_t expr_stack_size = 0;

    bool ok = shunting_yard(expr_stack, &expr_stack_size);
    if (!ok) return (Expr){};

    assert(expr_stack_size > 0);
    size_t expr_pos = 0;
    ExprToken eval_stack[1 << 10] = {};
    size_t eval_stack_size = 0;
    while (expr_pos < expr_stack_size) {
        if (expr_stack[expr_pos].is_operator) {
            Operator op = expr_stack[expr_pos++].op;

            assert(eval_stack_size > 0);
            ExprToken* e2 = &eval_stack[--eval_stack_size];
            operand_emit(e2);
            Operand o2 = e2->operand;

            if (op.type == INFIX) {
                assert(eval_stack_size > 0);
                ExprToken* e1 = &eval_stack[eval_stack_size - 1];
                Operand tmp = expr_registers[expr_registers_busy++];

                operand_emit(e1);
                Operand* o1 = &e1->operand;

                switch (op.tag) {
                    case OP_ADD: {
                        asm_mov(tmp, o2);
                        asm_add(tmp, *o1);
                        *o1 = tmp;
                        break;
                    }
                    case OP_LE: {
                        asm_cmp(*o1, o2);
                        asm_mov(tmp, immediate(0));
                        asm_setle(tmp);
                        *o1 = tmp;
                        break;
                    }
                    case OP_LT: {
                        asm_cmp(*o1, o2);
                        asm_mov(tmp, immediate(0));
                        asm_setl(tmp);
                        *o1 = tmp;
                        break;
                    }
                    case OP_GE: {
                        asm_cmp(*o1, o2);
                        asm_mov(tmp, immediate(0));
                        asm_setge(tmp);
                        *o1 = tmp;
                        break;
                    }
                    case OP_GT: {
                        asm_cmp(*o1, o2);
                        asm_mov(tmp, immediate(0));
                        asm_setg(tmp);
                        *o1 = tmp;
                        break;
                    }
                    case OP_REMAINDER: {
                        asm_push(RAX);
                        asm_push(RDX);

                        asm_mov(RAX, *o1);
                        asm_mov(RDX, immediate(0));
                        asm_idiv(o2);
                        Operand tmp_stack = stack_alloc(8);
                        asm_mov(tmp_stack, RDX);

                        asm_pop(RDX);
                        asm_pop(RAX);

                        *o1 = tmp_stack;
                        break;
                    }
                    case OP_AND: {
                        asm_cmp(*o1, immediate(0));
                        asm_mov(tmp, immediate(0));
                        asm_setne(tmp);

                        size_t je_pos = text_size;
                        asm_canary(6);

                        asm_cmp(o2, immediate(0));
                        asm_mov(tmp, immediate(0));
                        asm_setne(tmp);

                        size_t text_size_bak = text_size;
                        text_size = je_pos;
                        asm_je(text_size_bak - je_pos - 6);
                        text_size = text_size_bak;

                        *o1 = tmp;
                        break;
                    }
                    case OP_OR: {
                        fprintf(stderr, "TODO op_or\n");
                        break;
                    }
                    case OP_EQ: {
                        asm_cmp(*o1, o2);
                        asm_mov(tmp, immediate(0));
                        asm_sete(tmp);
                        *o1 = tmp;
                        break;
                    }
                    case OP_NEQ: {
                        fprintf(stderr, "TODO op_neq\n");
                        break;
                    }
                    case OP_ASSIGN: {
                        if (o1->lvalue.mode == 0) {
                            fprintf(stderr, "not an lvalue\n");
                            assert(false);
                            return (Expr){};
                        }
                        asm_mov((Operand){.tag = MEMORY, .memory = o1->lvalue}, o2);
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
                Operand* o2 = &e2->operand;
                switch (op.tag) {
                    case OP_INDEX: {
                        Operand idx = op.operand;
                        {
                            Operand ptr = stack_alloc(8);
                            asm_mov(ptr, idx);
                            asm_add(ptr, *o2);

                            Operand res = expr_registers[expr_registers_busy++];
                            res.lvalue = ptr.lvalue;
                            asm_mov(res, ptr);

                            *o2 = res;
                        }
                        goto c;
                    }
                    case OP_INCREMENT: {
                        {
                            Operand tmp = expr_registers[expr_registers_busy++];
                            asm_mov(tmp, *o2);
                            asm_add(tmp, immediate(1));
                            asm_mov(*o2, tmp);
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
            ExprToken* o = &expr_stack[expr_pos++];
            if (eval_stack_size == 0) operand_emit(o);
            eval_stack[eval_stack_size++] = *o;
        }
    c:;
    }

    Expr res = {.ok = true};
    res.operand = eval_stack[0].operand;
    return res;
}
