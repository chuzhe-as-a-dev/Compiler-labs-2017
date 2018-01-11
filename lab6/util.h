#ifndef UTIL_H
#define UTIL_H
#include <assert.h>

typedef char *string;
typedef char bool;

#define TRUE 1
#define FALSE 0

void *checked_malloc(int);
string String(char *);

typedef struct U_boolList_ *U_boolList;
struct U_boolList_ {bool head; U_boolList tail;};
U_boolList U_BoolList(bool head, U_boolList tail);

/*
 * Some fake set
 */

//typedef void *elm_t;
//typedef struct elmList_ *set_t;
//struct elmList_ {
//    elm_t elm;
//    set_t tail;
//};
//
//set_t Set_add(set_t set, elm_t node);
//set_t Set_remove(set_t set, elm_t node);
//bool Set_hasNode(set_t set, elm_t node);
//set_t Set_union(set_t s1, set_t s2);
//set_t Set_intersection(set_t s1, set_t s2);

#endif
