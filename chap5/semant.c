#include <stddef.h>
#include "util.h"
#include "errormsg.h"
#include "symbol.h"
#include "absyn.h"
#include "types.h"
#include "env.h"
#include "semant.h"

typedef void *Tr_exp;

struct expty {
  Tr_exp exp;
  Ty_ty ty;
};

struct expty expTy(Tr_exp exp, Ty_ty ty) {
  struct expty e;
  e.exp = exp;
  e.ty = ty;
  return e;
}

struct expty transVar(S_table venv, S_table tenv, A_var v);
struct expty transExp(S_table venv, S_table tenv, A_exp a);
void transDec(S_table venv, S_table tenv, A_dec dec);
Ty_ty transTy(S_table tenv, A_ty a);

void SEM_transProg(A_exp exp) {
  S_table venv = E_base_venv(), tenv = E_base_tenv();
  transExp(venv, tenv, exp);
}

struct expty transVar(S_table venv, S_table tenv, A_var v) {
  switch (v->kind) {
  case A_simpleVar: {
    E_enventry x = S_look(venv, v->u.simple);
    if (x && x->kind == E_varEntry)
      return expTy(NULL, NULL); // actual_ty(x->u.var.ty)
    else {
      EM_error(v->pos, "undefined variable %s", S_name(v->u.simple));
      return expTy(NULL, Ty_Int());
    }
    break;
  }
  case A_fieldVar:
    break;
  case A_subscriptVar:
    break;
  }
}

struct expty transExp(S_table venv, S_table tenv, A_exp a) {
  switch (a->kind) {
  case A_varExp:
    break;
  case A_nilExp:
    break;
  case A_intExp:
    break;
  case A_stringExp:
    break;
  case A_callExp:
    break;
  case A_opExp: {
    A_oper oper = a->u.op.oper;
    struct expty left = transExp(venv, tenv, a->u.op.left);
    struct expty right = transExp(venv, tenv, a->u.op.right);
    if (oper == A_plusOp) {
      if (left.ty->kind != Ty_int)
        EM_error(a->u.op.left->pos, "integer required");
      if (right.ty->kind != Ty_int)
        EM_error(a->u.op.right->pos, "integer required");
      return expTy(NULL, Ty_Int());
    }
    break;
  }
  case A_recordExp:
    break;
  case A_seqExp:
    break;
  case A_assignExp:
    break;
  case A_ifExp:
    break;
  case A_whileExp:
    break;
  case A_forExp:
    break;
  case A_breakExp:
    break;
  case A_letExp: {
    struct expty exp;
    A_decList d;
    S_beginScope(venv);
    S_beginScope(tenv);
    for (d = a->u.let.decs; d; d = d->tail)
      transDec(venv, tenv, d->head);
    exp = transExp(venv, tenv, a->u.let.body);
    S_endScope(tenv);
    S_endScope(venv);
    break;
  }
  case A_arrayExp:
    break;
  }
}

void transDec(S_table venv, S_table tenv, A_dec d) {
  switch (d->kind) {
  case A_varDec: {
    struct expty e = transExp(venv, tenv, d->u.var.init);
    S_enter(venv, d->u.var.var, E_VarEntry(e.ty));
    break;
  }
  case A_typeDec: {
    S_enter(tenv, d->u.type->head->name, transTy(tenv, d->u.type->head->ty));
    break;
  }
  case A_functionDec: {
    // TODO: Iterate through the list.
    A_fundec f = d->u.function->head;
    Ty_ty resultTy = S_look(tenv, f->result);
    Ty_tyList formalTys = NULL; // makeFormalTyList(tenv, f->params)
    S_enter(venv, f->name, E_FunEntry(formalTys, resultTy));
    S_beginScope(venv);
    {
      A_fieldList l;
      Ty_tyList t;
      for (l = f->params, t = formalTys; l; l = l->tail, t = t->tail)
        S_enter(venv, l->head->name, E_VarEntry(t->head));
    }
    transExp(venv, tenv, f->body);
    S_endScope(venv);
    break;
  }
  }
}

Ty_ty transTy(S_table tenv, A_ty a) {}
