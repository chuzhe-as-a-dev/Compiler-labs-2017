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
#include "flowgraph.h"
#include "liveness.h"
#include "table.h"


/*
 * use tempList as set
 */

static Temp_tempList Set_add(Temp_tempList set, Temp_temp node) {
    Temp_tempList old = set;

    bool has_node = 0;
    for (; set; set = set->tail) {
        if (set->head == node) {
            has_node = 1;
            return old;
        }
    }

    return Temp_TempList(node, old);
}

static Temp_tempList Set_remove(Temp_tempList set, Temp_temp node) {  // NOTE: will effect input list
    Temp_tempList result = set;

    Temp_tempList *p;
    for (p = &result; *p; p = &((*p)->tail)) {
        if ((*p)->head == node) {
            *p = (*p)->tail;
            break;
        }
    }

    return result;
}

static bool Set_hasNode(Temp_tempList set, Temp_temp node) {
    for (; set; set = set->tail) {
        if (set->head == node) {
            return 1;
        }
    }

    return 0;
}

static bool Set_node_hasNode(G_nodeList set, G_node node) {
    for (; set; set = set->tail) {
        if (set->head == node) {
            return 1;
        }
    }

    return 0;
}

static G_nodeList Set_node_add(G_nodeList set, G_node node) {
    G_nodeList old = set;

    bool has_node = 0;
    for (; set; set = set->tail) {
        if (set->head == node) {
            has_node = 1;
            return old;
        }
    }

    return G_NodeList(node, old);
}

static Temp_tempList Set_union(Temp_tempList s1, Temp_tempList s2) {
    Temp_tempList result = NULL;  // not to mess up with old list

    for (; s1; s1 = s1->tail) {
        result = Set_add(result, s1->head);
    }

    for (; s2; s2 = s2->tail) {
        result = Set_add(result, s2->head);
    }

    return result;
}

static Temp_tempList Set_intersection(Temp_tempList s1, Temp_tempList s2) {
    Temp_tempList result = NULL;
    for (; s2; s2 = s2->tail) {
        if (Set_hasNode(s1, s2->head)) {
            result = Set_add(result, s2->head);
        }
    }

    return result;
}


/*
 * liveness
 */
Live_moveList Live_MoveList(G_node src, G_node dst, Live_moveList tail) {
    Live_moveList lm = (Live_moveList) checked_malloc(sizeof(*lm));
    lm->src = src;
    lm->dst = dst;
    lm->tail = tail;
    return lm;
}

void printFlowNode(G_node node, G_table live_in, G_table live_out, G_table printed) {
    Temp_tempList use = FG_use(node);
    Temp_tempList def = FG_def(node);
    Temp_tempList in = G_look(live_in, node);
    Temp_tempList out = G_look(live_out, node);
    AS_instr instr = G_nodeInfo(node);

    Temp_map some_map = Temp_layerMap(F_tempMap, Temp_name());

    AS_print(stdout, instr, some_map);
    printf(" use: ");
    for (; use; use = use->tail)
        printf("%s, ", Temp_look(some_map, use->head));
    printf("\n def: ");
    for (; def; def = def->tail)
        printf("%s, ", Temp_look(some_map, def->head));
    printf("\n in: ");
    for (; in; in = in->tail)
        printf("%s, ", Temp_look(some_map, in->head));
    printf("\n out: ");
    for (; out; out = out->tail)
        printf("%s, ", Temp_look(some_map, out->head));
    printf("\n");

//    G_enter(printed, node, node);
//
//    for (G_nodeList succs = G_succ(node); succs; succs = succs->tail) {
//        G_node node = succs->head;
//        if (G_look(printed, node) == NULL) {
//            printFlowNode(node, live_in, live_out, printed);
//        }
//    }
}

void showFlowResult(G_graph flow, G_table live_in, G_table live_out) {
    printf("======== result of liveness analysis =====\n");
    G_nodeList instrs = G_nodes(flow);
    G_table printed = G_empty();

    for (; instrs; instrs = instrs->tail) {
        G_node node = instrs->head;
//        if (G_look(printed, node) == NULL) {
        printFlowNode(node, live_in, live_out, printed);
//        }
    }
}

void showInterference(G_graph interference, Live_moveList moves) {
    printf("======== interference graph =====\n");
    G_nodeList nodes = G_nodes(interference);
    for (; nodes; nodes = nodes->tail) {
        G_node node = nodes->head;
        Temp_temp temp = G_nodeInfo(node);
        if (Temp_look(F_tempMap, temp) != NULL) {  // precolored one are not interesting
            continue;
        }

        int count = 0;
        printf("%s:\n", Temp_look(Temp_name(), temp));
        printf(" conf: ");
        G_nodeList node_list;
        for (node_list = G_adj(node); node_list; node_list = node_list->tail) {
            printf("%s, ", Temp_look(Temp_layerMap(F_tempMap, Temp_name()), (Temp_temp) G_nodeInfo(node_list->head)));
            ++count;
        }
        printf("(%d)\n move: ", count);
        count = 0;
        Live_moveList move_list;
        for (move_list = moves; move_list; move_list = move_list->tail) {
            if (move_list->src == node) {
                printf("(to)%s, ", Temp_look(Temp_layerMap(F_tempMap, Temp_name()), (Temp_temp) G_nodeInfo(move_list->dst)));
                ++count;
            } else if (move_list->dst == node) {
                printf("(from)%s, ",
                       Temp_look(Temp_layerMap(F_tempMap, Temp_name()), (Temp_temp) G_nodeInfo(move_list->src)));
                ++count;
            }
        }
        printf("(%d)\n", count);
    }
}


void trace_temp(G_node flow_node, Temp_temp temp, G_table live_in, G_table live_out, G_table visited) {
//    printf("tracing temp %s at inst ", Temp_look(Temp_layerMap(F_tempMap, Temp_name()), temp));
//    AS_print(stdout, (AS_instr) G_nodeInfo(flow_node), Temp_layerMap(F_tempMap, Temp_name()));
//    printf(" before:\n");
//    printf("  live out is:\n");

    // update out set
    Temp_tempList out = (Temp_tempList) G_look(live_out, flow_node);
    if (!Set_hasNode(out, temp)) {  // if not in out yet
        G_nodeList succ = G_succ(flow_node);
        for (; succ; succ = succ->tail) {  // check all successor's in
            Temp_tempList succ_live_in = G_look(live_in, succ->head);

            if (Set_hasNode(succ_live_in, temp)) {  // if any successor includes this temp
                out = Set_add(out, temp);  // add temp to out
                G_enter(live_out, flow_node, out);  // update out

                break;
            }
        }
    }

    bool in_changed = 0;  // mark is re-propagation is needed

    // update in set, use[n] ∪ (out[n] − def[n])
    bool in_use = Set_hasNode(FG_use(flow_node), temp);
    bool in_def = Set_hasNode(FG_def(flow_node), temp);
    bool in_out = Set_hasNode(out, temp);
    if (in_use || (in_out && !in_def)) {  // now temp should be in in-set
        Temp_tempList in = G_look(live_in, flow_node);
        if (!Set_hasNode(in, temp)) {  // add when it's not in yet
            in = Set_add(in, temp);
            G_enter(live_in, flow_node, in);
            in_changed = 1;
        }
    }

    G_enter(visited, flow_node, flow_node);  // mark itself as visited

    // dfs backtrace till met temp in def
    if (!in_def ||              // usual case, not in def
        (in_def && in_use)) {   // in both def and use, like sub src, dst
        G_nodeList pred = G_pred(flow_node);
        for (; pred; pred = pred->tail) {
            if (in_changed ||                       // need to re-propogate upwards (pred's out may be changed)
                G_look(visited, pred->head) == NULL) {    // or just unvisited pred
                trace_temp(pred->head, temp, live_in, live_out, visited);
            }
        }
    }
}

Temp_temp Live_gtemp(G_node n) {
    return G_nodeInfo(n);
}

bool usableTemp(Temp_temp temp) {
    char *name = Temp_look(Temp_layerMap(F_tempMap, Temp_name()), temp);
    if (strncmp(name, "%esp", 4) == 0 || strncmp(name, "%ebp", 4) == 0) {
        return 0;
    }

    return 1;
}

struct Live_graph Live_liveness(G_graph flow) {
    // from instr-node to temp-node
    Temp_tempList unique_temps = NULL;

    G_nodeList instrs = G_nodes(flow);
    for (; instrs; instrs = instrs->tail) {  // generate set of unique temps
        AS_instr instr = G_nodeInfo(instrs->head);
        Temp_tempList srcs = instr->u.OPER.src;
        Temp_tempList dsts = instr->u.OPER.dst;

        for (; srcs; srcs = srcs->tail) {
            if (srcs->head == NULL) {
                printf("xixi");
            }
            unique_temps = Set_add(unique_temps, srcs->head);
        }

        for (; dsts; dsts = dsts->tail) {
            if (dsts->head == NULL) {
                printf("xixi");
            }
            unique_temps = Set_add(unique_temps, dsts->head);
        }
    }

    // build empty interference graph
    G_graph interference = G_Graph();
    TAB_table temp_to_node = TAB_empty();
    for (; unique_temps; unique_temps = unique_temps->tail) {
        G_node node = G_Node(interference, unique_temps->head);  // use temp as node info
        TAB_enter(temp_to_node, unique_temps->head, node);
    }

    // cal live-in, live-out
    G_table live_in = G_empty();  // G_node -> Temp_tempList
    G_table live_out = G_empty();  // G_node -> Temp_tempList

    G_nodeList temps = G_nodes(interference);
    for (; temps; temps = temps->tail) {
        instrs = G_nodes(flow);
        Temp_temp temp = G_nodeInfo(temps->head);  // temp to trace
        G_table visited = G_empty();  // mark visited instrs

        for (; instrs; instrs = instrs->tail) {
            if (Set_hasNode(FG_use(instrs->head), temp)) {  // for each temp, find use sets they are in
                trace_temp(instrs->head, temp, live_in, live_out, visited);  // backtrace till def of that temp
            }
        }
    }

    // print all in set, out set
//    showFlowResult(flow, live_in, live_out);

    // build interference edges (def -> out)
    instrs = G_nodes(flow);
    for (; instrs; instrs = instrs->tail) {
        G_node instr_node = instrs->head;

        Temp_tempList defs = FG_def(instr_node);
        for (; defs; defs = defs->tail) {
            G_node def_node = TAB_look(temp_to_node, defs->head);
            Temp_tempList outs = G_look(live_out, instr_node);

            for (; outs; outs = outs->tail) {
                G_node out_node = TAB_look(temp_to_node, outs->head);

                if ((FG_isMove(instr_node) &&                       // if is a move instr,
                     Set_hasNode(FG_use(instr_node), outs->head)) ||  // and this out is the move src
                    def_node == out_node ||                         // don't link to yourself
                    Set_node_hasNode(G_adj(def_node), out_node)) {   // don't link to some node more than once
                    continue;
                }

                G_addEdge(def_node, out_node);
            }
        }

//        if (FG_isMove(instr_node)) {  // no edge for def-use in move
//            for (; def; def = def->tail) {
//                G_node def_node = TAB_look(temp_to_node, def->head);
//                Temp_tempList out = G_look(live_out, instr_node);
//
//                for (; out; out = out->tail) {
//                    G_node out_node = TAB_look(temp_to_node, out->head);
//                    if (!Set_hasNode(FG_use(instr_node), out->head)) {  // if out is not the src of move
//                        G_addEdge(def_node, out_node);
//                    }
//                }
//            }
//        } else {
//
//        }
    }

    // build move list
    Live_moveList moves = NULL;
    instrs = G_nodes(flow);
    for (; instrs; instrs = instrs->tail) {
        if (FG_isMove(instrs->head)) {
            AS_instr instr = G_nodeInfo(instrs->head);

            // just ignore move that involving fp/sp, to avoid unwanted coalesce
            if (usableTemp(instr->u.MOVE.src->head) &&
                usableTemp(instr->u.MOVE.dst->head)) {
                G_node src = TAB_look(temp_to_node, instr->u.MOVE.src->head);
                G_node dst = TAB_look(temp_to_node, instr->u.MOVE.dst->head);

                moves = Live_MoveList(src, dst, moves);
            }
        }
    }

    // show interference result
//    showInterference(interference, moves);

    struct Live_graph lg = {.graph=interference, .moves=moves};
    return lg;
}
