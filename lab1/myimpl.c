#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "prog1.h"


// typedef and struct def about variable table
typedef struct Table_ *Table;
typedef struct IntAndTable_ *IntAndTable;
struct Table_ {
	string id;
	int value;
	Table tail;
};
struct IntAndTable_ {
	int value;
	Table table;
};

// function prototypes
int maxArgsInStm(A_stm stm);
int maxArgsInExp(A_exp exp);
int maxArgsInExpList(A_expList exps);
int argsInExpList(A_expList exps);
Table makeTable(string id, int value, Table tail);
IntAndTable makeIntAndTable(int value, Table tail);
int lookup(Table t, string key);
Table interpStm(A_stm s, Table t);
IntAndTable interpExp(A_exp e, Table t);
IntAndTable interpThenPrintExpList(A_expList exps, Table table);

// API for lab1
int maxargs(A_stm stm) {
	return maxArgsInStm(stm);
}

void interp(A_stm stm) {
	interpStm(stm, NULL);
}

// find maximum number of arguments in statement
int maxArgsInStm(A_stm stm) {
	switch (stm->kind) {
		case A_compoundStm: {
			int count1 = maxArgsInStm(stm->u.compound.stm1);
			int count2 = maxArgsInStm(stm->u.compound.stm2);
			return count1 > count2 ? count1 : count2;
		}
		case A_assignStm:
			return maxArgsInExp(stm->u.assign.exp);
		case A_printStm: {
			int root_count = argsInExpList(stm->u.print.exps);
			int sub_count = maxArgsInExpList(stm->u.print.exps);
			return root_count > sub_count ? root_count : sub_count;
		}
		default:
			assert(FALSE);;
	}
}

// find maximum number of arguments in expression
int maxArgsInExp(A_exp exp) {
	switch (exp->kind) {
		case A_opExp: {
			int count1 = maxArgsInExp(exp->u.op.left);
			int count2 = maxArgsInExp(exp->u.op.right);
			return count1 > count2 ? count1 : count2;
		}
		case A_eseqExp: {
			int count1 = maxArgsInStm(exp->u.eseq.stm);
			int count2 = maxArgsInExp(exp->u.eseq.exp);
			return count1 > count2 ? count1 : count2;
		}
		case A_idExp: case A_numExp:
			return 0;
		default:
			assert(FALSE);
	}
}

// find maximum number of arguments in expression list
int maxArgsInExpList(A_expList exps) {
	switch (exps->kind) {
		case A_pairExpList: {
			int count1 = maxArgsInExp(exps->u.pair.head);
			int count2 = maxArgsInExpList(exps->u.pair.tail);
			return count1 > count2 ? count1 : count2;
		}
		case A_lastExpList:
			return maxArgsInExp(exps->u.last);
		default:
			assert(FALSE);
	}
}

// find number of arguments in expression list of print statement
int argsInExpList(A_expList exps) {
	switch (exps->kind) {
		case A_pairExpList:
			return 1 + argsInExpList(exps->u.pair.tail);
		case A_lastExpList:
			return 1;
		default:
			assert(FALSE);
	}
}

// build struct IntAndTable
IntAndTable makeIntAndTable(int value, Table tail) {
	IntAndTable it = checked_malloc(sizeof(struct IntAndTable_));
	it->value = value;
	it->table = tail;
	return it;
}

// Add new variable to table
Table makeTable(string id, int value, Table tail) {
	Table t = checked_malloc(sizeof(struct Table_));
	t->id=id;
	t->value=value;
	t->tail=tail;
	return t;
}

// find
int lookup(Table table, string key) {
	if (table == NULL) {
		assert(FALSE);
	}

	if (strcmp(table->id, key) == 0) {
		return table->value;
	} else {
		return lookup(table->tail, key);
	}
}

Table interpStm(A_stm stm, Table table) {
	switch (stm->kind) {
		case A_compoundStm:
			return interpStm(stm->u.compound.stm2, interpStm(stm->u.compound.stm1, table));
		case A_assignStm: {
			IntAndTable exp_result = interpExp(stm->u.assign.exp, table);
			return makeTable(stm->u.assign.id, exp_result->value, exp_result->table);
		}
		case A_printStm:
			return interpThenPrintExpList(stm->u.print.exps, table)->table;
		default:
			assert(FALSE);
	}
}

IntAndTable interpExp(A_exp exp, Table table) {
	switch (exp->kind) {
		case A_numExp:
			return makeIntAndTable(exp->u.num, table);
		case A_idExp:
			return makeIntAndTable(lookup(table, exp->u.id), table);
		case A_opExp: {
			IntAndTable result1 = interpExp(exp->u.op.left, table);
			IntAndTable result2 = interpExp(exp->u.op.right, result1->table);
			switch (exp->u.op.oper) {
				case A_plus:
					return makeIntAndTable(result1->value + result2->value, result2->table);
				case A_minus:
					return makeIntAndTable(result1->value - result2->value, result2->table);
				case A_times:
					return makeIntAndTable(result1->value * result2->value, result2->table);
				case A_div:
					return makeIntAndTable(result1->value / result2->value, result2->table);
				default:
					assert(FALSE);
			}
		}
		case A_eseqExp:
			return interpExp(exp->u.eseq.exp, interpStm(exp->u.eseq.stm, table));
		default:
			assert(FALSE);
	}
}

IntAndTable interpThenPrintExpList(A_expList exps, Table table) {
	switch (exps->kind) {
		case A_pairExpList: {
			IntAndTable result = interpExp(exps->u.pair.head, table);
			printf("%i ", result->value);
			return interpThenPrintExpList(exps->u.pair.tail, result->table);
		}
		case A_lastExpList: {
			IntAndTable result = interpExp(exps->u.last,table);
			printf("%i\n", result->value);
			return result;
		}
		default:
			assert(FALSE);
	}
}
