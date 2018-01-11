#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"
#include "symbol.h"
#include "temp.h"
#include "table.h"
#include "tree.h"
#include "frame.h"


// machine dependent structures and globals
static int WORDSIZE = 4;

struct F_frame_ {
    Temp_label name;
    F_accessList formals;
    F_accessList locals;
    int stack_off;
    // Temp_tempList calleesaves, callersaves, specialregs;
    // int formal_num, local_num;
};

struct F_access_ {
    enum wa {
        inFrame, inReg
    } kind;
    union {
        int offset; //inFrame
        Temp_temp reg; //inReg
    } u;
};

// constructor
F_accessList F_AccessList(F_access head, F_accessList tail) {
    F_accessList list = malloc(sizeof(*list));
    list->head = head;
    list->tail = tail;
    return list;
}

F_fragList F_FragList(F_frag head, F_fragList tail) {
    F_fragList list = malloc(sizeof(*list));
    list->head = head;
    list->tail = tail;
    return list;
}

// helper
static F_access inFrameAccess(int offset) {
    F_access access = malloc(sizeof(struct F_access_));
    access->kind = inFrame;
    access->u.offset = offset;
    return access;
}

static F_access inRegAccess(Temp_temp reg) {
    F_access access = malloc(sizeof(struct F_access_));
    access->kind = inReg;
    access->u.reg = reg;
    return access;
}

// provided functionality
F_frame F_newFrame(Temp_label name, U_boolList formals) {
    F_frame frame = malloc(sizeof(*frame));

    frame->name = name;
    frame->locals = NULL;
    frame->stack_off = 0;

    // create access of formal parameters
    F_accessList head = malloc(sizeof(struct F_accessList_));  // create a dummy head node
    F_accessList tail = head;
    int formal_off = 8;  // first argument stays at 8(%esp)
    while (formals) {
        if (formals->head != TRUE) {
            printf("Frame: formal parameter should be passed in stack.\n");
        }

        // treat all parameters as passed in stack
        F_access access = inFrameAccess(formal_off);
        formal_off += 4;

        tail->tail = F_AccessList(access, NULL);
        tail = tail->tail;

        formals = formals->tail;
    }
    frame->formals = head->tail;  // skip dummy head

    return frame;
}

F_access F_allocLocal(F_frame frame, bool escape) {
    F_access access;
    if (escape != TRUE) {
        access = inRegAccess(Temp_newtemp());
    } else {
        access = inFrameAccess(frame->stack_off);
        frame->stack_off = frame->stack_off + 4;
    }

    return access;
}

Temp_label F_name(F_frame frame) {
    return frame->name;
}

F_accessList F_formals(F_frame frame) {
    return frame->formals;
}

// TODO: identify all the FPs
Temp_temp F_FP() {
    return Temp_newtemp();
}

// TODO: identify all the RVs
Temp_temp F_RV() {
    return Temp_newtemp();
}

T_exp F_Exp(F_access acc, T_exp frame_ptr) {
    switch (acc->kind) {
        case inFrame:
            return T_Mem(T_Binop(T_plus, frame_ptr, T_Const(acc->u.offset)));
        case inReg:
            return T_Temp(acc->u.reg);
        default:
            assert(0);
    }
}

T_exp F_externalCall(string s, T_expList args) {
    return T_Call(T_Name(Temp_namedlabel(s)), args);
}

F_frag F_StringFrag(Temp_label label, string str) {
    F_frag frag = malloc(sizeof(*frag));
    frag->kind = F_stringFrag;
    frag->u.stringg.label = label;
    frag->u.stringg.str = str;

    return frag;
}

F_frag F_ProcFrag(T_stm body, F_frame frame) {
    F_frag frag = malloc(sizeof(*frag));
    frag->kind = F_procFrag;
    frag->u.proc.body = body;
    frag->u.proc.frame = frame;

    return frag;
}

// TODO
T_stm F_procEntryExit1(F_frame frame, T_stm stm) {
    return stm;
}