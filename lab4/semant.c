#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "util.h"
#include "errormsg.h"
#include "symbol.h"
#include "absyn.h"
#include "types.h"
#include "helper.h"
#include "env.h"
#include "semant.h"


typedef void* Tr_exp;
struct expty {
	Tr_exp exp;
	Ty_ty ty;
};

//In Lab4, the first argument exp should always be **NULL**.
struct expty expTy(Tr_exp exp, Ty_ty ty) {
	struct expty e;
	e.exp = exp;
	e.ty = ty;

	return e;
}

Ty_ty actual_ty(Ty_ty t) {
	while (t && t->kind == Ty_name) {
		t = t->u.name.ty;
	}

	return t;
}

struct expty transVar(S_table venv, S_table tenv, A_var v) {
	switch(v->kind) {
		case A_simpleVar: {
			E_enventry x = S_look(venv, v->u.simple);
			if (!x || x->kind != E_varEntry) {
				EM_error(v->pos, "undefined variable %s", S_name(v->u.simple));
				return expTy(NULL, Ty_Int());
			}

			return expTy(NULL, actual_ty(x->u.var.ty));
		}
	   	case A_fieldVar: {
			struct expty lvalue = transVar(venv, tenv, v->u.field.var);

			if (lvalue.ty->kind != Ty_record) {
				EM_error(v->pos, "not a record type");
				return expTy(NULL, Ty_Int());
			}

			// find the field of the record
			Ty_fieldList fields = lvalue.ty->u.record;
			while (fields && fields->head->name != v->u.field.sym) {
				fields = fields->tail;
			}
			if (fields == NULL) {
				EM_error(v->pos, "field %s doesn't exist", S_name(v->u.field.sym));
				return expTy(NULL, Ty_Int());
			}

			return expTy(NULL, actual_ty(fields->head->ty));
		}
		case A_subscriptVar: {
			struct expty lvalue = transVar(venv, tenv, v->u.subscript.var);
			struct expty index = transExp(venv, tenv, v->u.subscript.exp);

			if (lvalue.ty->kind != Ty_array) {
				EM_error(v->pos, "array type required");
				return expTy(NULL, Ty_Int());
			}

			if (index.ty->kind != Ty_int) {
				EM_error(v->pos, "index type is not int");
				return expTy(NULL, Ty_Int());
			}

			return expTy(NULL, actual_ty(lvalue.ty->u.array));
		}
	}
	assert(0); /* should have returned from some clause of the switch */
}

int hasLoopVar(S_table venv, A_var v) {
	switch(v->kind) {
		case A_simpleVar: {
			E_enventry x = S_look(venv, v->u.simple);
			if (x->readonly) {
				EM_error(v->pos, "loop variable can't be assigned");
				return 1;
			}
			return 0;
		}
	   	case A_fieldVar:
			return hasLoopVar(venv, v->u.field.var);
		case A_subscriptVar:
			return hasLoopVar(venv, v->u.subscript.var);
	}
	assert(0); /* should have returned from some clause of the switch */
}

struct expty transExp(S_table venv, S_table tenv, A_exp a) {
	switch(a->kind) {
		case A_varExp:
			return transVar(venv, tenv, a->u.var);
		case A_nilExp:
			return expTy(NULL, Ty_Nil());
		case A_intExp:
			return expTy(NULL, Ty_Int());
		case A_stringExp:
			return expTy(NULL, Ty_String());
		case A_callExp: {
			E_enventry x = S_look(venv, a->u.call.func);
			if (!x || x->kind != E_funEntry) {
				EM_error(a->pos, "undefined function %s", S_name(a->u.call.func));
				return expTy(NULL, Ty_Int());
			}

			// check parameter types and number
			Ty_tyList expected = x->u.fun.formals;
			A_expList actual = a->u.call.args;
			for (; actual && expected; actual = actual->tail, expected = expected->tail) {
				struct expty exp = transExp(venv, tenv, actual->head);
				if (actual_ty(exp.ty)->kind != actual_ty(expected->head)->kind) {
					EM_error(actual->head->pos, "para type mismatch");
				}
			}
			if (expected != NULL || actual != NULL) {
				EM_error(a->pos, "too many params in function %s", S_name(a->u.call.func));
			}

			return expTy(NULL, actual_ty(x->u.fun.result));
		}
		case A_opExp: {
			A_oper oper = a->u.op.oper;
			struct expty left = transExp(venv, tenv, a->u.op.left);
			struct expty right = transExp(venv, tenv, a->u.op.right);

			if (oper == A_plusOp || oper == A_minusOp || oper == A_timesOp || oper == A_divideOp) {
				if (actual_ty(left.ty)->kind != Ty_int) {
					EM_error(a->u.op.left->pos, "integer required");
					return expTy(NULL, Ty_Int());
				}

				if (actual_ty(right.ty)->kind != Ty_int) {
					EM_error(a->u.op.right->pos, "integer required");
					return expTy(NULL, Ty_Int());
				}

				return expTy(NULL, Ty_Int());
			} else {
				if (actual_ty(left.ty) != actual_ty(right.ty)) {
					EM_error(a->u.op.left->pos, "same type required");
				}
			}

			return expTy(NULL, Ty_Int());
		}
		case A_recordExp: {
			Ty_ty type = S_look(tenv, a->u.record.typ);
			type = actual_ty(type);  // REVIEW: necessary?

			if (!type) {
				EM_error(a->pos, "undefined type %s", S_name(a->u.record.typ));
				return expTy(NULL, Ty_Int());
			}

			if (type->kind !=Ty_record) {
				EM_error(a->pos, "not record type %s", S_name(a->u.record.typ));
				return expTy(NULL, type);
			}

			// check fields
			Ty_fieldList expected = type->u.record;
			A_efieldList actual = a->u.record.fields;
			for (; actual && expected; actual = actual->tail, expected = expected->tail) {
				// check name and type
				if (expected->head->name != actual->head->name) {
					EM_error(a->pos, "expected %s but get %s", S_name(expected->head->name), S_name(actual->head->name));
				}
				struct expty exp = transExp(venv, tenv, actual->head->exp);
				if (actual_ty(expected->head->ty) != actual_ty(exp.ty)) {
					EM_error(a->pos, "type not match");
				}
			}
			if (expected != NULL || actual != NULL) {
				EM_error(a->pos, "field number of %s does not match", S_name(a->u.record.typ));
			}

			return expTy(NULL, type);
		}
		case A_seqExp: {
			A_expList seq = a->u.seq;
			struct expty exp = expTy(NULL, Ty_Void());
			while (seq) {
				exp = transExp(venv, tenv, seq->head);
				seq = seq->tail;
			}

			return exp;
		}
		case A_assignExp: {
			struct expty lvalue = transVar(venv, tenv, a->u.assign.var);
			struct expty exp = transExp(venv, tenv, a->u.assign.exp);

			// not allow to assign to loop variable
			hasLoopVar(venv, a->u.assign.var);

			if (actual_ty(lvalue.ty) != actual_ty(exp.ty)) {
				EM_error(a->pos, "unmatched assign exp");
			}

			return expTy(NULL, Ty_Void());
		}
		case A_ifExp: {
			struct expty test = transExp(venv, tenv, a->u.iff.test);
			if (actual_ty(test.ty)->kind != Ty_int) {
				EM_error(a->u.iff.test->pos, "type of test expression shoulf be int");
			}

			struct expty then = transExp(venv, tenv, a->u.iff.then);
			struct expty elsee;
			if (a->u.iff.elsee) {
				elsee = transExp(venv, tenv, a->u.iff.elsee);
				if (actual_ty(then.ty) != actual_ty(elsee.ty)) {
					EM_error(a->u.iff.then->pos, "then exp and else exp type mismatch");
				}
			} else {
				elsee = expTy(NULL, Ty_Void());
				if (actual_ty(then.ty)->kind != Ty_void || actual_ty(elsee.ty)->kind != Ty_void) {
					EM_error(a->u.iff.then->pos, "if-then exp's body must produce no value");
				}
			}

			return expTy(NULL, then.ty);
		}
		case A_whileExp: {
			struct expty test = transExp(venv, tenv, a->u.whilee.test);
			struct expty body = transExp(venv, tenv, a->u.whilee.body);

			if (actual_ty(test.ty)->kind != Ty_int) {
				EM_error(a->u.whilee.test->pos, "type of test expression shoulf be int");
			}

			if (actual_ty(body.ty)->kind != Ty_void) {
				EM_error(a->u.whilee.body->pos, "while body must produce no value");
			}

			return expTy(NULL, Ty_Void());
		}
		case A_forExp: {
			struct expty start = transExp(venv, tenv, a->u.forr.lo);
			struct expty end = transExp(venv, tenv, a->u.forr.hi);

			if (actual_ty(start.ty)->kind != Ty_int) {
				EM_error(a->u.forr.lo->pos, "for exp's range type is not integer");
			}
			if (actual_ty(end.ty)->kind != Ty_int) {
				EM_error(a->u.forr.hi->pos, "for exp's range type is not integer");
			}

			S_enter(venv, a->u.forr.var, E_ROVarEntry(Ty_Int()));
			S_beginScope(venv);
			struct expty body = transExp(venv, tenv, a->u.forr.body);
			S_endScope(venv);
			if (actual_ty(body.ty)->kind != Ty_void) {
				EM_error(a->u.forr.body->pos, "type of body expression should be void");
			}

			return expTy(NULL, Ty_Void());
		}
		case A_breakExp:
			return expTy(NULL, Ty_Void());
		case A_letExp: {
			S_beginScope(venv);
			S_beginScope(tenv);
			A_decList d;
			for (d = a->u.let.decs; d; d = d->tail) {
				transDec(venv, tenv, d->head);
			}
		    struct expty exp = transExp(venv, tenv, a->u.let.body);
		    S_endScope(tenv);
		    S_endScope(venv);

			return exp;
		}
		case A_arrayExp: {
			Ty_ty type = S_look(tenv, a->u.array.typ);
			type = actual_ty(type);

			if (!type) {
				EM_error(a->pos, "undefined type %s", S_name(a->u.array.typ));
				return expTy(NULL, Ty_Int());
			}

			if (type->kind != Ty_array) {
				EM_error(a->pos, "not array type %s", S_name(a->u.record.typ));
				return expTy(NULL, type);
			}

			struct expty size = transExp(venv, tenv, a->u.array.size);
			struct expty init = transExp(venv, tenv, a->u.array.init);

			if (actual_ty(size.ty)->kind != Ty_int) {
				EM_error(a->u.array.size->pos, "type of size expression should be int");
			}
			if (actual_ty(init.ty) != actual_ty(type->u.array)) {
				EM_error(a->u.array.init->pos, "type mismatch");
			}

			return expTy(NULL, type);
		}

	}
	assert(0); /* should have returned from some clause of the switch */
}

Ty_tyList makeFormalTyList(S_table tenv, A_fieldList params) {
	if (params == NULL) {
		return NULL;
	}

	Ty_ty type = S_look(tenv, params->head->typ);
	return Ty_TyList(type, makeFormalTyList(tenv, params->tail));
}

void transDec(S_table venv, S_table tenv, A_dec d) {
	switch(d->kind) {
		case A_varDec: {
			// printf("dealing %s\n", S_name(d->u.var.var));

			struct expty init = transExp(venv, tenv, d->u.var.init);
			if (d->u.var.typ) {
				Ty_ty type = S_look(tenv, d->u.var.typ);
				if (!type) {
					EM_error(d->u.var.init->pos, "type not exist %s", S_name(d->u.var.typ));
				}

				if (actual_ty(type) != actual_ty(init.ty)) {
					EM_error(d->u.var.init->pos, "type mismatch");
				}
			} else if (actual_ty(init.ty)->kind == Ty_nil) {
				EM_error(d->u.var.init->pos, "init should not be nil without type specified");
			}
			S_enter(venv, d->u.var.var, E_VarEntry(init.ty));
			break;
		}
		case A_typeDec: {
			// put type declarations to enviroment
			A_nametyList types = d->u.type;
			while (types) {
				// printf("dealing %s header\n", S_name(types->head->name));
				if (S_look(tenv, types->head->name) != NULL) {
					EM_error(d->pos, "two types have the same name");
				} else {
					S_enter(tenv, types->head->name, Ty_Name(types->head->name, NULL));
				}

				types = types->tail;
			}

			// resolve references
			types = d->u.type;
			while (types) {
				// printf("dealing %s body\n", S_name(types->head->name));
				Ty_ty type = S_look(tenv, types->head->name);
				type->u.name.ty = transTy(tenv, types->head->ty);
				types = types->tail;
			}

			// cycle detection
			types = d->u.type;
			while (types) {
				Ty_ty init = S_look(tenv, types->head->name);
				Ty_ty type = init;
				while((type = type->u.name.ty)->kind == Ty_name) {
					// printf("checking name type %s\n", S_name(type->u.name.sym));
					if (type == init) {
						EM_error(d->pos, "illegal type cycle");
						init->u.name.ty = Ty_Int();
						break;
					}
				}

				types = types->tail;
			}

			break;
		}
		case A_functionDec: {
			// put function declarations to enviroment
			A_fundecList func = d->u.function;
			while (func) {
				// printf("dealing %s header\n", S_name(func->head->name));
				if (S_look(venv, func->head->name) != NULL) {
					EM_error(d->pos, "two functions have the same name");
					func = func->tail;
					continue;
				}

				Ty_ty resultTy;
				if (func->head->result) {
					resultTy = S_look(tenv, func->head->result);
					if (!resultTy) {
						EM_error(func->head->pos, "undefined result type %s", S_name(func->head->result));
						resultTy = Ty_Void();
					}
				} else {
					resultTy = Ty_Void();
				}

				Ty_tyList formalTys = makeFormalTyList(tenv, func->head->params);
				E_enventry entry = E_FunEntry(formalTys, resultTy);
				entry->kind = E_funEntry;
				S_enter(venv, func->head->name, entry);

				func = func->tail;
			}

			// then check function bodies
			func = d->u.function;
			while (func) {
				// printf("dealing %s body\n", S_name(func->head->name));

				Ty_tyList formalTys = makeFormalTyList(tenv,func->head->params);
				A_fieldList formalNames = func->head->params;

				S_beginScope(venv);
				for (; formalNames; formalNames = formalNames->tail, formalTys = formalTys->tail) {
					S_enter(venv, formalNames->head->name, E_VarEntry(formalTys->head));
				}
				struct expty exp = transExp(venv, tenv, func->head->body);
				E_enventry x = S_look(venv, func->head->name);
				// check return type
				if (actual_ty(x->u.fun.result)->kind == Ty_void) {  // if no value should be returned
					if (actual_ty(exp.ty)->kind != Ty_void) {  // but body returns something
						EM_error(func->head->pos, "procedure returns value");
					}
				} else if (actual_ty(x->u.fun.result)->kind != actual_ty(exp.ty)->kind) {  // when types do not match
					EM_error(func->head->pos, "procedure returns unexpected type");
				}
				S_endScope(venv);

				func = func->tail;
			}

			break;
		}
	}
}

Ty_fieldList makeFieldList(S_table tenv, A_fieldList fields) {
	Ty_ty type = S_look(tenv, fields->head->typ);
	if (!type) {
		EM_error(fields->head->pos, "undefined type %s", S_name(fields->head->typ));
		type = Ty_Int();
	}
	Ty_field field = Ty_Field(fields->head->name, type);

	if (fields->tail == NULL) {
		return Ty_FieldList(field, NULL);
	} else {
		return Ty_FieldList(field, makeFieldList(tenv, fields->tail));
	}
}

Ty_ty transTy (S_table tenv, A_ty a) {
	switch (a->kind) {
		case A_nameTy: {
			Ty_ty type = S_look(tenv, a->u.name);
			if (!type) {
				EM_error(a->pos, "undefined type %s", S_name(a->u.name));
				return Ty_Int();
			}

			return Ty_Name(a->u.name, type);
		}
		case A_recordTy:
			return Ty_Record(makeFieldList(tenv, a->u.record));
		case A_arrayTy: {
			Ty_ty type = S_look(tenv, a->u.array);
			if (!type) {
				EM_error(a->pos, "undefined type %s", S_name(a->u.array));
				return Ty_Int();
			}

			return Ty_Array(type);
		}
	}

	return NULL;
}

void SEM_transProg(A_exp exp) {
	transExp(E_base_venv(), E_base_tenv(), exp);
}
