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
  while (t && t->kind == Ty_name) {
    t = t->u.name.ty;
    if (t == type)
      return NULL;
  }
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
  case A_fieldVar: {
    struct expty recordType = transVar(venv, tenv, v->u.field.var);
    if (!recordType.ty || recordType.ty->kind != Ty_record) {
      EM_error(v->pos, "variable is not a record");
      return expTy(NULL, Ty_Int());
    }
    // Now let's try to find a field with the right name.
    Ty_fieldList fields = recordType.ty->u.record;
    S_symbol desiredField = v->u.field.sym;
    while (fields) {
      if (desiredField == fields->head->name)
        return expTy(NULL, fields->head->ty);
      fields = fields->tail;
    }
    EM_error(v->pos, "could not find a field with name %s",
             S_name(desiredField));
    return expTy(NULL, Ty_Int());
  }
  case A_subscriptVar: {
    struct expty arrayType = transVar(venv, tenv, v->u.subscript.var);
    if (!arrayType.ty || arrayType.ty->kind != Ty_array) {
      EM_error(v->pos, "subscript operator used on non-array type");
      return expTy(NULL, Ty_Int());
    }
    struct expty indexType = transExp(venv, tenv, v->u.subscript.exp);
    if (!indexType.ty || indexType.ty->kind != Ty_int) {
      EM_error(v->pos, "subscript operator used with non-integer index");
      return expTy(NULL, Ty_Int());
    }
    // Now evaluate to the array type.
    return expTy(NULL, arrayType.ty->u.array);
  }
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
      EM_error(a->pos, "undefined function %s", S_name(a->u.call.func));
      return expTy(NULL, Ty_Void()); // What to do here?
    }
    // Check arguments here.
    A_expList args = a->u.call.args;
    Ty_tyList desiredTypes = func->u.fun.formals;
    while (args) {
      if (!desiredTypes) {
        EM_error(a->pos, "function %s called with too many arguments",
                 S_name(a->u.call.func));
        return expTy(NULL, Ty_Void());
      }
      struct expty argType = transExp(venv, tenv, args->head);
      Ty_ty desiredType = actual_ty(desiredTypes->head);
      if (argType.ty != desiredType) {
        EM_error(a->pos, "mismatching type to function call %s",
                 S_name(a->u.call.func));
        return expTy(NULL, Ty_Void());
      }
      args = args->tail;
      desiredTypes = desiredTypes->tail;
    }
    if (desiredTypes) {
      EM_error(a->pos, "function %s called with not enough arguments",
               S_name(a->u.call.func));
      return expTy(NULL, Ty_Void());
    }
    return expTy(NULL, func->u.fun.result ? func->u.fun.result : Ty_Void());
  }
  case A_opExp: {
    A_oper oper = a->u.op.oper;
    struct expty left = transExp(venv, tenv, a->u.op.left);
    struct expty right = transExp(venv, tenv, a->u.op.right);
    // Arithmetic operators.
    switch (oper) {
    case A_plusOp:
    case A_minusOp:
    case A_timesOp:
    case A_divideOp:
    case A_eqOp:
    case A_neqOp:
    case A_ltOp:
    case A_leOp:
    case A_gtOp:
    case A_geOp:
      if (left.ty->kind != Ty_int)
        EM_error(a->u.op.left->pos, "integer required");
      if (right.ty->kind != Ty_int)
        EM_error(a->u.op.right->pos, "integer required");
      return expTy(NULL, Ty_Int());
    default:
      EM_error(a->pos, "unknown oper");
      return expTy(NULL, Ty_Int());
    }
  }
  case A_recordExp: {
    Ty_ty recordType = S_look(tenv, a->u.record.typ);
    if (!recordType) {
      EM_error(a->pos, "unrecognised record type: %s", S_name(a->u.record.typ));
      return expTy(NULL, Ty_Void());
    }
    return expTy(NULL, recordType);
  }
  case A_seqExp: {
    A_expList currentExp = a->u.seq;
    Ty_ty currentExpType = NULL;
    while (currentExp) {
      currentExpType = transExp(venv, tenv, currentExp->head).ty;
      currentExp = currentExp->tail;
    }
    return expTy(NULL, currentExpType ? currentExpType : Ty_Void());
  }
  case A_assignExp: {
    // Check that the types match up.
    struct expty lhs = transVar(venv, tenv, a->u.assign.var);
    struct expty rhs = transExp(venv, tenv, a->u.assign.exp);
    if (lhs.ty != rhs.ty)
      EM_error(a->pos, "type error in assignment");
    // Assignments don't evaluate to anything.
    return expTy(NULL, Ty_Void());
  }
  case A_ifExp: {
    struct expty condType = transExp(venv, tenv, a->u.iff.test);
    if (condType.ty->kind != Ty_int) {
      EM_error(a->pos, "if condition evaluates to non-integer value");
      return expTy(NULL, Ty_Void());
    }
    struct expty thenType = transExp(venv, tenv, a->u.iff.then);
    // If there is no else clause, then clause can't evaluate to a type.
    if (!a->u.iff.elsee) {
      if (thenType.ty->kind != Ty_void)
        EM_error(a->pos, "if expression with else block has a then block "
                         "returning non-void");
    } else {
      struct expty elseType = transExp(venv, tenv, a->u.iff.elsee);
      if (thenType.ty != elseType.ty)
        EM_error(a->pos, "if expression has then and else blocks that evaluate "
                         "to different types");
      else
        return expTy(NULL, thenType.ty);
    }
    return expTy(NULL, Ty_Void());
  }
  case A_whileExp: {
    struct expty condType = transExp(venv, tenv, a->u.whilee.test);
    if (condType.ty->kind != Ty_int) {
      EM_error(a->pos, "while condition evaluates to non-integer value");
      return expTy(NULL, Ty_Int());
    }
    struct expty bodyType = transExp(venv, tenv, a->u.whilee.body);
    if (bodyType.ty->kind != Ty_void)
      EM_error(a->pos, "while expression contains a non-void body expression");
    return expTy(NULL, Ty_Int());
  }
  case A_forExp: {
    S_beginScope(venv);
    S_enter(venv, a->u.forr.var, E_VarEntry(Ty_Int()));
    struct expty lowType = transExp(venv, tenv, a->u.forr.lo);
    if (lowType.ty->kind != Ty_int)
      EM_error(a->pos, "lower bound of for expression has non-integer type");
    struct expty highType = transExp(venv, tenv, a->u.forr.hi);
    if (highType.ty->kind != Ty_int)
      EM_error(a->pos, "upper bound of for expression has non-integer type");
    struct expty bodyType = transExp(venv, tenv, a->u.forr.body);
    if (bodyType.ty->kind != Ty_void)
      EM_error(a->pos, "body of for expression has non-void type");
    S_endScope(venv);
    return expTy(NULL, Ty_Void());
  }
  case A_breakExp: {
    // Check that we're nested within a loop here.
    return expTy(NULL, Ty_Void());
  }
  case A_letExp: {
    A_decList d;
    S_beginScope(venv);
    S_beginScope(tenv);
    for (d = a->u.let.decs; d; d = d->tail)
      transDec(venv, tenv, d->head);
    struct expty exp = transExp(venv, tenv, a->u.let.body);
    S_endScope(tenv);
    S_endScope(venv);
    return expTy(NULL, exp.ty);
  }
  case A_arrayExp: {
    Ty_ty nameType = S_look(tenv, a->u.array.typ);
    if (!nameType) {
      EM_error(a->pos, "undefined array type: %s", S_name(a->u.array.typ));
      return expTy(NULL, Ty_Void());
    }
    assert(nameType->kind == Ty_name);
    Ty_ty arrayType = actual_ty(nameType);
    assert(arrayType->kind == Ty_array);
    Ty_ty elemType = actual_ty(arrayType->u.array);
    // Check that initializer is the right type.
    struct expty initType = transExp(venv, tenv, a->u.array.init);
    if (initType.ty->kind != elemType->kind) {
      EM_error(a->pos, "init type does not match array type %s",
               S_name(a->u.array.typ));
      return expTy(NULL, nameType);
    }
    // Check that size evaluates to an integer.
    struct expty sizeType = transExp(venv, tenv, a->u.array.size);
    if (sizeType.ty->kind != Ty_int) {
      EM_error(a->pos, "array size is not an integer");
      return expTy(NULL, nameType);
    }
    return expTy(NULL, nameType);
  }
  }
}

Ty_tyList makeFormalTyList(S_table tenv, A_fieldList fieldList) {
  Ty_tyList head = NULL, current = NULL, prev = NULL;
  while (fieldList) {
    A_field currentField = fieldList->head;
    Ty_ty e = S_look(tenv, currentField->typ);
    current = Ty_TyList(actual_ty(e), NULL);
    if (!head)
      head = current;
    else
      prev->tail = current;
    fieldList = fieldList->tail;
    prev = current;
  }
  return head;
}

void transDec(S_table venv, S_table tenv, A_dec d) {
  switch (d->kind) {
  case A_varDec: {
    struct expty e = transExp(venv, tenv, d->u.var.init);
    if (d->u.var.typ) {
      Ty_ty specifiedType = S_look(tenv, d->u.var.typ);
      if (actual_ty(specifiedType) != actual_ty(e.ty))
        EM_error(d->pos, "type error in variable decl");
    }
    S_enter(venv, d->u.var.var, E_VarEntry(e.ty));
    break;
  }
  case A_typeDec: {
    // We do two passes to handle recursive types.
    // First we register the names of each type.
    A_nametyList tyDecList = d->u.type;
    while (tyDecList) {
      S_enter(tenv, tyDecList->head->name,
              Ty_Name(tyDecList->head->name, NULL));
      tyDecList = tyDecList->tail;
    }
    // Now fill in the types.
    tyDecList = d->u.type;
    while (tyDecList) {
      Ty_ty currentType = S_look(tenv, tyDecList->head->name);
      assert(currentType->kind == Ty_name);
      currentType->u.name.ty = transTy(tenv, tyDecList->head->ty);
      if (!actual_ty(currentType->u.name.ty)) {
        EM_error(d->pos, "invalid recursive type");
      }
      tyDecList = tyDecList->tail;
    }
    break;
  }
  case A_functionDec: {
    // We do two passes to handle recursive functions.
    // First we register the names of each function.
    A_fundecList funDecList = d->u.function;
    while (funDecList) {
      A_fundec f = funDecList->head;
      Ty_ty resultTy = f->result ? S_look(tenv, f->result) : NULL;
      Ty_tyList formalTys = makeFormalTyList(tenv, f->params);
      S_enter(venv, f->name, E_FunEntry(formalTys, resultTy));
      funDecList = funDecList->tail;
    }
    // Now walk the bodies.
    funDecList = d->u.function;
    while (funDecList) {
      A_fundec f = funDecList->head;
      E_enventry e = S_look(venv, f->name);
      assert(e->kind == E_funEntry);
      S_beginScope(venv);
      {
        A_fieldList l;
        Ty_tyList t;
        for (l = f->params, t = e->u.fun.formals; l; l = l->tail, t = t->tail)
          S_enter(venv, l->head->name, E_VarEntry(t->head));
      }
      struct expty actualReturn = transExp(venv, tenv, f->body);
      Ty_ty expectedReturn = f->result ? S_look(tenv, f->result) : NULL;
      if (expectedReturn) {
        if (actualReturn.ty->kind != expectedReturn->kind)
          EM_error(f->pos, "function %s returns wrong type", S_name(f->name));
      } else {
        if (actualReturn.ty->kind != Ty_void)
          EM_error(f->pos, "procedure %s returns value", S_name(f->name));
      }
      S_endScope(venv);
      funDecList = funDecList->tail;
    }
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
