#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "util.h"
#include "symbol.h"
#include "temp.h"
#include "tree.h"
#include "absyn.h"
#include "assem.h"
#include "frame.h"
#include "graph.h"
#include "flowgraph.h"
#include "errormsg.h"
#include "table.h"


Temp_tempList FG_def(G_node n) {
    AS_instr instr = G_nodeInfo(n);
    Temp_tempList result = NULL;
    Temp_tempList list;
    for (list = instr->u.OPER.dst; list; list = list->tail) {
        if (list->head != F_FP() && list->head != F_SP()) {
            result = Temp_TempList(list->head, result);
        }
    }

    return result;
}

Temp_tempList FG_use(G_node n) {  // filter out sp/fp, for god's sake
    AS_instr instr = G_nodeInfo(n);
    Temp_tempList result = NULL;
    Temp_tempList list;
    for (list = instr->u.OPER.src; list; list = list->tail) {
        if (list->head != F_FP() && list->head != F_SP()) {
            result = Temp_TempList(list->head, result);
        }
    }

    return result;
}

bool FG_isMove(G_node n) {
    AS_instr instr = G_nodeInfo(n);
    return instr->kind == I_MOVE;
}

// note: labels are not added to graph
G_graph FG_AssemFlowGraph(AS_instrList il, F_frame frame) {
    G_graph graph = G_Graph();
    TAB_table label_to_node = TAB_empty();

    Temp_labelList unrelated_labels = NULL;
    G_node last_node = NULL;

    // first iter: mark all fall-through, mark label-to-instr
    for (; il; il = il->tail) {
        AS_instr instr = il->head;
        if (instr->kind == I_LABEL) {  // for label, just add it to unrelated labels list
            unrelated_labels = Temp_LabelList(instr->u.LABEL.label, unrelated_labels);
            continue;
        }

        G_node node = G_Node(graph, instr);

        // add lebel mapping
        while (unrelated_labels) {
            TAB_enter(label_to_node, unrelated_labels->head, node);
            unrelated_labels = unrelated_labels->tail;
        }

        // add edge for fall-through
        if (instr->kind == I_OPER &&
            strncmp(instr->u.OPER.assem, "jmp", 3) == 0) {  // for oper, only direct jump needs special care
            if (last_node != NULL) {
                G_addEdge(last_node, node);
            }
            last_node = NULL;
        } else {  // other situations are just fall-through
            if (last_node != NULL) {
                G_addEdge(last_node, node);
            }
            last_node = node;
        }
    }

    // second iter: mark all jump
    G_nodeList nodes = G_nodes(graph);
    while (nodes) {
        G_node node = nodes->head;
        AS_instr instr = G_nodeInfo(node);
        Temp_labelList targets = NULL;

        if (instr->kind == I_OPER && instr->u.OPER.jumps != NULL) {
            targets = instr->u.OPER.jumps->labels;
        }

        while (targets) {
            G_node target_node = TAB_look(label_to_node, targets->head);
            if (target_node != NULL) {  // some function call and jmp to the ending label won't have a related instr
                G_addEdge(node, target_node);  // link to all possible instr
            }
            targets = targets->tail;
        }

        nodes = nodes->tail;
    }

    return graph;
}
