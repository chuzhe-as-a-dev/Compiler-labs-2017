#include <stdio.h>
#include <string.h>
#include "util.h"
#include "symbol.h"
#include "temp.h"
#include "tree.h"
#include "absyn.h"
#include "assem.h"
#include "frame.h"
#include "graph.h"
#include "liveness.h"
#include "color.h"
#include "regalloc.h"
#include "table.h"
#include "flowgraph.h"


static bool hasTemp(Temp_tempList list, Temp_temp temp) {
    for (; list; list = list->tail)
        if (list->head == temp)
            return 1;
    return 0;
}

static void replaceTemp(Temp_tempList list, Temp_temp old, Temp_temp new_) {
    for (; list; list = list->tail)
        if (list->head == old)
            list->head = new_;
}


AS_instrList rewriteProgram(F_frame frame, AS_instrList il, Temp_tempList spills) {
    AS_instrList result = il;
    for (; spills; spills = spills->tail) {
        Temp_temp spill = spills->head;
        frame->in_frame_local_num += 1;
        frame->frame_off -= 4;
        int new_frame_off = frame->frame_off;

        AS_instrList instrs = result;//, previous = NULL;
        for (; instrs; instrs = instrs->tail) {
            AS_instr instr = instrs->head;

            if (instr->kind == I_OPER || instr->kind == I_MOVE) {
                if (hasTemp(instr->u.OPER.dst, spill)) {  // a def
                    Temp_temp temp = Temp_newtemp();
                    replaceTemp(instr->u.OPER.dst, spill, temp);  // replace with new temp

                    // create a store for the def
                    char inst[100];
                    sprintf(inst, "movl `s0, %d(%%ebp)", new_frame_off);
                    AS_instr store = AS_Oper(String(inst), NULL, Temp_TempList(temp, NULL), NULL);

                    // put the store after the instr
                    instrs->tail = AS_InstrList(store, instrs->tail);
                } else if (hasTemp(instr->u.OPER.src, spill)) {  // a use
                    Temp_temp temp = Temp_newtemp();
                    replaceTemp(instr->u.OPER.src, spill, temp);  // replace with new temp

                    // create a fetch for the use
                    char inst[100];
                    sprintf(inst, "movl %d(%%ebp), `d0", new_frame_off);
                    AS_instr fetch = AS_Oper(String(inst), Temp_TempList(temp, NULL), NULL, NULL);

                    // put the fetch before the instr (after the previous)
                    instrs->head = fetch;
                    instrs->tail = AS_InstrList(instr, instrs->tail);
//                    previous->tail = AS_InstrList(fetch, previous->tail);
                }
            }

//            previous = instrs;
        }
    }

    return result;
}


void showinfo(void *temp) {
    printf("temp %s, \n", Temp_look(Temp_name(), (Temp_temp) temp));
}


struct RA_result RA_regAlloc(F_frame f, AS_instrList il) {
    // flow graph
    G_graph flow_graph = FG_AssemFlowGraph(il, f);

    // liveness
    struct Live_graph live_graph = Live_liveness(flow_graph);

    // check inteference graph
//    G_show(stdout, G_nodes(live_graph.graph), showinfo);

    // prepare precolored and un-precolored temps
//    Temp_tempList regs = NULL;
//    G_nodeList temps = G_nodes(live_graph.graph);
//    for (; temps; temps = temps->tail) {
//        Temp_temp temp = G_nodeInfo(temps->head);
//        regs = Temp_TempList(temp, regs);
//    }

    // color and spill
    struct COL_result color = COL_color(live_graph.graph, F_tempMap, NULL, live_graph.moves);;

    // rewrite
    if (color.spills != NULL) {
        Temp_map some_map = Temp_layerMap(F_tempMap, Temp_layerMap(color.coloring, Temp_name()));
//        printf("====== before rewrite =======\n");
//        AS_printInstrList(stdout, il, some_map);
//        printf("====== spilled regs =======\n");
//        for (Temp_tempList list = color.spills; list; list = list->tail) {
//            printf("%s, ", Temp_look(some_map, list->head));
//        }
//        printf("\n");

        // rewrite program with spills
        AS_instrList new_il = rewriteProgram(f, il, color.spills);
        return RA_regAlloc(f, new_il);
    }

    // eliminate coalesced moves
    Temp_map final_result = Temp_layerMap(F_tempMap, Temp_layerMap(color.coloring, Temp_name()));
    AS_instrList *p = &il;
    while (*p) {
        AS_instr instr = (*p)->head;
        if (instr->kind == I_MOVE) {
            char *src = Temp_look(final_result, instr->u.MOVE.src->head);
            char *dst = Temp_look(final_result, instr->u.MOVE.dst->head);

            if (strncmp(src, dst, 4) == 0) {
                *p = (*p)->tail;
                continue;
            }
        }

        p = &((*p)->tail);
    }

    // eliminate useless jmp
    p = &il;
    while (*p) {
        AS_instr instr = (*p)->head;
        if (instr->kind == I_OPER && strncmp(instr->u.OPER.assem, "jmp", 3) == 0) {
            AS_instr next = (*p)->tail->head;
            if (next->kind == I_LABEL && next->u.LABEL.label == instr->u.OPER.jumps->labels->head) {
                *p = (*p)->tail;
                continue;
            }
        }

        p = &((*p)->tail);
    }

    // after assignment, add prologues
    // i.g. push ebp; mov esp, ebp; sub xx, esp;
    AS_instrList prologue = AS_InstrList(
            AS_Oper(String("pushl %ebp"), NULL, NULL, NULL),
            AS_InstrList(
                    AS_Move(String("movl %esp, %ebp"), NULL, NULL), NULL));

    if (f->in_frame_local_num != 0) {
        char inst[100];
        sprintf(inst, "subl $%d, %%esp", f->in_frame_local_num * 4);
        AS_instr decr_esp = AS_Oper(String(inst), NULL, NULL, NULL);

        prologue = AS_splice(prologue, AS_InstrList(decr_esp, NULL));
    }

    struct RA_result ret = {.coloring=color.coloring, .il=AS_splice(prologue, il)};
    return ret;
}
