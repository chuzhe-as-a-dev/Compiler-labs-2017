#include <stdio.h>
#include <string.h>
#include "util.h"
#include "symbol.h"
#include "absyn.h"
#include "escape.h"
#include "table.h"
#include "env.h"

static void traverseExp(S_table env, int depth, A_exp e);

static void traverseDec(S_table env, int depth, A_dec d);

static void traverseVar(S_table env, int depth, A_var v);

static void traverseExp(S_table env, int depth, A_exp e) {
    switch (e->kind) {
        case A_varExp:
            traverseVar(env, depth, e->u.var);
            break;
        case A_callExp: {
            A_expList args = e->u.call.args;
            for (; args; args = args->tail) {
                traverseExp(env, depth, args->head);
            }
            break;
        }
        case A_opExp:
            traverseExp(env, depth, e->u.op.left);
            traverseExp(env, depth, e->u.op.right);
            break;
        case A_recordExp: {
            A_efieldList fields = e->u.record.fields;
            for (; fields; fields = fields->tail) {
                traverseExp(env, depth, fields->head->exp);
            }
            break;
        }
        case A_seqExp: {
            A_expList exps = e->u.seq;
            for (; exps; exps = exps->tail) {
                traverseExp(env, depth, exps->head);
            }
            break;
        }
        case A_assignExp:
            traverseVar(env, depth, e->u.assign.var);
            traverseExp(env, depth, e->u.assign.exp);
            break;
        case A_ifExp:
            traverseExp(env, depth, e->u.iff.test);
            traverseExp(env, depth, e->u.iff.then);
            if (e->u.iff.elsee != NULL) {
                traverseExp(env, depth, e->u.iff.elsee);
            }
            break;
        case A_whileExp:
            traverseExp(env, depth, e->u.whilee.test);
            traverseExp(env, depth, e->u.whilee.body);
            break;
        case A_forExp: {
            // add new loop var
            e->u.forr.escape = FALSE;
            S_enter(env, e->u.forr.var, E_EscEntry(depth, &e->u.forr.escape));

            traverseExp(env, depth, e->u.forr.lo);
            traverseExp(env, depth, e->u.forr.hi);
            traverseExp(env, depth, e->u.forr.body);
            break;
        }
        case A_letExp: {
            A_decList decs = e->u.let.decs;
            for (; decs; decs = decs->tail) {
                traverseDec(env, depth, decs->head);
            }
            traverseExp(env, depth, e->u.let.body);
            break;
        }
        case A_arrayExp:
            traverseExp(env, depth, e->u.array.init);
            traverseExp(env, depth, e->u.array.size);
            break;
        case A_nilExp:
            break;
        case A_intExp:
            break;
        case A_stringExp:
            break;
        case A_breakExp:
            break;
    }
}

static void traverseDec(S_table env, int depth, A_dec d) {
    switch (d->kind) {
        case A_functionDec: {
            A_fundecList decs = d->u.function;
            // for each func dec
            for (; decs; decs = decs->tail) {
                A_fundec dec = decs->head;
                // enter decs of formal para
                A_fieldList formals = dec->params;
                for (; formals; formals = formals->tail) {
                    A_field formal = formals->head;
                    formal->escape = FALSE;
                    S_enter(env, formal->name, E_EscEntry(depth, &formal->escape));
                }

                // then traverse body
                traverseExp(env, depth + 1, dec->body);
            }
            break;
        }
        case A_varDec:
            d->u.var.escape = FALSE;
            S_enter(env, d->u.var.var, E_EscEntry(depth, &d->u.var.escape));
            traverseExp(env, depth, d->u.var.init);
            break;
        case A_typeDec:
            break;
    }
}

static void traverseVar(S_table env, int depth, A_var v) {
    switch (v->kind) {
        case A_simpleVar: {
            E_enventry enventry = S_look(env, v->u.simple);
            if (depth > enventry->u.esc.depth) {
                *(enventry->u.esc.escape) = TRUE;
            }
            break;
        }
        case A_fieldVar:
            traverseVar(env, depth, v->u.field.var);
            break;
        case A_subscriptVar:
            traverseVar(env, depth, v->u.subscript.var);
            traverseExp(env, depth, v->u.subscript.exp);
            break;
    }

}

void Esc_findEscape(A_exp exp) {
    // NOTE: do we need to add all dec first? to ensure every use of var has an entry in env
    S_table env = S_empty();
    traverseExp(env, 1, exp);
}
