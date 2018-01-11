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
#include "table.h"


#define K (6)


/*
 * debug helper
 */
void showNodeList(G_nodeList list) {
    Temp_map some_map = Temp_layerMap(F_tempMap, Temp_name());
    for (; list; list = list->tail) {
        printf("%s, ", Temp_look(some_map, (Temp_temp) G_nodeInfo(list->head)));
    }
    printf("\n");
}

void showMoveList(Live_moveList list) {
    Temp_map some_map = Temp_layerMap(F_tempMap, Temp_name());
    for (; list; list = list->tail) {
        printf("%s->%s, ",
               Temp_look(some_map, (Temp_temp) G_nodeInfo(list->src)),
               Temp_look(some_map, (Temp_temp) G_nodeInfo(list->dst)));
    }
    printf("\n");
}


/*
 * data structures
 */
// mutually disjoint
G_nodeList initial;
G_nodeList precolored;
G_nodeList simplifyWorklist;
G_nodeList freezeWorklist;
G_nodeList spillWorklist;
G_nodeList spilledNodes;
G_nodeList coalescedNodes;
G_nodeList coloredNodes;
G_nodeList selectStack;

// mutually disjoint
Live_moveList worklistMoves;
Live_moveList coalescedMoves;
Live_moveList constrainedMoves;
Live_moveList frozenMoves;
Live_moveList activeMoves;

// other data structure (no adjSet, just don't want performance!!)
G_table adjList;  // G_node -> G_nodeList
G_table degree;  // G_node -> int
G_table moveList;  // G_node -> Live_moveList
G_table alias;  // G_node(coalesced) -> G_node
G_table color;  // G_node -> color(type not decided yet)

/*
 * fake set
 */
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

static G_nodeList Set_node_remove(G_nodeList set, G_node node) {  // NOTE: will effect input list, be careful!!!
    G_nodeList result = set;

    G_nodeList *p;
    for (p = &result; *p; p = &((*p)->tail)) {
        if ((*p)->head == node) {
            *p = (*p)->tail;
            break;
        }
    }

    return result;
}

static bool Set_node_hasNode(G_nodeList set, G_node node) {
    for (; set; set = set->tail) {
        if (set->head == node) {
            return 1;
        }
    }

    return 0;
}

static G_nodeList Set_node_difference(G_nodeList s1, G_nodeList s2) {  // returns s1 - s2
    G_nodeList result = NULL;  // not to mess up with old list

    for (; s1; s1 = s1->tail) {
        result = Set_node_add(result, s1->head);
    }

    for (; s2; s2 = s2->tail) {
        result = Set_node_remove(result, s2->head);
    }

    return result;
}


static G_nodeList Set_node_union(G_nodeList s1, G_nodeList s2) {
    G_nodeList result = NULL;  // not to mess up with old list

    for (; s1; s1 = s1->tail) {
        result = Set_node_add(result, s1->head);
    }

    for (; s2; s2 = s2->tail) {
        result = Set_node_add(result, s2->head);
    }

    return result;
}

static G_nodeList Set_node_intersection(G_nodeList s1, G_nodeList s2) {
    G_nodeList result = NULL;
    for (; s2; s2 = s2->tail) {
        if (Set_node_hasNode(s1, s2->head)) {
            result = Set_node_add(result, s2->head);
        }
    }

    return result;
}

static Live_moveList Set_move_add(Live_moveList set, Live_moveList node) {
    Live_moveList old = set;

    bool has_node = 0;
    for (; set; set = set->tail) {
        if ((set->src == node->src && set->dst == node->dst) ||
            (set->src == node->dst && set->dst == node->src)) {
            has_node = 1;
            return old;
        }
    }

    return Live_MoveList(node->src, node->dst, old);
}

static Live_moveList
Set_move_remove(Live_moveList set, Live_moveList node) {  // NOTE: will effect input list, be careful!!!
    Live_moveList result = set;

    Live_moveList *p;
    for (p = &result; *p; p = &((*p)->tail)) {
        if (((*p)->src == node->src && (*p)->dst == node->dst) ||
            ((*p)->src == node->dst && (*p)->dst == node->src)) {
            *p = (*p)->tail;
            break;
        }
    }

    return result;
}

static bool Set_move_hasNode(Live_moveList set, Live_moveList node) {
    for (; set; set = set->tail) {
        if ((set->src == node->src && set->dst == node->dst) ||
            (set->src == node->dst && set->dst == node->src)) {
            return 1;
        }
    }

    return 0;
}

static Live_moveList Set_move_difference(Live_moveList s1, Live_moveList s2) {  // returns s1 - s2
    Live_moveList result = NULL;  // not to mess up with old list

    for (; s1; s1 = s1->tail) {
        result = Set_move_add(result, s1);
    }

    for (; s2; s2 = s2->tail) {
        result = Set_move_remove(result, s2);
    }

    return result;
}


static Live_moveList Set_move_union(Live_moveList s1, Live_moveList s2) {
    Live_moveList result = NULL;  // not to mess up with old list

    for (; s1; s1 = s1->tail) {
        result = Set_move_add(result, s1);
    }

    for (; s2; s2 = s2->tail) {
        result = Set_move_add(result, s2);
    }

    return result;
}

static Live_moveList Set_move_intersection(Live_moveList s1, Live_moveList s2) {
    Live_moveList result = NULL;
    for (; s2; s2 = s2->tail) {
        if (Set_move_hasNode(s1, s2)) {
            result = Set_move_add(result, s2);
        }
    }

    return result;
}


/*
 * fake stack
 */
static G_nodeList Stack_push(G_nodeList stack, G_node node) {  // add to the front
    return G_NodeList(node, stack);
}

static G_nodeList Stack_pop(G_nodeList stack, G_node *node) {  // remove from the front
    if (stack == NULL) {
        printf("fake stack: why you wanna pop from empty stack? wtf dude\n");
        return NULL;
    }
    *node = stack->head;

    return stack->tail;
}


/*
 * colors
 */
typedef struct colorList_ *colorList;
struct colorList_ {
    int color;
    colorList tail;
};

colorList ColorList(int c, colorList tail) {
    colorList p = checked_malloc(sizeof(*p));
    p->color = c;
    p->tail = tail;
    return p;
}

colorList removeColor(colorList set, int c) {
    colorList result = set;

    colorList *p;
    for (p = &result; *p; p = &((*p)->tail)) {
        if ((*p)->color == c) {
            *p = (*p)->tail;
            break;
        }
    }

    return result;
}

colorList initOk() {
    colorList ok = NULL;
    int i;
    for (i = K; i > 0; --i) {
        ok = ColorList(i - 1, ok);
    }
    return ok;
}

int *int_p(int i) {
    int *ip = checked_malloc(sizeof(int));
    *ip = i;
    return ip;
}

/*
 * procedures
 */
Live_moveList nodeMoves(G_node n) {
    // returns moveList[n] ∩ (activeMoves ∪ worklistMoves)
    Live_moveList n_invloved = G_look(moveList, n);
    Live_moveList rhs = Set_move_union(activeMoves, worklistMoves);
    return Set_move_intersection(n_invloved, rhs);
}

bool moveRelated(G_node n) {
    return nodeMoves(n) != NULL;
}

void makeWorklist() {
    for (; initial; initial = initial->tail) {
        G_node n = initial->head;
        if (G_degree(n) >= K) {
            spillWorklist = Set_node_add(spillWorklist, n);
        } else if (moveRelated(n)) {
            freezeWorklist = Set_node_add(freezeWorklist, n);
        } else {
            simplifyWorklist = Set_node_add(simplifyWorklist, n);
        }
    }
}

G_nodeList adjacent(G_node n) {
    // returns adjList[n] \ (selectStack ∪ coalescedNodes)
    G_nodeList result = G_look(adjList, n);
    result = Set_node_difference(result, selectStack);
    result = Set_node_difference(result, coalescedNodes);
    return result;
}

void enableMoves(G_nodeList nodes) {
    for (; nodes; nodes = nodes->tail) {
        G_node n = nodes->head;
        Live_moveList moves = nodeMoves(n);
        for (; moves; moves = moves->tail) {
            Live_moveList m = moves;
            if (Set_move_hasNode(activeMoves, m)) {
                activeMoves = Set_move_remove(activeMoves, m);
                worklistMoves = Set_move_add(worklistMoves, m);
            }
        }
    }
}

void decrementDegree(G_node m) {
    int d = *(int *) G_look(degree, m);
    G_enter(degree, m, (void *) int_p(d - 1));

    if (d == K) {
        enableMoves(G_NodeList(m, adjacent(m)));  // {m} ∪ Adjacent(m)
        spillWorklist = Set_node_remove(spillWorklist, m);
        if (!Set_node_hasNode(precolored, m)) {
            if (moveRelated(m)) {
                freezeWorklist = Set_node_add(freezeWorklist, m);
            } else {
                simplifyWorklist = Set_node_add(simplifyWorklist, m);
            }
        }
    }
}

G_node getAlias(G_node n) {
    if (Set_node_hasNode(coalescedNodes, n)) {
        G_node a = G_look(alias, n);
        return getAlias(a);
    } else {
        return n;
    }
}

void addWorkList(G_node u) {
    if (!Set_node_hasNode(precolored, u) &&
        !moveRelated(u) &&
        *(int *) G_look(degree, u) < K) {

        freezeWorklist = Set_node_remove(freezeWorklist, u);
        simplifyWorklist = Set_node_add(simplifyWorklist, u);
    }
}

bool ok(G_node t, G_node r) {
    // returns (degree[t] < K) ∨ (t ∈ precolored) ∨ ((t,r) ∈ adjSet)
    bool temp0 = *(int *) G_look(degree, t) < K;
    bool temp1 = Set_node_hasNode(precolored, t);
    G_nodeList adj_of_t = G_look(adjList, t);
    bool temp2 = Set_node_hasNode(adj_of_t, r);

    return temp0 || temp1 || temp2;
}

bool conservative(G_nodeList nodes) {
    int k = 0;
    for (; nodes; nodes = nodes->tail) {
        G_node n = nodes->head;
        if (*(int *) G_look(degree, n) >= K) {
            k = k + 1;
        }
    }

    return k < K;
}

void addEdge(G_node u, G_node v) {
    G_nodeList adj_of_u = G_look(adjList, u);
    if (u != v && !Set_node_hasNode(adj_of_u, v)) {
        // update adj of u
        adj_of_u = Set_node_add(adj_of_u, v);
        G_enter(adjList, u, adj_of_u);

        // update adj of v
        G_nodeList adj_of_v = G_look(adjList, v);
        adj_of_v = Set_node_add(adj_of_v, u);
        G_enter(adjList, v, adj_of_v);

        // update both degree
        G_enter(degree, u, (void *) int_p(*(int *) G_look(degree, u) + 1));
        G_enter(degree, v, (void *) int_p(*(int *) G_look(degree, v) + 1));
    }
}

void combine(G_node u, G_node v) {
    if (Set_node_hasNode(freezeWorklist, v)) {
        freezeWorklist = Set_node_remove(freezeWorklist, v);
    } else {
        spillWorklist = Set_node_remove(spillWorklist, v);
    }

    coalescedNodes = Set_node_add(coalescedNodes, v);
    G_enter(alias, v, u);

    Live_moveList temp = Set_move_union(G_look(moveList, u), G_look(moveList, v));
    G_enter(moveList, u, temp);

    G_nodeList adj = adjacent(v);
    for (; adj; adj = adj->tail) {
        G_node t = adj->head;
        addEdge(t, u);
        decrementDegree(t);
    }

    // degree[u] ≥ K ∧ u ∈ freezeWorkList, and not precolored
    if (*(int *) G_look(degree, u) >= K && Set_node_hasNode(freezeWorklist, u) && !Set_node_hasNode(precolored, u)) {
        freezeWorklist = Set_node_remove(freezeWorklist, u);
        spillWorklist = Set_node_add(spillWorklist, u);
    }
}

void simplify() {
    G_node n = simplifyWorklist->head;
    simplifyWorklist = simplifyWorklist->tail;

    selectStack = Stack_push(selectStack, n);

    G_nodeList adj = adjacent(n);
    for (; adj; adj = adj->tail) {
        decrementDegree(adj->head);
    }
}

void coalesce() {
    Live_moveList m = worklistMoves;  // m = dst <- src
    worklistMoves = worklistMoves->tail;

    G_node x = getAlias(m->src);
    G_node y = getAlias(m->dst);

    G_node u, v;
    if (Set_node_hasNode(precolored, y)) {
        u = y;
        v = x;
    } else {
        u = x;
        v = y;
    }

    if (u == v) {
        coalescedMoves = Set_move_add(coalescedMoves, m);
        addWorkList(u);
        return;
    }

    if (Set_node_hasNode(precolored, v) || Set_node_hasNode((G_nodeList) G_look(adjList, u), v)) {
        // both are precolored, or is conflict
        constrainedMoves = Set_move_add(constrainedMoves, m);
        addWorkList(u);
        addWorkList(v);
        return;
    }

    if (Set_node_hasNode(precolored, u)) {
        bool all_ok = 1;
        G_nodeList adj = adjacent(v);
        for (; adj; adj = adj->tail) {
            G_node t = adj->head;
            if (!ok(t, u)) {
                all_ok = 0;
                break;
            }
        }

        if (all_ok) {  // if (∀t ∈ Adjacent(v), OK(t, u)
            coalescedMoves = Set_move_add(coalescedMoves, m);
            combine(u, v);
            addWorkList(u);
            return;
        }
    } else {
        G_nodeList temp = Set_node_union(adjacent(u), adjacent(v));
        if (conservative(temp)) {  // if Conservative(Adjacent(u) ∪ Adjacent(v))
            coalescedMoves = Set_move_add(coalescedMoves, m);
            combine(u, v);
            addWorkList(u);
            return;
        }
    }

    activeMoves = Set_move_add(activeMoves, m);
}

void freezeMoves(G_node u) {
    Live_moveList moves = nodeMoves(u);
    for (; moves; moves = moves->tail) {
        Live_moveList m = moves;

        G_node x = m->src, y = m->dst;
        G_node v;
        if (getAlias(y) == getAlias(u)) {   // assign v to be the other node (not u)
            v = getAlias(x);
        } else {
            v = getAlias(y);
        }

        activeMoves = Set_move_remove(activeMoves, m);
        frozenMoves = Set_move_add(frozenMoves, m);

        if (nodeMoves(v) == NULL && *(int *) G_look(degree, v) < K && !Set_node_hasNode(precolored, v)) {
            freezeWorklist = Set_node_remove(freezeWorklist, v);
            simplifyWorklist = Set_node_add(simplifyWorklist, v);
        }
    }
}

void freeze() {
    G_node u = freezeWorklist->head;
    freezeWorklist = freezeWorklist->tail;
    simplifyWorklist = Set_node_add(simplifyWorklist, u);
    freezeMoves(u);
}

void selectSpill() {
    // TODO: more heuristic selecting?

    // spill the one with highest degrees
    G_nodeList potentials = spillWorklist;
    G_node m = NULL;
    int highest_degree = -1;
    for (; potentials; potentials = potentials->tail) {
        int d = *(int *) G_look(degree, potentials->head);
        if (d > highest_degree) {
            highest_degree = d;
            m = potentials->head;
        }
    }

    spillWorklist = Set_node_remove(spillWorklist, m);
    simplifyWorklist = Set_node_add(simplifyWorklist, m);
    freezeMoves(m);
}

void assignColors() {
    while (selectStack != NULL) {
        G_node n;
        selectStack = Stack_pop(selectStack, &n);

//        printf("======== before assign color to %s =======\n",
//               Temp_look(Temp_layerMap(F_tempMap, Temp_name()), (Temp_temp) G_nodeInfo(n)));
//        printf("spilledNodes: ");
//        showNodeList(spilledNodes);
//        printf("coloredNodes: ");
//        showNodeList(coloredNodes);
//        printf("selectStack: ");
//        showNodeList(selectStack);

        colorList okColors = initOk();

        G_nodeList adj = G_look(adjList, n);
//        printf("colors of neighbors: ");
        for (; adj; adj = adj->tail) {
            G_node w = adj->head;
            G_nodeList temp = Set_node_union(coloredNodes, precolored);
            if (Set_node_hasNode(temp, getAlias(w))) {  // if GetAlias(w) ∈ (coloredNodes ∪ precolored)
                int color_of_w = *(int *) G_look(color, getAlias(w));
                okColors = removeColor(okColors, color_of_w);

//                printf("%d(%s, i.g., %s), ",
//                       color_of_w,
//                       Temp_look(Temp_layerMap(F_tempMap, Temp_name()), (Temp_temp) G_nodeInfo(w)),
//                       Temp_look(Temp_layerMap(F_tempMap, Temp_name()), (Temp_temp) G_nodeInfo(getAlias(w))));
            }
        }
//        printf("\n");

        if (okColors == NULL) {
//            printf("so this node spilled\n");
            spilledNodes = Set_node_add(spilledNodes, n);
        } else {
            coloredNodes = Set_node_add(coloredNodes, n);
            int c = okColors->color;
            G_enter(color, n, (void *) int_p(c));
//            printf("so color picked is %d\n", c);
        }
    }

    G_nodeList nodes = coalescedNodes;
    for (; nodes; nodes = nodes->tail) {
        G_node n = nodes->head;
        int c = *(int *) G_look(color, getAlias(n));
        G_enter(color, n, (void *) int_p(c));
    }
}

struct COL_result
COL_color(G_graph interference, Temp_map precolor_map, Temp_tempList all_temps, Live_moveList move_list) {
    /*
     * reset data structure
     */
    initial = NULL;
    precolored = NULL;
    simplifyWorklist = NULL;
    freezeWorklist = NULL;
    spillWorklist = NULL;
    spilledNodes = NULL;
    coalescedNodes = NULL;
    coloredNodes = NULL;
    selectStack = NULL;

    // mutually disjoint along with worklistMoves
    worklistMoves = NULL;
    coalescedMoves = NULL;
    constrainedMoves = NULL;
    frozenMoves = NULL;
    activeMoves = NULL;

    // other data structure
    // adjSet?
    adjList = G_empty();  // G_node -> G_nodeList
    degree = G_empty();  // G_node -> int
    moveList = G_empty();  // G_node -> Live_moveList
    alias = G_empty();  // G_node(coalesced) -> G_node
    color = G_empty();  // G_node -> color(type not decided yet)


    /*
     * preparation
     */
    // build initial, precolored, color
    G_nodeList nodes = G_nodes(interference);
    for (; nodes; nodes = nodes->tail) {
        G_node node = nodes->head;
        Temp_temp temp = G_nodeInfo(node);

        char *reg = Temp_look(precolor_map, temp);
        if (reg == NULL) {  // not precolored
            initial = G_NodeList(node, initial);
        } else {
            precolored = G_NodeList(node, precolored);

            if (strncmp(reg, "%eax", 4) == 0) {
                G_enter(color, node, (void *) int_p(0));
            } else if (strncmp(reg, "%ecx", 4) == 0) {
                G_enter(color, node, (void *) int_p(1));
            } else if (strncmp(reg, "%edx", 4) == 0) {
                G_enter(color, node, (void *) int_p(2));
            } else if (strncmp(reg, "%ebx", 4) == 0) {
                G_enter(color, node, (void *) int_p(3));
            } else if (strncmp(reg, "%esi", 4) == 0) {
                G_enter(color, node, (void *) int_p(4));
            } else if (strncmp(reg, "%edi", 4) == 0) {
                G_enter(color, node, (void *) int_p(5));
            } else if (strncmp(reg, "%esp", 4) == 0) {
                G_enter(color, node, (void *) int_p(6));
            } else if (strncmp(reg, "%ebp", 4) == 0) {
                G_enter(color, node, (void *) int_p(7));
            } else {
                printf("color: what fucking reg do you want? %s? wtf\n", reg);
                assert(0);
            }
        }
    }

    // build adjList, degree
    nodes = G_nodes(interference);
    for (; nodes; nodes = nodes->tail) {
        G_enter(adjList, nodes->head, G_adj(nodes->head));
        G_enter(degree, nodes->head, (void *) int_p(G_degree(nodes->head)));
    }

    // build worklistMoves (unique)
    for (; move_list; move_list = move_list->tail) {
        worklistMoves = Set_move_add(worklistMoves, move_list);
    }

    // build moveList
    Live_moveList moves = worklistMoves;
    for (; moves; moves = moves->tail) {
        Live_moveList old = G_look(moveList, moves->src);
        G_enter(moveList, moves->src, Set_move_add(old, moves));
        old = G_look(moveList, moves->dst);
        G_enter(moveList, moves->dst, Set_move_add(old, moves));
    }

    // initialize work-lists
    makeWorklist();

    /*
     * coloring
     */
    // simplify, coalesce, freeze, spill loop
    do {
//        printf("======== before this round of coloring =======\n");
//        printf("simplifyWorklist: ");
//        showNodeList(simplifyWorklist);
//        printf("worklistMoves: ");
//        showMoveList(worklistMoves);
//        printf("freezeWorklist: ");
//        showNodeList(freezeWorklist);
//        printf("spillWorklist: ");
//        showNodeList(spillWorklist);
//
//        printf("coalescedNodes: ");
//        showNodeList(coalescedNodes);
//        printf("selectStack: ");
//        showNodeList(selectStack);
//        printf("coalescedMoves: ");
//        showMoveList(coalescedMoves);
//        printf("constrainedMoves: ");
//        showMoveList(constrainedMoves);
//        printf("frozenMoves: ");
//        showMoveList(frozenMoves);
//        printf("activeMoves: ");
//        showMoveList(activeMoves);

        if (simplifyWorklist != NULL) {
            simplify();
        } else if (worklistMoves != NULL) {
            coalesce();
        } else if (freezeWorklist != NULL) {
            freeze();
        } else if (spillWorklist != NULL) {
            selectSpill();
        }
    } while (simplifyWorklist != NULL || worklistMoves != NULL ||
             freezeWorklist != NULL || spillWorklist != NULL);

    // assign colors
    assignColors();


    /*
     * prepare result
     */
    // from G_table to Temp_map
    Temp_map result_map = Temp_empty();
    nodes = Set_node_union(coloredNodes, coalescedNodes);
    for (; nodes; nodes = nodes->tail) {
        G_node node = nodes->head;
        Temp_temp temp = G_nodeInfo(node);
        int c = *(int *) G_look(color, node);
        switch (c) {
            case 0:
                Temp_enter(result_map, temp, String("%eax"));
                break;
            case 1:
                Temp_enter(result_map, temp, String("%ecx"));
                break;
            case 2:
                Temp_enter(result_map, temp, String("%edx"));
                break;
            case 3:
                Temp_enter(result_map, temp, String("%ebx"));
                break;
            case 4:
                Temp_enter(result_map, temp, String("%esi"));
                break;
            case 5:
                Temp_enter(result_map, temp, String("%edi"));
                break;
            case 6:
                Temp_enter(result_map, temp, String("%esp"));
                break;
            case 7:
                Temp_enter(result_map, temp, String("%ebp"));
                break;
            default:
                printf("color: why %d is assigned?\n", c);
                assert(0);
        }
    }

    // from G_nodeList to Temp_tempList
    Temp_tempList actual_spills = NULL;
    nodes = spilledNodes;
    for (; nodes; nodes = nodes->tail) {
        Temp_temp temp = G_nodeInfo(nodes->head);
        actual_spills = Temp_TempList(temp, actual_spills);
    }

    struct COL_result ret = {.coloring=result_map, .spills=actual_spills};
    return ret;
}
