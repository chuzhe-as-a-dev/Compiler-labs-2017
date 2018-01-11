
/*Lab5: This header file is not complete. Please finish it with more definition.*/

#ifndef FRAME_H
#define FRAME_H

#include "tree.h"

extern const int F_wordSize;


typedef struct F_frame_ *F_frame;
typedef struct F_access_ *F_access;
typedef struct F_accessList_ *F_accessList;
typedef struct F_frag_ *F_frag;
typedef struct F_fragList_ *F_fragList;


struct F_accessList_ {
	F_access head;
	F_accessList tail;
};

struct F_frag_ {
	enum {F_stringFrag, F_procFrag} kind;
	union {
		struct {Temp_label label; string str;} stringg;
		struct {T_stm body; F_frame frame;} proc;
	} u;
};

struct F_fragList_ {F_frag head; F_fragList tail;};


Temp_label F_name(F_frame frame);

F_frame F_newFrame(Temp_label name, U_boolList formals);
F_access F_allocLocal(F_frame frame, bool escape);
F_accessList F_formals(F_frame frame);
F_accessList F_AccessList(F_access head, F_accessList tail);//TODO

F_frag F_StringFrag(Temp_label label, string str);
F_frag F_ProcFrag(T_stm body, F_frame frame);
F_fragList F_FragList(F_frag head, F_fragList tail);

Temp_temp F_FP();  // TODO
Temp_temp F_RV(void);// TODO
void F_string(Temp_label label, string str);// TODO
T_exp F_Exp(F_access acc, T_exp frame_ptr);// TODO
T_exp F_externalCall(string s, T_expList args);
T_stm F_procEntryExit1(F_frame frame, T_stm stm);// TODO the saving and restoring of callee-save registers


#endif
