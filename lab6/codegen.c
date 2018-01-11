#include <stdio.h>
#include <stdlib.h>
#include "util.h"
#include "symbol.h"
#include "absyn.h"
#include "temp.h"
#include "errormsg.h"
#include "tree.h"
#include "assem.h"
#include "frame.h"
#include "codegen.h"
#include "table.h"

#define IS_PLUS(exp) (exp->kind == T_BINOP && exp->u.BINOP.op == T_plus)
#define IS_MUL(exp) (exp->kind == T_BINOP && exp->u.BINOP.op == T_mul)
#define IS_CONST(exp) (exp->kind == T_CONST)
#define IS_MEM(exp) (exp->kind == T_MEM)
#define IS_TEMP(exp) (exp->kind == T_TEMP)
#define IS_NAME(exp) (exp->kind == T_NAME)
#define HAS_CONST(exp) (IS_CONST(exp->u.BINOP.left) || IS_CONST(exp->u.BINOP.right))
#define HAS_MUL(exp) (IS_MUL(exp->u.BINOP.left) || IS_MUL(exp->u.BINOP.right))


struct exp_munch {
    AS_instrList list;
    Temp_temp temp;
};

typedef struct tempRefList_ *tempRefList;
struct tempRefList_ {
    Temp_temp *head;
    tempRefList tail;
};

struct pattern_candidate {
    AS_instr inst;
    int cost;
    T_expList to_munch;
    tempRefList temp_ref_list;
};


/*
 * func prototypes
 */
struct exp_munch munchExp(F_frame frame, T_exp exp);

AS_instrList munchStm(F_frame frame, T_stm stm);


/*
 * helper funcs
 */
T_expList reverse(T_expList original) {
    if (original == NULL)
        return NULL;

    T_expList *last = &original;
    while ((*last)->tail) last = &((*last)->tail);

    T_exp exp = (*last)->head;
    *last = NULL;
    return T_ExpList(exp, reverse(original));
}


tempRefList TempRefList(Temp_temp *head, tempRefList tail) {
    tempRefList p = (tempRefList) checked_malloc(sizeof *p);
    p->head = head;
    p->tail = tail;
    return p;
}

// reg <- mem(ib + ri * s)
AS_instr mem_ib_plus_ri_mul_scale_to_reg(int base, Temp_temp index, int scale, Temp_temp dst) {
    char inst[100];
    sprintf(inst, "movl %d(, `s0, %d), `d0", base, scale);
    return AS_Oper(String(inst), Temp_TempList(dst, NULL), Temp_TempList(index, NULL), NULL);
}

// reg <- mem(rb + ri * s)
AS_instr mem_rb_plus_ri_mul_scale_to_reg(Temp_temp base, Temp_temp index, int scale, Temp_temp dst) {
    char inst[100];
    sprintf(inst, "movl (`s0, `s1, %d), `d0", scale);
    return AS_Oper(String(inst), Temp_TempList(dst, NULL), Temp_TempList(base, Temp_TempList(index, NULL)), NULL);
}

// reg <- mem(ib + rb)
AS_instr mem_ib_plus_rb_to_reg(int displacement, Temp_temp base, Temp_temp dst) {
    char inst[100];
    sprintf(inst, "movl %d(`s0), `d0", displacement);
    return AS_Oper(String(inst), Temp_TempList(dst, NULL), Temp_TempList(base, NULL), NULL);
}

// reg <- mem(rb + ri)
AS_instr mem_rb_plus_ri_to_reg(Temp_temp base, Temp_temp index, Temp_temp dst) {
    return AS_Oper(String("movl (`s0, `s1), `d0"),
                   Temp_TempList(dst, NULL), Temp_TempList(base, Temp_TempList(index, NULL)), NULL);
}

// reg <- mem(ib)
AS_instr mem_ib_to_reg(int addr, Temp_temp dst) {
    char inst[100];
    sprintf(inst, "movl %d, `d0", addr);
    return AS_Oper(String(inst), Temp_TempList(dst, NULL), NULL, NULL);
}

// reg <- mem(rb)
AS_instr mem_rb_to_reg(Temp_temp addr, Temp_temp dst) {
    return AS_Oper(String("movl (`s0), `d0"), Temp_TempList(dst, NULL), Temp_TempList(addr, NULL), NULL);
}

// mem(ib + ri * s) <- imm
AS_instr imm_to_mem_ib_plus_ri_mul_scale(int src, int base, Temp_temp index, int scale) {
    char inst[100];
    sprintf(inst, "movl $%d, %d(, `s0, %d)", src, base, scale);
    return AS_Oper(String(inst), NULL, Temp_TempList(index, NULL), NULL);
}

// mem(ib + ri * s) <- label
AS_instr label_to_mem_ib_plus_ri_mul_scale(Temp_label src, int base, Temp_temp index, int scale) {
    char inst[100];
    sprintf(inst, "movl $%s, %d(, `s0, %d)", Temp_labelstring(src), base, scale);
    return AS_Oper(String(inst), NULL, Temp_TempList(index, NULL), NULL);
}

// mem(ib + ri * s) <- reg
AS_instr reg_to_mem_ib_plus_ri_mul_scale(Temp_temp src, int base, Temp_temp index, int scale) {
    char inst[100];
    sprintf(inst, "movl `s0, %d(, `s1, %d)", base, scale);
    return AS_Oper(String(inst), NULL, Temp_TempList(src, Temp_TempList(index, NULL)), NULL);
}

// mem(rb + ri * s) <- imm
AS_instr imm_to_mem_rb_plus_ri_mul_scale(int src, Temp_temp base, Temp_temp index, int scale) {
    char inst[100];
    sprintf(inst, "movl $%d, (`s0, `s1, %d)", src, scale);
    return AS_Oper(String(inst), NULL, Temp_TempList(base, Temp_TempList(index, NULL)), NULL);
}

// mem(rb + ri * s) <- label
AS_instr label_to_mem_rb_plus_ri_mul_scale(Temp_label src, Temp_temp base, Temp_temp index, int scale) {
    char inst[100];
    sprintf(inst, "movl $%s, (`s0, `s1, %d)", Temp_labelstring(src), scale);
    return AS_Oper(String(inst), NULL, Temp_TempList(base, Temp_TempList(index, NULL)), NULL);
}

// mem(rb + ri * s) <- reg
AS_instr reg_to_mem_rb_plus_ri_mul_scale(Temp_temp src, Temp_temp base, Temp_temp index, int scale) {
    char inst[100];
    sprintf(inst, "movl `s0, (`s1, `s2, %d)", scale);
    return AS_Oper(String(inst), NULL, Temp_TempList(src, Temp_TempList(base, Temp_TempList(index, NULL))), NULL);
}

// mem(ib + rb) <- imm
AS_instr imm_to_mem_ib_plus_rb(int src, int displacement, Temp_temp base) {
    char inst[100];
    sprintf(inst, "movl $%d, %d(`s0)", src, displacement);
    return AS_Oper(String(inst), NULL, Temp_TempList(base, NULL), NULL);
}

// mem(ib + rb) <- label
AS_instr label_to_mem_ib_plus_rb(Temp_label src, int displacement, Temp_temp base) {
    char inst[100];
    sprintf(inst, "movl $%s, %d(`s0)", Temp_labelstring(src), displacement);
    return AS_Oper(String(inst), NULL, Temp_TempList(base, NULL), NULL);
}

// mem(ib + rb) <- reg
AS_instr reg_to_mem_ib_plus_rb(Temp_temp src, int displacement, Temp_temp base) {
    char inst[100];
    sprintf(inst, "movl `s0, %d(`s1)", displacement);
    return AS_Oper(String(inst), NULL, Temp_TempList(src, Temp_TempList(base, NULL)), NULL);
}

// mem(rb + ri) <- imm
AS_instr imm_to_mem_rb_plus_ri(int src, Temp_temp base, Temp_temp index) {
    char inst[100];
    sprintf(inst, "movl $%d, (`s0, `s1)", src);
    return AS_Oper(String(inst), NULL, Temp_TempList(base, Temp_TempList(index, NULL)), NULL);
}

// mem(rb + ri) <- label
AS_instr label_to_mem_rb_plus_ri(Temp_label src, Temp_temp base, Temp_temp index) {
    char inst[100];
    sprintf(inst, "movl $%s, (`s0, `s1)", Temp_labelstring(src));
    return AS_Oper(String(inst), NULL, Temp_TempList(base, Temp_TempList(index, NULL)), NULL);
}

// mem(rb + ri) <- reg
AS_instr reg_to_mem_rb_plus_ri(Temp_temp src, Temp_temp base, Temp_temp index) {
    return AS_Oper(String("movl `s0, (`s1, `s2)"),
                   NULL, Temp_TempList(src, Temp_TempList(base, Temp_TempList(index, NULL))), NULL);
}

// mem(ib) <- imm
AS_instr imm_to_mem_ib(int src, int addr) {
    char inst[100];
    sprintf(inst, "movl $%d, %d", src, addr);
    return AS_Oper(String(inst), NULL, NULL, NULL);
}

// mem(ib) <- label
AS_instr label_to_mem_ib(Temp_label src, int addr) {
    char inst[100];
    sprintf(inst, "movl $%s, %d", Temp_labelstring(src), addr);
    return AS_Oper(String(inst), NULL, NULL, NULL);
}

// mem(ib) <- reg
AS_instr reg_to_mem_ib(Temp_temp src, int addr) {
    char inst[100];
    sprintf(inst, "movl `s0, %d", addr);
    return AS_Oper(String(inst), NULL, Temp_TempList(src, NULL), NULL);
}

// mem(rb) <- imm
AS_instr imm_to_mem_rb(int src, Temp_temp addr) {
    char inst[100];
    sprintf(inst, "movl $%d, (`s0)", src);
    return AS_Oper(String(inst), NULL, Temp_TempList(addr, NULL), NULL);
}

// mem(rb) <- label
AS_instr label_to_mem_rb(Temp_label src, Temp_temp addr) {
    char inst[100];
    sprintf(inst, "movl $%s, (`s0)", Temp_labelstring(src));
    return AS_Oper(String(inst), NULL, Temp_TempList(addr, NULL), NULL);
}

// mem(rb) <- reg
AS_instr reg_to_mem_rb(Temp_temp src, Temp_temp addr) {
    return AS_Oper(String("movl `s0, (`s1)"), NULL, Temp_TempList(src, Temp_TempList(addr, NULL)), NULL);
}

// reg <- imm
AS_instr imm_to_reg(int src, Temp_temp dst) {
    char inst[100];
    sprintf(inst, "movl $%d, `d0", src);
    return AS_Oper(String(inst), Temp_TempList(dst, NULL), NULL, NULL);
}

// reg <- label
AS_instr label_to_reg(Temp_label src, Temp_temp dst) {
    char inst[100];
    sprintf(inst, "movl $%s, `d0", Temp_labelstring(src));
    return AS_Oper(String(inst), Temp_TempList(dst, NULL), NULL, NULL);
}

// reg <- reg
AS_instr reg_to_reg(Temp_temp src, Temp_temp dst) {
    return AS_Move(String("movl `s0, `d0"), Temp_TempList(dst, NULL), Temp_TempList(src, NULL));
}

// op with mem(ib + ri * s)
AS_instr op_mem_ib_plus_ri_mul_scale(int base, Temp_temp index, int scale, Temp_temp dst, string op) {
    char inst[100];
    sprintf(inst, "%s %d(, `s0, %d), `d0", op, base, scale);
    return AS_Oper(String(inst), Temp_TempList(dst, NULL), Temp_TempList(index, Temp_TempList(dst, NULL)), NULL);
}

// op with mem(rb + ri * s)
AS_instr op_mem_rb_plus_ri_mul_scale(Temp_temp base, Temp_temp index, int scale, Temp_temp dst, string op) {
    char inst[100];
    sprintf(inst, "%s (`s0, `s1, %d), `d0", op, scale);
    return AS_Oper(String(inst), Temp_TempList(dst, NULL),
                   Temp_TempList(base, Temp_TempList(index, Temp_TempList(dst, NULL))), NULL);
}

// op with mem(ib + rb)
AS_instr op_mem_ib_plus_rb(int displacement, Temp_temp base, Temp_temp dst, string op) {
    char inst[100];
    sprintf(inst, "%s %d(`s0), `d0", op, displacement);
    return AS_Oper(String(inst), Temp_TempList(dst, NULL), Temp_TempList(base, Temp_TempList(dst, NULL)), NULL);
}

// op with mem(rb + ri)
AS_instr op_mem_rb_plus_ri(Temp_temp base, Temp_temp index, Temp_temp dst, string op) {
    char inst[100];
    sprintf(inst, "%s (`s0, `s1), `d0", op);
    return AS_Oper(String(inst), Temp_TempList(dst, NULL),
                   Temp_TempList(base, Temp_TempList(index, Temp_TempList(dst, NULL))), NULL);
}

// op with mem(ib)
AS_instr op_mem_ib(int addr, Temp_temp dst, string op) {
    char inst[100];
    sprintf(inst, "%s %d, `d0", op, addr);
    return AS_Oper(String(inst), Temp_TempList(dst, NULL), Temp_TempList(dst, NULL), NULL);
}

// op with mem(rb)
AS_instr op_mem_rb(Temp_temp addr, Temp_temp dst, string op) {
    char inst[100];
    sprintf(inst, "%s (`s0), `d0", op);
    return AS_Oper(String(inst), Temp_TempList(dst, NULL), Temp_TempList(addr, Temp_TempList(dst, NULL)), NULL);
}

// op with imm
AS_instr op_imm(int src, Temp_temp dst, string op) {
    char inst[100];
    sprintf(inst, "%s $%d, `d0", op, src);
    return AS_Oper(String(inst), Temp_TempList(dst, NULL), Temp_TempList(dst, NULL), NULL);
}

// op with label
AS_instr op_label(Temp_label src, Temp_temp dst, string op) {
    char inst[100];
    sprintf(inst, "%s $%s, `d0", op, Temp_labelstring(src));
    return AS_Oper(String(inst), Temp_TempList(dst, NULL), Temp_TempList(dst, NULL), NULL);
}

// op with reg
AS_instr op_reg(Temp_temp src, Temp_temp dst, string op) {
    char inst[100];
    sprintf(inst, "%s `s0, `d0", op);
    return AS_Oper(String(inst), Temp_TempList(dst, NULL), Temp_TempList(src, Temp_TempList(dst, NULL)), NULL);
}


AS_instrList munchStm(F_frame frame, T_stm stm) {
    switch (stm->kind) {
        case T_SEQ: {
            printf("munchStm: T_SEQ should be eliminated after canonicalization\n");
            assert(0);
        }
        case T_LABEL: {
            AS_instr instr = AS_Label(Temp_labelstring(stm->u.LABEL), stm->u.LABEL);
            return AS_InstrList(instr, NULL);
        }
        case T_JUMP: {
            if (stm->u.JUMP.exp->kind != T_NAME) {
                printf("munchStm: T_JUMP should have a T_NAME exp as target\n");
                assert(0);
            }
            AS_instr instr = AS_Oper(String("jmp `j0"), NULL, NULL, AS_Targets(stm->u.JUMP.jumps));
            return AS_InstrList(instr, NULL);
        }
        case T_CJUMP: {
            struct exp_munch left = munchExp(frame, stm->u.CJUMP.left);
            struct exp_munch right = munchExp(frame, stm->u.CJUMP.right);
            AS_instr cmp_instr = AS_Oper(String("cmpl `s0, `s1"), NULL,
                                         Temp_TempList(right.temp, Temp_TempList(left.temp, NULL)), NULL);
            AS_instr jmp_instr_true = NULL;
            switch (stm->u.CJUMP.op) {
                case T_eq:
                    jmp_instr_true = AS_Oper(String("je `j0"),
                                             NULL, NULL, AS_Targets(Temp_LabelList(stm->u.CJUMP.
                    true, NULL)));
                    break;
                case T_ne:
                    jmp_instr_true = AS_Oper(String("jne `j0"),
                                             NULL, NULL, AS_Targets(Temp_LabelList(stm->u.CJUMP.
                    true, NULL)));
                    break;
                case T_lt:
                    jmp_instr_true = AS_Oper(String("jl `j0"),
                                             NULL, NULL, AS_Targets(Temp_LabelList(stm->u.CJUMP.
                    true, NULL)));
                    break;
                case T_le:
                    jmp_instr_true = AS_Oper(String("jle `j0"),
                                             NULL, NULL, AS_Targets(Temp_LabelList(stm->u.CJUMP.
                    true, NULL)));
                    break;
                case T_gt:
                    jmp_instr_true = AS_Oper(String("jg `j0"),
                                             NULL, NULL, AS_Targets(Temp_LabelList(stm->u.CJUMP.
                    true, NULL)));
                    break;
                case T_ge:
                    jmp_instr_true = AS_Oper(String("jge `j0"),
                                             NULL, NULL, AS_Targets(Temp_LabelList(stm->u.CJUMP.
                    true, NULL)));
                    break;
                case T_ult:
                    jmp_instr_true = AS_Oper(String("jb `j0"),
                                             NULL, NULL, AS_Targets(Temp_LabelList(stm->u.CJUMP.
                    true, NULL)));
                    break;
                case T_ule:
                    jmp_instr_true = AS_Oper(String("jbe `j0"),
                                             NULL, NULL, AS_Targets(Temp_LabelList(stm->u.CJUMP.
                    true, NULL)));
                    break;
                case T_ugt:
                    jmp_instr_true = AS_Oper(String("ja `j0"),
                                             NULL, NULL, AS_Targets(Temp_LabelList(stm->u.CJUMP.
                    true, NULL)));
                    break;
                case T_uge:
                    jmp_instr_true = AS_Oper(String("jae `j0"),
                                             NULL, NULL, AS_Targets(Temp_LabelList(stm->u.CJUMP.
                    true, NULL)));
                    break;
            }
            AS_instr jmp_instr_false = AS_Oper(
                    String("jmp `j0"), NULL, NULL, AS_Targets(Temp_LabelList(stm->u.CJUMP.
            false, NULL)));

            AS_instrList cmp_and_jump = AS_InstrList(
                    cmp_instr, AS_InstrList(jmp_instr_true, AS_InstrList(jmp_instr_false, NULL)));

            return AS_splice(left.list, AS_splice(right.list, cmp_and_jump));
        }
        case T_MOVE: {
            AS_instrList result = NULL;

            struct pattern_candidate best_yet = {.cost = -1};

            // TODO: add imm(reg, reg), imm(reg, reg, s), if you really have nothing to do
            // if dst is mem, then must save result to mem
            if (IS_MEM(stm->u.MOVE.dst)) {  // mem <- ?
                int src = IS_CONST(stm->u.MOVE.src) ? stm->u.MOVE.src->u.CONST : 0;  // prefetch src imm is possible

                T_exp addr = stm->u.MOVE.dst->u.MEM;
                if (IS_PLUS(addr)) {
                    if (HAS_MUL(addr)) {
                        T_exp mul = IS_MUL(addr->u.BINOP.left) ? addr->u.BINOP.left : addr->u.BINOP.right;

                        if (HAS_CONST(mul)) {
                            int scale = IS_CONST(mul->u.BINOP.left) ? mul->u.BINOP.left->u.CONST
                                                                    : mul->u.BINOP.right->u.CONST;
                            T_exp index = IS_CONST(mul->u.BINOP.left) ? mul->u.BINOP.right : mul->u.BINOP.left;

                            if (HAS_CONST(addr)) {
                                int base = IS_CONST(addr->u.BINOP.left) ? addr->u.BINOP.left->u.CONST
                                                                        : addr->u.BINOP.right->u.CONST;

                                if (IS_CONST(stm->u.MOVE.src) && best_yet.cost < 7) {  // mem(ib + ri * s) <- imm
                                    best_yet.cost = 7;
                                    best_yet.inst = imm_to_mem_ib_plus_ri_mul_scale(src, base, NULL, scale);
                                    best_yet.temp_ref_list = TempRefList(&(best_yet.inst->u.MOVE.src->head), NULL);
                                    best_yet.to_munch = T_ExpList(index, NULL);
                                }

                                if (IS_NAME(stm->u.MOVE.src) && best_yet.cost < 7) {  // mem(ib + ri * s) <- label
                                    best_yet.cost = 7;
                                    best_yet.inst = label_to_mem_ib_plus_ri_mul_scale(stm->u.MOVE.src->u.NAME, base,
                                                                                      NULL, scale);
                                    best_yet.temp_ref_list = TempRefList(&(best_yet.inst->u.MOVE.src->head), NULL);
                                    best_yet.to_munch = T_ExpList(index, NULL);
                                }

                                if (best_yet.cost < 6) {  // mem(ib + ri * s) <- reg
                                    best_yet.cost = 6;
                                    best_yet.inst = reg_to_mem_ib_plus_ri_mul_scale(NULL, base, NULL, scale);
                                    best_yet.temp_ref_list = TempRefList(
                                            &(best_yet.inst->u.MOVE.src->head),
                                            TempRefList(&(best_yet.inst->u.MOVE.src->tail->head), NULL));
                                    best_yet.to_munch = T_ExpList(stm->u.MOVE.src, T_ExpList(index, NULL));
                                }
                            }

                            T_exp base = IS_MUL(addr->u.BINOP.left) ? addr->u.BINOP.right : addr->u.BINOP.left;

                            if (IS_CONST(stm->u.MOVE.src) && best_yet.cost < 6) {  // mem(rb + ri * s) <- imm
                                best_yet.cost = 6;
                                best_yet.inst = imm_to_mem_rb_plus_ri_mul_scale(src, NULL, NULL, scale);
                                best_yet.temp_ref_list = TempRefList(
                                        &(best_yet.inst->u.MOVE.src->head),
                                        TempRefList(&(best_yet.inst->u.MOVE.src->tail->head), NULL));
                                best_yet.to_munch = T_ExpList(base, T_ExpList(index, NULL));
                            }

                            if (IS_NAME(stm->u.MOVE.src) && best_yet.cost < 6) {  // mem(rb + ri * s) <- label
                                best_yet.cost = 6;
                                best_yet.inst = label_to_mem_rb_plus_ri_mul_scale(stm->u.MOVE.src->u.NAME, NULL, NULL,
                                                                                  scale);
                                best_yet.temp_ref_list = TempRefList(
                                        &(best_yet.inst->u.MOVE.src->head),
                                        TempRefList(&(best_yet.inst->u.MOVE.src->tail->head), NULL));
                                best_yet.to_munch = T_ExpList(base, T_ExpList(index, NULL));
                            }

                            if (best_yet.cost < 5) {  // mem(rb + ri * s) <- reg
                                best_yet.cost = 5;
                                best_yet.inst = reg_to_mem_rb_plus_ri_mul_scale(NULL, NULL, NULL, scale);
                                best_yet.temp_ref_list = TempRefList(
                                        &(best_yet.inst->u.MOVE.src->head),
                                        TempRefList(
                                                &(best_yet.inst->u.MOVE.src->tail->head),
                                                TempRefList(&(best_yet.inst->u.MOVE.src->tail->tail->head), NULL)));
                                best_yet.to_munch = T_ExpList(stm->u.MOVE.src, T_ExpList(base, T_ExpList(index, NULL)));
                            }
                        }
                    }

                    if (HAS_CONST(addr)) {
                        int displacement = IS_CONST(addr->u.BINOP.left) ? addr->u.BINOP.left->u.CONST
                                                                        : addr->u.BINOP.right->u.CONST;
                        T_exp base = IS_CONST(addr->u.BINOP.left) ? addr->u.BINOP.right : addr->u.BINOP.left;

                        if (IS_CONST(stm->u.MOVE.src) && best_yet.cost < 5) {  // mem(imm + reg) <- imm
                            best_yet.cost = 5;
                            best_yet.inst = imm_to_mem_ib_plus_rb(src, displacement, NULL);
                            best_yet.temp_ref_list = TempRefList(&(best_yet.inst->u.MOVE.src->head), NULL);
                            best_yet.to_munch = T_ExpList(base, NULL);
                        }

                        if (IS_CONST(stm->u.MOVE.src) && best_yet.cost < 5) {  // mem(imm + reg) <- label
                            best_yet.cost = 5;
                            best_yet.inst = label_to_mem_ib_plus_rb(stm->u.MOVE.src->u.NAME, displacement, NULL);
                            best_yet.temp_ref_list = TempRefList(&(best_yet.inst->u.MOVE.src->head), NULL);
                            best_yet.to_munch = T_ExpList(base, NULL);
                        }

                        if (best_yet.cost < 4) {  // mem(imm + reg) <- reg
                            best_yet.cost = 4;
                            best_yet.inst = reg_to_mem_ib_plus_rb(NULL, displacement, NULL);
                            best_yet.temp_ref_list = TempRefList(
                                    &(best_yet.inst->u.MOVE.src->head),
                                    TempRefList(&(best_yet.inst->u.MOVE.src->tail->head), NULL));
                            best_yet.to_munch = T_ExpList(stm->u.MOVE.src, T_ExpList(base, NULL));
                        }
                    }

                    if (IS_CONST(stm->u.MOVE.src) && best_yet.cost < 4) {  // mem(reg + reg) <- imm
                        best_yet.cost = 4;
                        best_yet.inst = imm_to_mem_rb_plus_ri(src, NULL, NULL);
                        best_yet.temp_ref_list = TempRefList(
                                &(best_yet.inst->u.MOVE.src->head),
                                TempRefList(&(best_yet.inst->u.MOVE.src->tail->head), NULL));
                        best_yet.to_munch = T_ExpList(addr->u.BINOP.left, T_ExpList(addr->u.BINOP.right, NULL));
                    }

                    if (IS_NAME(stm->u.MOVE.src) && best_yet.cost < 4) {  // mem(reg + reg) <- label
                        best_yet.cost = 4;
                        best_yet.inst = label_to_mem_rb_plus_ri(stm->u.MOVE.src->u.NAME, NULL, NULL);
                        best_yet.temp_ref_list = TempRefList(
                                &(best_yet.inst->u.MOVE.src->head),
                                TempRefList(&(best_yet.inst->u.MOVE.src->tail->head), NULL));
                        best_yet.to_munch = T_ExpList(addr->u.BINOP.left, T_ExpList(addr->u.BINOP.right, NULL));
                    }

                    if (best_yet.cost < 3) {  // mem(reg + reg) <- reg
                        best_yet.cost = 3;
                        best_yet.inst = reg_to_mem_rb_plus_ri(NULL, NULL, NULL);
                        best_yet.temp_ref_list = TempRefList(
                                &(best_yet.inst->u.MOVE.src->head),
                                TempRefList(
                                        &(best_yet.inst->u.MOVE.src->tail->head),
                                        TempRefList(&(best_yet.inst->u.MOVE.src->tail->tail->head), NULL)));
                        best_yet.to_munch = T_ExpList(
                                stm->u.MOVE.src, T_ExpList(addr->u.BINOP.left, T_ExpList(addr->u.BINOP.right, NULL)));
                    }
                }

                if (IS_CONST(addr)) {
                    int base = addr->u.CONST;

                    if (IS_CONST(stm->u.MOVE.src) && best_yet.cost < 4) {  // mem(imm) <- imm
                        best_yet.cost = 4;
                        best_yet.inst = imm_to_mem_ib(src, base);
                        best_yet.temp_ref_list = NULL;
                        best_yet.to_munch = NULL;
                    }

                    if (IS_NAME(stm->u.MOVE.src) && best_yet.cost < 4) {  // mem(imm) <- label
                        best_yet.cost = 4;
                        best_yet.inst = label_to_mem_ib(stm->u.MOVE.src->u.NAME, base);
                        best_yet.temp_ref_list = NULL;
                        best_yet.to_munch = NULL;
                    }

                    if (best_yet.cost < 3) {  // mem(imm) <- reg
                        best_yet.cost = 3;
                        best_yet.inst = reg_to_mem_ib(NULL, base);
                        best_yet.temp_ref_list = TempRefList(&(best_yet.inst->u.MOVE.src->head), NULL);
                        best_yet.to_munch = T_ExpList(stm->u.MOVE.src, NULL);
                    }
                }

                if (IS_CONST(stm->u.MOVE.src) && best_yet.cost < 3) {  // mem(reg) <- imm
                    best_yet.cost = 3;
                    best_yet.inst = imm_to_mem_rb(src, NULL);
                    best_yet.temp_ref_list = TempRefList(&(best_yet.inst->u.MOVE.src->head), NULL);
                    best_yet.to_munch = T_ExpList(addr, NULL);
                }

                if (IS_NAME(stm->u.MOVE.src) && best_yet.cost < 3) {  // mem(reg) <- label
                    best_yet.cost = 3;
                    best_yet.inst = label_to_mem_rb(stm->u.MOVE.src->u.NAME, NULL);
                    best_yet.temp_ref_list = TempRefList(&(best_yet.inst->u.MOVE.src->head), NULL);
                    best_yet.to_munch = T_ExpList(addr, NULL);
                }

                if (best_yet.cost < 2) {  // mem(reg) <- reg
                    best_yet.cost = 2;
                    best_yet.inst = reg_to_mem_rb(NULL, NULL);
                    best_yet.temp_ref_list = TempRefList(
                            &(best_yet.inst->u.MOVE.src->head),
                            TempRefList(&(best_yet.inst->u.MOVE.src->tail->head), NULL));
                    best_yet.to_munch = T_ExpList(stm->u.MOVE.src, T_ExpList(addr, NULL));
                }
            } else {
                if (IS_MEM(stm->u.MOVE.src)) {  // reg <- mem
                    T_exp addr = stm->u.MOVE.src->u.MEM;
                    if (IS_PLUS(addr)) {
                        if (HAS_MUL(addr)) {
                            T_exp mul = IS_MUL(addr->u.BINOP.left) ? addr->u.BINOP.left : addr->u.BINOP.right;

                            if (HAS_CONST(mul)) {
                                int scale = IS_CONST(mul->u.BINOP.left) ? mul->u.BINOP.left->u.CONST
                                                                        : mul->u.BINOP.right->u.CONST;
                                T_exp index = IS_CONST(mul->u.BINOP.left) ? mul->u.BINOP.right : mul->u.BINOP.left;

                                if (HAS_CONST(addr) && best_yet.cost < 6) {  // reg <- mem(ib + ri * s)
                                    int base = IS_CONST(addr->u.BINOP.left) ? addr->u.BINOP.left->u.CONST
                                                                            : addr->u.BINOP.right->u.CONST;

                                    best_yet.cost = 6;
                                    best_yet.inst = mem_ib_plus_ri_mul_scale_to_reg(base, NULL, scale, NULL);
                                    best_yet.temp_ref_list = TempRefList(
                                            &(best_yet.inst->u.MOVE.src->head),
                                            TempRefList(&(best_yet.inst->u.MOVE.dst->head), NULL));
                                    best_yet.to_munch = T_ExpList(index, T_ExpList(stm->u.MOVE.dst, NULL));
                                }

                                if (best_yet.cost < 5) {  // reg <- mem(rb + ri * s)
                                    T_exp base = IS_MUL(addr->u.BINOP.left) ? addr->u.BINOP.right : addr->u.BINOP.left;

                                    best_yet.cost = 5;
                                    best_yet.inst = mem_rb_plus_ri_mul_scale_to_reg(NULL, NULL, scale, NULL);
                                    best_yet.temp_ref_list = TempRefList(
                                            &(best_yet.inst->u.MOVE.src->head),
                                            TempRefList(
                                                    &(best_yet.inst->u.MOVE.src->tail->head),
                                                    TempRefList(&(best_yet.inst->u.MOVE.dst->head), NULL)));
                                    best_yet.to_munch = T_ExpList(base,
                                                                  T_ExpList(index, T_ExpList(stm->u.MOVE.dst, NULL)));
                                }
                            }
                        }

                        if (HAS_CONST(addr) && best_yet.cost < 4) {  // reg <- mem(imm + reg)
                            int displacement = IS_CONST(addr->u.BINOP.left) ? addr->u.BINOP.left->u.CONST
                                                                            : addr->u.BINOP.right->u.CONST;
                            T_exp base = IS_CONST(addr->u.BINOP.left) ? addr->u.BINOP.right : addr->u.BINOP.left;

                            best_yet.cost = 4;
                            best_yet.inst = mem_ib_plus_rb_to_reg(displacement, NULL, NULL);
                            best_yet.temp_ref_list = TempRefList(
                                    &(best_yet.inst->u.MOVE.src->head),
                                    TempRefList(&(best_yet.inst->u.MOVE.dst->head), NULL));
                            best_yet.to_munch = T_ExpList(base, T_ExpList(stm->u.MOVE.dst, NULL));
                        }

                        if (best_yet.cost < 3) {  // reg <- mem(reg + reg)
                            best_yet.cost = 3;
                            best_yet.inst = mem_rb_plus_ri_to_reg(NULL, NULL, NULL);
                            best_yet.temp_ref_list = TempRefList(
                                    &(best_yet.inst->u.MOVE.src->head),
                                    TempRefList(
                                            &(best_yet.inst->u.MOVE.src->tail->head),
                                            TempRefList(&(best_yet.inst->u.MOVE.dst->head), NULL)));
                            best_yet.to_munch = T_ExpList(
                                    addr->u.BINOP.left,
                                    T_ExpList(addr->u.BINOP.right, T_ExpList(stm->u.MOVE.dst, NULL)));
                        }
                    }

                    if (IS_CONST(addr) && best_yet.cost < 3) {  // reg <- mem(imm)
                        best_yet.cost = 3;
                        best_yet.inst = mem_ib_to_reg(addr->u.CONST, NULL);
                        best_yet.temp_ref_list = TempRefList(&(best_yet.inst->u.MOVE.dst->head), NULL);
                        best_yet.to_munch = T_ExpList(stm->u.MOVE.dst, NULL);
                    }

                    if (best_yet.cost < 2) {  // reg <- mem(reg)
                        best_yet.cost = 2;
                        best_yet.inst = mem_rb_to_reg(NULL, NULL);
                        best_yet.temp_ref_list = TempRefList(
                                &(best_yet.inst->u.MOVE.src->head),
                                TempRefList(&(best_yet.inst->u.MOVE.dst->head), NULL));
                        best_yet.to_munch = T_ExpList(addr, T_ExpList(stm->u.MOVE.dst, NULL));
                    }
                }

                if (IS_CONST(stm->u.MOVE.src) && best_yet.cost < 2) {  // reg <- imm
                    best_yet.cost = 2;
                    best_yet.inst = imm_to_reg(stm->u.MOVE.src->u.CONST, NULL);
                    best_yet.temp_ref_list = TempRefList(&(best_yet.inst->u.MOVE.dst->head), NULL);
                    best_yet.to_munch = T_ExpList(stm->u.MOVE.dst, NULL);
                }

                if (IS_NAME(stm->u.MOVE.src) && best_yet.cost < 2) {  // reg <- label
                    best_yet.cost = 2;
                    best_yet.inst = label_to_reg(stm->u.MOVE.src->u.NAME, NULL);
                    best_yet.temp_ref_list = TempRefList(&(best_yet.inst->u.MOVE.dst->head), NULL);
                    best_yet.to_munch = T_ExpList(stm->u.MOVE.dst, NULL);
                }

                if (best_yet.cost < 1) {  // reg <- reg
                    best_yet.cost = 1;
                    best_yet.inst = reg_to_reg(NULL, NULL);
                    best_yet.temp_ref_list = TempRefList(&(best_yet.inst->u.MOVE.src->head),
                                                         TempRefList(&(best_yet.inst->u.MOVE.dst->head), NULL));
                    best_yet.to_munch = T_ExpList(stm->u.MOVE.src, T_ExpList(stm->u.MOVE.dst, NULL));
                }
            }

            AS_instrList list = NULL;
            while (best_yet.to_munch) {
                struct exp_munch exp = munchExp(frame, best_yet.to_munch->head);
                *(best_yet.temp_ref_list->head) = exp.temp;
                list = AS_splice(list, exp.list);

                best_yet.to_munch = best_yet.to_munch->tail;
                best_yet.temp_ref_list = best_yet.temp_ref_list->tail;
            }

            return AS_splice(list, AS_InstrList(best_yet.inst, NULL));
        }
        case T_EXP:
            return munchExp(frame, stm->u.EXP).list;
        default:
            assert(0);
    }
}

struct exp_munch munchExp(F_frame frame, T_exp exp) {
    switch (exp->kind) {
        case T_BINOP: {
            struct exp_munch left = munchExp(frame, exp->u.BINOP.left);
            struct exp_munch right_aka_src = munchExp(frame, exp->u.BINOP.right);

            if (exp->u.BINOP.op != T_div) {
                Temp_temp dst_temp = Temp_newtemp();  // not to mess up with original value
                AS_instr left_to_dst = reg_to_reg(left.temp, dst_temp);
                T_exp dst = T_Temp(dst_temp);
                string op = NULL;
                switch (exp->u.BINOP.op) {
                    case T_plus:
                        op = String("addl");
                        break;
                    case T_minus:
                        op = String("subl");
                        break;
                    case T_mul:
                        op = String("imul");
                        break;
                    case T_and:
                        op = String("andl");
                        break;
                    case T_or:
                        op = String("orl");
                        break;
                    case T_lshift:
                        op = String("shll");
                        break;
                    case T_rshift:
                        op = String("shrl");
                        break;
                    case T_arshift:
                        op = String("sarl");
                        break;
                    case T_xor:
                        op = String("xorl");
                        break;
                    case T_div:
                        assert(0);  // will be handled using code below
                }

                struct pattern_candidate best_yet = {.cost = -1};

                // check pattern of the right side
                if (IS_MEM(exp->u.BINOP.right)) {  // mem
                    T_exp addr = exp->u.BINOP.right->u.MEM;
                    if (IS_PLUS(addr)) {
                        if (HAS_MUL(addr)) {
                            T_exp mul = IS_MUL(addr->u.BINOP.left) ? addr->u.BINOP.left : addr->u.BINOP.right;

                            if (HAS_CONST(mul)) {
                                int scale = IS_CONST(mul->u.BINOP.left) ? mul->u.BINOP.left->u.CONST
                                                                        : mul->u.BINOP.right->u.CONST;
                                T_exp index = IS_CONST(mul->u.BINOP.left) ? mul->u.BINOP.right : mul->u.BINOP.left;

                                if (HAS_CONST(addr) && best_yet.cost < 6) {  // mem(ib + ri * s)
                                    int base = IS_CONST(addr->u.BINOP.left) ? addr->u.BINOP.left->u.CONST
                                                                            : addr->u.BINOP.right->u.CONST;

                                    best_yet.cost = 6;
                                    best_yet.inst = op_mem_ib_plus_ri_mul_scale(base, NULL, scale, NULL, op);
                                    best_yet.temp_ref_list = TempRefList(
                                            &(best_yet.inst->u.OPER.src->head),
                                            TempRefList(&(best_yet.inst->u.OPER.src->tail->head),
                                                        TempRefList(&(best_yet.inst->u.OPER.dst->head), NULL)));
                                    best_yet.to_munch = T_ExpList(index, T_ExpList(dst, T_ExpList(dst, NULL)));
                                }

                                if (best_yet.cost < 5) {  // mem(rb + ri * s)
                                    T_exp base = IS_MUL(addr->u.BINOP.left) ? addr->u.BINOP.right : addr->u.BINOP.left;

                                    best_yet.cost = 5;
                                    best_yet.inst = op_mem_rb_plus_ri_mul_scale(NULL, NULL, scale, NULL, op);
                                    best_yet.temp_ref_list = TempRefList(
                                            &(best_yet.inst->u.OPER.src->head),
                                            TempRefList(
                                                    &(best_yet.inst->u.OPER.src->tail->head),
                                                    TempRefList(&(best_yet.inst->u.OPER.src->tail->tail->head),
                                                                TempRefList(&(best_yet.inst->u.OPER.dst->head), NULL))));
                                    best_yet.to_munch = T_ExpList(base, T_ExpList(index, T_ExpList(dst, T_ExpList(dst, NULL))));
                                }
                            }
                        }

                        if (HAS_CONST(addr) && best_yet.cost < 4) {  // mem(imm + reg)
                            int displacement = IS_CONST(addr->u.BINOP.left) ? addr->u.BINOP.left->u.CONST
                                                                            : addr->u.BINOP.right->u.CONST;
                            T_exp base = IS_CONST(addr->u.BINOP.left) ? addr->u.BINOP.right : addr->u.BINOP.left;

                            best_yet.cost = 4;
                            best_yet.inst = op_mem_ib_plus_rb(displacement, NULL, NULL, op);
                            best_yet.temp_ref_list = TempRefList(
                                    &(best_yet.inst->u.OPER.src->head),
                                    TempRefList(&(best_yet.inst->u.OPER.src->tail->head),
                                                TempRefList(&(best_yet.inst->u.OPER.dst->head), NULL)));
                            best_yet.to_munch = T_ExpList(base, T_ExpList(dst, T_ExpList(dst, NULL)));
                        }

                        if (best_yet.cost < 3) {  // mem(reg + reg)
                            best_yet.cost = 3;
                            best_yet.inst = op_mem_rb_plus_ri(NULL, NULL, NULL, op);
                            best_yet.temp_ref_list = TempRefList(
                                    &(best_yet.inst->u.OPER.src->head),
                                    TempRefList(
                                            &(best_yet.inst->u.OPER.src->tail->head),
                                            TempRefList(&(best_yet.inst->u.OPER.src->tail->tail->head),
                                                        TempRefList(&(best_yet.inst->u.OPER.dst->head), NULL))));
                            best_yet.to_munch = T_ExpList(
                                    addr->u.BINOP.left, T_ExpList(
                                            addr->u.BINOP.right, T_ExpList(dst, T_ExpList(dst, NULL))));
                        }
                    }

                    if (IS_CONST(addr) && best_yet.cost < 3) {  // mem(imm)
                        best_yet.cost = 3;
                        best_yet.inst = op_mem_ib(addr->u.CONST, NULL, op);
                        best_yet.temp_ref_list = TempRefList(
                                &(best_yet.inst->u.OPER.src->head),
                                TempRefList(&(best_yet.inst->u.OPER.dst->head), NULL));
                        best_yet.to_munch = T_ExpList(dst, T_ExpList(dst, NULL));
                    }

                    if (best_yet.cost < 2) {  // mem(reg)
                        best_yet.cost = 2;
                        best_yet.inst = op_mem_rb(NULL, NULL, op);
                        best_yet.temp_ref_list = TempRefList(
                                &(best_yet.inst->u.OPER.src->head),
                                TempRefList(&(best_yet.inst->u.OPER.src->tail->head),
                                            TempRefList(&(best_yet.inst->u.OPER.dst->head), NULL)));
                        best_yet.to_munch = T_ExpList(addr, T_ExpList(dst, T_ExpList(dst, NULL)));
                    }
                }

                if (IS_CONST(exp->u.BINOP.right) && best_yet.cost < 2) {  // imm
                    best_yet.cost = 2;
                    best_yet.inst = op_imm(exp->u.BINOP.right->u.CONST, NULL, op);
                    best_yet.temp_ref_list = TempRefList(
                            &(best_yet.inst->u.OPER.src->head),
                            TempRefList(&(best_yet.inst->u.OPER.dst->head), NULL));
                    best_yet.to_munch = T_ExpList(dst, T_ExpList(dst, NULL));
                }

                if (IS_NAME(exp->u.BINOP.right) && best_yet.cost < 2) {  // label
                    best_yet.cost = 2;
                    best_yet.inst = op_label(exp->u.BINOP.right->u.NAME, NULL, op);
                    best_yet.temp_ref_list = TempRefList(
                            &(best_yet.inst->u.OPER.src->head),
                            TempRefList(&(best_yet.inst->u.OPER.dst->head), NULL));
                    best_yet.to_munch = T_ExpList(dst, T_ExpList(dst, NULL));
                }

                if (best_yet.cost < 1) {  // reg
                    best_yet.cost = 1;
                    best_yet.inst = op_reg(NULL, NULL, op);
                    best_yet.temp_ref_list = TempRefList(
                            &(best_yet.inst->u.OPER.src->head),
                            TempRefList(&(best_yet.inst->u.OPER.src->tail->head),
                                        TempRefList(&(best_yet.inst->u.OPER.dst->head), NULL)));
                    best_yet.to_munch = T_ExpList(exp->u.BINOP.right, T_ExpList(dst, T_ExpList(dst, NULL)));
                }

                AS_instrList list = NULL;
                while (best_yet.to_munch) {
                    struct exp_munch exp = munchExp(frame, best_yet.to_munch->head);
                    *(best_yet.temp_ref_list->head) = exp.temp;
                    list = AS_splice(list, exp.list);

                    best_yet.to_munch = best_yet.to_munch->tail;
                    best_yet.temp_ref_list = best_yet.temp_ref_list->tail;
                }

                list = AS_splice(left.list,  // prepare left
                                 AS_splice(list,  // prepare right
                                           AS_InstrList(left_to_dst,  // move left to temp
                                                        AS_InstrList(best_yet.inst, NULL))));
                struct exp_munch rst = {.list = list, .temp = dst_temp};
                return rst;
            } else {  // special for div
                Temp_temp temp = Temp_newtemp();

                AS_instr prepare0 = imm_to_reg(0, F_EDX());
                AS_instr prepare1 = reg_to_reg(left.temp, F_EAX());
                AS_instr div = AS_Oper(String("idivl `s0"),
                                       Temp_TempList(F_EAX(), Temp_TempList(F_EDX(), NULL)),
                                       Temp_TempList(right_aka_src.temp, NULL), NULL);
                AS_instr fetch = reg_to_reg(F_EAX(), temp);

                AS_instrList ops = AS_InstrList(
                        prepare0, AS_InstrList(prepare1, AS_InstrList(div, AS_InstrList(fetch, NULL))));
                AS_instrList list = AS_splice(left.list, AS_splice(right_aka_src.list, ops));
                struct exp_munch rst = {.list = list, .temp = temp};
                return rst;
            }
        }
        case T_MEM: {
            Temp_temp temp = Temp_newtemp();
            T_stm mem_to_temp = T_Move(T_Temp(temp), exp);

            struct exp_munch rst = {.list = munchStm(frame, mem_to_temp), .temp = temp};
            return rst;
        }
        case T_TEMP: {
            struct exp_munch rst = {.list = NULL, .temp = exp->u.TEMP};
            return rst;
        }
        case T_ESEQ: {
            printf("munchExp: T_ESEQ should be eliminated after canonicalization\n");
            assert(0);
        }
        case T_NAME: {
            printf("munchExp: T_NAME should be directly accessed by call/jmp\n");
            assert(0);
        }
        case T_CONST: {
            Temp_temp temp = Temp_newtemp();
            T_stm const_to_temp = T_Move(T_Temp(temp), exp);

            struct exp_munch rst = {.list = munchStm(frame, const_to_temp), .temp = temp};
            return rst;
        }
        case T_CALL: {
            // put args to stack
            AS_instrList prepare_args = NULL;
            AS_instrList put_args = NULL;
            int arg_num = 0;

            T_expList args = reverse(exp->u.CALL.args);
            while (args) {
                arg_num++;
                T_exp arg = args->head;

                // TODO: add `push mem`
                char inst[100];
                if (IS_TEMP(arg)) {
                    AS_instr instr = AS_Oper(String("pushl `s0"), NULL, Temp_TempList(arg->u.TEMP, NULL), NULL);
                    put_args = AS_splice(put_args, AS_InstrList(instr, NULL));
                } else if (IS_CONST(arg)) {
                    sprintf(inst, "pushl $%d", arg->u.CONST);
                    AS_instr instr = AS_Oper(String(inst), NULL, NULL, NULL);
                    put_args = AS_splice(put_args, AS_InstrList(instr, NULL));
                } else if (IS_NAME(arg)) {
                    sprintf(inst, "pushl $%s", Temp_labelstring(arg->u.NAME));
                    AS_instr instr = AS_Oper(String(inst), NULL, NULL, NULL);
                    put_args = AS_splice(put_args, AS_InstrList(instr, NULL));
                } else {
                    struct exp_munch munched = munchExp(frame, arg);
                    prepare_args = AS_splice(prepare_args, munched.list);

                    AS_instr instr = AS_Oper(String("pushl `s0"), NULL, Temp_TempList(munched.temp, NULL), NULL);
                    put_args = AS_splice(put_args, AS_InstrList(instr, NULL));
                }

                args = args->tail;
            }

            // call function
            if (exp->u.CALL.fun->kind != T_NAME) {
                printf("munchExp: fun should be T_NAME for call exp, not %d\n", exp->u.CALL.fun->kind);
                assert(0);
            }
            AS_instr call = AS_Oper(String("call `j0"),
                                    Temp_TempList(F_EAX(), Temp_TempList(F_ECX(), Temp_TempList(F_EDX(), NULL))),
                                    NULL, AS_Targets(Temp_LabelList(exp->u.CALL.fun->u.NAME, NULL)));

            // fetch result
            Temp_temp temp = Temp_newtemp();
            AS_instr fetch = AS_Move(String("movl `s0, `d0"), Temp_TempList(temp, NULL), Temp_TempList(F_RV(), NULL));

            // splice instrs together
            AS_instrList list = AS_splice(
                    prepare_args, AS_splice(
                            put_args, AS_InstrList(
                                    call, AS_InstrList(
                                            fetch, NULL))));
            if (arg_num > 0) {
                // resotre stack
                char inst[100];
                sprintf(inst, "addl $%d, %%esp", arg_num * 4);
                AS_instr incr_esp = AS_Oper(
                        String(inst), NULL, NULL, NULL);

                list = AS_splice(list, AS_InstrList(incr_esp, NULL));
            }

            struct exp_munch rst = {.list = list, .temp = temp};
            return rst;
        }
        default:
            assert(0);
    }
}


// using the “Maximal Munch” translation algorithm from IR trees to the Assem data structure.
AS_instrList F_codegen(F_frame frame, T_stmList stm_list) {
    AS_instrList instr_list = NULL;

    // move callee-saved to temps F_EBX(), F_ESI(), F_EDI()
    Temp_temp ebx_temp = Temp_newtemp();
    Temp_temp esi_temp = Temp_newtemp();
    Temp_temp edi_temp = Temp_newtemp();
    AS_instrList save_reg = AS_InstrList(reg_to_reg(F_EBX(), ebx_temp),
                                         AS_InstrList(reg_to_reg(F_ESI(), esi_temp),
                                                      AS_InstrList(reg_to_reg(F_EDI(), edi_temp), NULL)));

    // init local variable
//    F_accessList locals = frame->locals;
//    for (; locals; locals = locals->tail) {
//        F_access local = locals->head;
//        if (local->init == NULL) {  // like loop_var, which is inited at the beginning of the loop
//            continue;
//        }
//
//        if (local->kind == inFrame) {  // init to mem
//            T_stm init_to_mem = T_Move(T_Mem(T_Binop(T_plus, T_Temp(F_FP()), T_Const(local->u.offset))), local->init);
//            instr_list = AS_splice(instr_list, munchStm(frame, init_to_mem));
//        } else {  // init to reg
//            T_stm init_to_temp = T_Move(T_Temp(local->u.reg), local->init);
//            instr_list = AS_splice(instr_list, munchStm(frame, init_to_temp));
//        }
//    }

    while (stm_list) {
        T_stm stm = stm_list->head;
        instr_list = AS_splice(instr_list, munchStm(frame, stm));
        stm_list = stm_list->tail;
    }

    // move saved callee-saved from temps to reg
    AS_instrList restore_reg = AS_InstrList(reg_to_reg(ebx_temp, F_EBX()),
                                            AS_InstrList(reg_to_reg(esi_temp, F_ESI()),
                                                         AS_InstrList(reg_to_reg(edi_temp, F_EDI()), NULL)));

    // leave, i.e., movl ebp, esp; popl ebp
    AS_instr leave = AS_Oper(String("leave"), NULL, NULL, NULL);

    // ret
    // make sure %eax and other callee-saved regs are live-out, by putting those regs into src list
    AS_instr ret = AS_Oper(
            "ret", NULL,
            Temp_TempList(F_EAX(), Temp_TempList(F_EBX(), Temp_TempList(F_ESI(), Temp_TempList(F_EDI(), NULL)))), NULL);


    // put save-reg after first label
    return AS_splice(AS_InstrList(instr_list->head, AS_splice(save_reg, instr_list->tail)),
                     AS_splice(restore_reg, AS_InstrList(leave, AS_InstrList(ret, NULL))));
}
