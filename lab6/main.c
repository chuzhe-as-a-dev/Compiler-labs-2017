/*
 * main.c
 */

#include <stdio.h>
#include <ctype.h>
#include "util.h"
#include "symbol.h"
#include "types.h"
#include "absyn.h"
#include "errormsg.h"
#include "temp.h" /* needed by translate.h */
#include "tree.h" /* needed by frame.h */
#include "assem.h"
#include "frame.h" /* needed by translate.h and printfrags prototype */
#include "translate.h"
#include "env.h"
#include "semant.h" /* function prototype for transProg */
#include "canon.h"
#include "prabsyn.h"
#include "printtree.h"
#include "escape.h"
#include "parse.h"
#include "codegen.h"
#include "regalloc.h"
#include "graph.h"
#include "flowgraph.h"

extern bool anyErrors;

/*Lab6: complete the function doProc
 * 1. initialize the F_tempMap
 * 2. initialize the register lists (for register allocation)
 * 3. do register allocation
 * 4. output (print) the assembly code of each function
 
 * Uncommenting the following printf can help you debugging.*/


/* print the assembly language instructions to filename.s */
static void doProc(FILE *out, F_frame frame, T_stm body) {
    AS_proc proc;
    struct RA_result allocation;
    T_stmList stmList;
    AS_instrList iList;
    struct C_block blo;

    printf("\ndoProc for function %s:\n", S_name(F_name(frame)));

    printf("-------====IR tree=====-----\n");
    printStmList(stdout, T_StmList(body, NULL), Temp_layerMap(F_tempMap, Temp_name()));

    stmList = C_linearize(body);
    printf("-------====Linearlized=====-----\n");
    printStmList(stdout, stmList, Temp_layerMap(F_tempMap, Temp_name()));

    blo = C_basicBlocks(stmList);
    C_stmListList stmLists = blo.stmLists;
//    for (; stmLists; stmLists = stmLists->tail) {
//        printf("------====Basic block=====-------\n");
//        printStmList(stdout, stmLists->head, Temp_layerMap(F_tempMap, Temp_name()));
//    }

    stmList = C_traceSchedule(blo);
//    printf("-------====trace=====-----\n");
//    printStmList(stdout, stmList, Temp_layerMap(F_tempMap, Temp_name()));

    iList = F_codegen(frame, stmList); /* 9 */
    printf("----======before RA=======-----\n");
    AS_printInstrList(stdout, iList, Temp_layerMap(F_tempMap, Temp_name()));

    struct RA_result ra = RA_regAlloc(frame, iList);  /* 11 */
    printf("----======after RA=======-----\n");
    AS_printInstrList(stdout, ra.il, Temp_layerMap(F_tempMap, Temp_layerMap(ra.coloring, Temp_name())));



    fprintf(out, ".text\n");
    fprintf(out, ".global %s\n", Temp_labelstring(F_name(frame)));
    fprintf(out, ".type %s, @function\n", Temp_labelstring(F_name(frame)));
    fprintf(out, "%s:\n", Temp_labelstring(F_name(frame)));
    AS_printInstrList (out, ra.il,
                       Temp_layerMap(F_tempMap,ra.coloring));
    fprintf(out, ".size %s, .-%s\n\n", Temp_labelstring(F_name(frame)), Temp_labelstring(F_name(frame)));


//    fprintf(out, "BEGIN function\n");
//    AS_printInstrList(out, ra.il, Temp_layerMap(F_tempMap, ra.coloring));
//    fprintf(out, "END function\n");

    //Part of TA's implementation. Just for reference.
//    AS_rewrite(ra.il, Temp_layerMap(F_tempMap, ra.coloring));
//    proc =	F_procEntryExit3(frame, ra.il);

//    string procName = S_name(F_name(frame));
//    fprintf(out, ".text\n");
//    fprintf(out, ".globl %s\n", procName);
//    fprintf(out, ".type %s, @function\n", procName);
//    fprintf(out, "%s:\n", procName);


    //fprintf(stdout, "%s:\n", Temp_labelstring(F_name(frame)));
    //prologue
//    fprintf(out, "%s", proc->prolog);
//    AS_printInstrList (out, ra.il,
//                       Temp_layerMap(F_tempMap, Temp_layerMap(ra.coloring, Temp_name())));
//    fprintf(out, "%s", proc->epilog);
    //fprintf(out, "END %s\n\n", Temp_labelstring(F_name(frame)));
}

void doStr(FILE *out, Temp_label label, string str) {
    fprintf(out, ".section .rodata\n");
    fprintf(out, "%s:\n", S_name(label));

    int length = *(int *) str;
    // it may contains zeros in the middle of string. To keep this work, we need to print all the charactors instead of using fprintf(str)
//    fprintf(out, ".int %d\n", strlen(str));
    fprintf(out, ".string \"");
    int i = 0;
    for (; i < length + 4; i++,++str) {
//        fprintf(out, "%c", str[i]);
        if (*str == '\n') {
            fprintf(out, "\\n");
        } else if (*str == '\t') {
            fprintf(out, "\\t");
        } else if (*str == '\\') {
            fprintf(out, "\\\\");
        } else if (*str == '\"') {
            fprintf(out, "\\\"");
        } else if (isprint(*str)) {
            fprintf(out, "%c", *str);
        } else {
            fprintf(out, "\\%d%d%d", *str / 64 % 8, *str / 8 % 8, *str % 8);
        }
    }
//    for (; *str != 0; ++str) {
//
//    }
    fprintf(out, "\"\n");









}

int main(int argc, string *argv) {
    A_exp absyn_root;
    S_table base_env, base_tenv;
    F_fragList frags;
    char outfile[100];
    FILE *out = stdout;

    if (argc == 2) {
        absyn_root = parse(argv[1]);
        if (!absyn_root)
            return 1;

#if 0
        pr_exp(out, absyn_root, 0); /* print absyn data structure */
        fprintf(out, "\n");
#endif



        //Lab 6: escape analysis
        //If you have implemented escape analysis, uncomment this
        Esc_findEscape(absyn_root); /* set varDec's escape field */

        F_tempMap = Temp_empty();

        frags = SEM_transProg(absyn_root);
        if (anyErrors) return 1; /* don't continue */

        /* convert the filename */
        sprintf(outfile, "%s.s", argv[1]);
        out = fopen(outfile, "w");
        /* Chapter 8, 9, 10, 11 & 12 */
        for (; frags; frags = frags->tail)
            if (frags->head->kind == F_procFrag) {
                doProc(out, frags->head->u.proc.frame, frags->head->u.proc.body);
            } else if (frags->head->kind == F_stringFrag)
                doStr(out, frags->head->u.stringg.label, frags->head->u.stringg.str);

        fclose(out);
        return 0;
    }
    EM_error(0, "usage: tiger file.tig");
    return 1;
}
