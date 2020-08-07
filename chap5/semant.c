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

Ty_ty actual_ty(Ty_ty type) {
  Ty_ty t = type;
  while (t && t->kind == Ty_name)
    t = t->u.name.ty;
  assert(t);
  return t;
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
      return expTy(NULL, actual_ty(x->u.var.ty));
    else {
      EM_error(v->pos, "undefined variable %s", S_name(v->u.simple));
      return expTy(NULL, Ty_Int());
    }
  }
  case A_fieldVar:
    break;
  case A_subscriptVar:
    break;
  }
}

struct expty transExp(S_table venv, S_table tenv, A_exp a) {
  switch (a->kind) {
  case A_varExp: {
    return transVar(venv, tenv, a->u.var);
  }
  case A_nilExp: {
    return expTy(NULL, Ty_Nil());
  }
  case A_intExp: {
    return expTy(NULL, Ty_Int());
  }
  case A_stringExp: {
    return expTy(NULL, Ty_String());
  }
  case A_callExp: {
    E_enventry func = S_look(venv, a->u.call.func);
    if (!func || func->kind != E_funEntry) {
      EM_error(a->pos, "undefined function %s", a->u.call.func);
      return expTy(NULL, Ty_Void()); // What to do here?
    }
    // Check arguments here.
    A_expList args = a->u.call.args;
    Ty_tyList desiredTypes = func->u.fun.formals;
    while (args) {
      struct expty argType = transExp(venv, tenv, args->head);
      Ty_ty desiredType = actual_ty(desiredTypes->head);
      if (argType.ty != desiredType) {
        EM_error(a->pos, "mismatching type to function call");
        return expTy(NULL, Ty_Void());
      }
      args = args->tail;
      desiredTypes = desiredTypes->tail;
    }
    return expTy(NULL, func->u.fun.result);
  }
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
  case A_recordExp: {
    Ty_ty recordType = S_look(tenv, a->u.record.typ);
    if (!recordType) {
      EM_error(a->pos, "unrecognised record type: %s", a->u.record.typ);
      return expTy(NULL, Ty_Void());
    }
    return expTy(NULL, recordType);
  }
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
  case A_breakExp: {
    // Check that we're nested within a loop here.
    return expTy(NULL, Ty_Void());
  }
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
  case A_arrayExp: {
    Ty_ty arrayType = S_look(tenv, a->u.array.typ);
    if (!arrayType) {
      EM_error(a->pos, "undefined array type: %s", a->u.array.typ);
      return expTy(NULL, Ty_Void());
    }
    // Check that initializer is the right type.
    struct expty initType = transExp(venv, tenv, a->u.array.init);
    if (initType.ty != arrayType) {
      EM_error(a->pos, "init type does not match array type");
      return expTy(NULL, Ty_Array(arrayType));
    }
    // Check that size evaluates to an integer.
    struct expty sizeType = transExp(venv, tenv, a->u.array.size);
    if (sizeType.ty != Ty_Int()) {
      EM_error(a->pos, "array size is not an integer");
      return expTy(NULL, Ty_Array(arrayType));
    }
    return expTy(NULL, Ty_Array(arrayType));
  }
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

Ty_fieldList transFieldList(S_table tenv, A_pos pos, A_fieldList fieldList) {
  Ty_fieldList head = NULL, current = NULL, prev = NULL;
  while (fieldList) {
    Ty_ty currentType = S_look(tenv, fieldList->head->typ);
    if (!currentType) {
      EM_error(pos, "field list references unrecognised type");
      currentType = Ty_Void();
    }
    current = Ty_FieldList(Ty_Field(fieldList->head->name, currentType), NULL);
    if (!head)
      head = current;
    else
      prev->tail = current;
    fieldList = fieldList->tail;
    prev = current;
  }
  return head;
}

Ty_ty transTy(S_table tenv, A_ty a) {
  switch (a->kind) {
  case A_nameTy: {
    Ty_ty underlyingType = S_look(tenv, a->u.name);
    return Ty_Name(a->u.name, underlyingType);
  }
  case A_recordTy: {
    Ty_fieldList fieldList = transFieldList(tenv, a->pos, a->u.record);
    return Ty_Record(fieldList);
  }
  case A_arrayTy: {
    Ty_ty elementType = S_look(tenv, a->u.array);
    return Ty_Array(elementType);
  }
  }
}
