#include <stddef.h>
#include <stdio.h>

#include "util.h"
#include "errormsg.h"
#include "symbol.h"
#include "absyn.h"
#include "types.h"
#include "temp.h"
#include "tree.h"
#include "frame.h"
#include "translate.h"
#include "env.h"

#include <stdbool.h>

#include "semant.h"

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

struct expty transVar(Tr_level level, S_table venv, S_table tenv, A_var v);
struct expty transExp(Tr_level level, S_table venv, S_table tenv, A_exp a);
void transDec(Tr_level level, S_table venv, S_table tenv, A_dec dec);
Ty_ty transTy(S_table tenv, A_ty a);

F_fragList SEM_transProg(A_exp exp) {
  S_table venv = E_base_venv(), tenv = E_base_tenv();
  Tr_level outerLevel = Tr_outermost();
  transExp(outerLevel, venv, tenv, exp);
  return Tr_getResult();
}

struct expty transVar(Tr_level level, S_table venv, S_table tenv, A_var v) {
  switch (v->kind) {
  case A_simpleVar: {
    E_enventry x = S_look(venv, v->u.simple);
    if (x && x->kind == E_varEntry) {
      Tr_access local = Tr_allocLocal(level, true);
      Tr_exp simpleVar = Tr_simpleVar(local, level);
      return expTy(simpleVar, actual_ty(x->u.var.ty));
    } else {
      EM_error(v->pos, "undefined variable %s", S_name(v->u.simple));
      return expTy(NULL, Ty_Int());
    }
  }
  case A_fieldVar: {
    struct expty recordType = transVar(level, venv, tenv, v->u.field.var);
    if (!recordType.ty || recordType.ty->kind != Ty_record) {
      EM_error(v->pos, "variable is not a record");
      return expTy(NULL, Ty_Int());
    }
    // Now let's try to find a field with the right name.
    Ty_fieldList fields = recordType.ty->u.record;
    S_symbol desiredField = v->u.field.sym;
    size_t fieldNum = 0;
    while (fields) {
      if (desiredField == fields->head->name) {
        Tr_exp fieldVar = Tr_fieldVar(recordType.exp, fieldNum);
        return expTy(fieldVar, fields->head->ty);
      }
      fields = fields->tail;
      ++fieldNum;
    }
    EM_error(v->pos, "could not find a field with name %s",
             S_name(desiredField));
    return expTy(NULL, Ty_Int());
  }
  case A_subscriptVar: {
    struct expty arrayType = transVar(level, venv, tenv, v->u.subscript.var);
    if (!arrayType.ty || arrayType.ty->kind != Ty_array) {
      EM_error(v->pos, "subscript operator used on non-array type");
      return expTy(NULL, Ty_Int());
    }
    struct expty indexType = transExp(level, venv, tenv, v->u.subscript.exp);
    if (!indexType.ty || indexType.ty->kind != Ty_int) {
      EM_error(v->pos, "subscript operator used with non-integer index");
      return expTy(NULL, Ty_Int());
    }
    // Now evaluate to the array type.
    Tr_exp subscriptExp = Tr_subscriptVar(arrayType.exp, indexType.exp);
    return expTy(subscriptExp, arrayType.ty->u.array);
  }
  }
}

struct expty transExp(Tr_level level, S_table venv, S_table tenv, A_exp a) {
  switch (a->kind) {
  case A_varExp: {
    return transVar(level, venv, tenv, a->u.var);
  }
  case A_nilExp: {
    return expTy(NULL, Ty_Nil());
  }
  case A_intExp: {
    return expTy(Tr_intExp(a->u.intt), Ty_Int());
  }
  case A_stringExp: {
    return expTy(Tr_stringExp(a->u.stringg), Ty_String());
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
    Tr_expList headIR = NULL, tailIR = NULL;
    while (args) {
      if (!desiredTypes) {
        EM_error(a->pos, "function %s called with too many arguments",
                 S_name(a->u.call.func));
        return expTy(NULL, Ty_Void());
      }
      struct expty argType = transExp(level, venv, tenv, args->head);
      Ty_ty desiredType = actual_ty(desiredTypes->head);
      if (argType.ty != desiredType) {
        EM_error(a->pos, "mismatching type to function call %s",
                 S_name(a->u.call.func));
        return expTy(NULL, Ty_Void());
      }
      args = args->tail;
      desiredTypes = desiredTypes->tail;

      // Add to the IR expression list.
      Tr_expList newIR = Tr_ExpList(argType.exp, NULL);
      if (headIR)
        tailIR->tail = newIR;
      else
        headIR = newIR;
      tailIR = newIR;
    }
    if (desiredTypes) {
      EM_error(a->pos, "function %s called with not enough arguments",
               S_name(a->u.call.func));
      return expTy(NULL, Ty_Void());
    }
    Tr_exp callExp = Tr_callExp(func->u.fun.label, headIR);
    return expTy(callExp, func->u.fun.result ? func->u.fun.result : Ty_Void());
  }
  case A_opExp: {
    A_oper oper = a->u.op.oper;
    struct expty left = transExp(level, venv, tenv, a->u.op.left);
    struct expty right = transExp(level, venv, tenv, a->u.op.right);
    if (left.ty->kind != Ty_int)
      EM_error(a->u.op.left->pos, "integer required");
    if (right.ty->kind != Ty_int)
      EM_error(a->u.op.right->pos, "integer required");
    // Arithmetic operators.
    switch (oper) {
    case A_plusOp:
    case A_minusOp:
    case A_timesOp:
    case A_divideOp:
      return expTy(Tr_binOpExp(oper, left.exp, right.exp), Ty_Int());
    case A_eqOp:
    case A_neqOp:
    case A_ltOp:
    case A_leOp:
    case A_gtOp:
    case A_geOp:
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
    size_t numFields = 0;
    A_efieldList fields = a->u.record.fields;
    while (fields) {
      fields = fields->tail;
      ++numFields;
    }
    Tr_exp recordExp = Tr_recordVar(numFields);
    return expTy(recordExp, recordType);
  }
  case A_seqExp: {
    A_expList currentExp = a->u.seq;
    Ty_ty currentExpType = NULL;
    while (currentExp) {
      currentExpType = transExp(level, venv, tenv, currentExp->head).ty;
      currentExp = currentExp->tail;
    }
    return expTy(NULL, currentExpType ? currentExpType : Ty_Void());
  }
  case A_assignExp: {
    // Check that the types match up.
    struct expty lhs = transVar(level, venv, tenv, a->u.assign.var);
    struct expty rhs = transExp(level, venv, tenv, a->u.assign.exp);
    if (lhs.ty != rhs.ty)
      EM_error(a->pos, "type error in assignment");
    // Assignments don't evaluate to anything.
    return expTy(NULL, Ty_Void());
  }
  case A_ifExp: {
    struct expty condType = transExp(level, venv, tenv, a->u.iff.test);
    if (condType.ty->kind != Ty_int) {
      EM_error(a->pos, "if condition evaluates to non-integer value");
      return expTy(NULL, Ty_Void());
    }
    struct expty thenType = transExp(level, venv, tenv, a->u.iff.then);
    // If there is no else clause, then clause can't evaluate to a type.
    if (!a->u.iff.elsee) {
      if (thenType.ty->kind != Ty_void)
        EM_error(a->pos, "if expression with else block has a then block "
                         "returning non-void");
    } else {
      struct expty elseType = transExp(level, venv, tenv, a->u.iff.elsee);
      if (thenType.ty != elseType.ty)
        EM_error(a->pos, "if expression has then and else blocks that evaluate "
                         "to different types");
      else
        return expTy(NULL, thenType.ty);
    }
    return expTy(NULL, Ty_Void());
  }
  case A_whileExp: {
    struct expty condType = transExp(level, venv, tenv, a->u.whilee.test);
    if (condType.ty->kind != Ty_int) {
      EM_error(a->pos, "while condition evaluates to non-integer value");
      return expTy(NULL, Ty_Int());
    }
    struct expty bodyType = transExp(level, venv, tenv, a->u.whilee.body);
    if (bodyType.ty->kind != Ty_void)
      EM_error(a->pos, "while expression contains a non-void body expression");
    return expTy(NULL, Ty_Int());
  }
  case A_forExp: {
    Tr_access local = Tr_allocLocal(level, true);
    S_beginScope(venv);
    S_enter(venv, a->u.forr.var, E_VarEntry(local, Ty_Int()));
    struct expty lowType = transExp(level, venv, tenv, a->u.forr.lo);
    if (lowType.ty->kind != Ty_int)
      EM_error(a->pos, "lower bound of for expression has non-integer type");
    struct expty highType = transExp(level, venv, tenv, a->u.forr.hi);
    if (highType.ty->kind != Ty_int)
      EM_error(a->pos, "upper bound of for expression has non-integer type");
    struct expty bodyType = transExp(level, venv, tenv, a->u.forr.body);
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
      transDec(level, venv, tenv, d->head);
    struct expty exp = transExp(level, venv, tenv, a->u.let.body);
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
    struct expty initType = transExp(level, venv, tenv, a->u.array.init);
    if (initType.ty->kind != elemType->kind) {
      EM_error(a->pos, "init type does not match array type %s",
               S_name(a->u.array.typ));
      return expTy(NULL, nameType);
    }
    // Check that size evaluates to an integer.
    struct expty sizeType = transExp(level, venv, tenv, a->u.array.size);
    if (sizeType.ty->kind != Ty_int) {
      EM_error(a->pos, "array size is not an integer");
      return expTy(NULL, nameType);
    }
    Tr_exp arrayExp = Tr_arrayVar(sizeType.exp, initType.exp);
    return expTy(arrayExp, nameType);
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

U_boolList makeFormalBoolList(A_fieldList fieldList) {
  U_boolList head = NULL, current = NULL, prev = NULL;
  while (fieldList) {
    current = U_BoolList(true, NULL);
    if (!head)
      head = current;
    else
      prev->tail = current;
    fieldList = fieldList->tail;
    prev = current;
  }
  return head;
}

void transDec(Tr_level level, S_table venv, S_table tenv, A_dec d) {
  switch (d->kind) {
  case A_varDec: {
    struct expty e = transExp(level, venv, tenv, d->u.var.init);
    if (d->u.var.typ) {
      Ty_ty specifiedType = S_look(tenv, d->u.var.typ);
      if (actual_ty(specifiedType) != actual_ty(e.ty))
        EM_error(d->pos, "type error in variable decl");
    }
    Tr_access local = Tr_allocLocal(level, true);
    S_enter(venv, d->u.var.var, E_VarEntry(local, e.ty));
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
      U_boolList formalBools = makeFormalBoolList(f->params);
      Temp_label newLabel = Temp_newlabel();
      Tr_level newLevel = Tr_newLevel(level, newLabel, formalBools);
      S_enter(venv, f->name,
              E_FunEntry(newLevel, newLabel, formalTys, resultTy));
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
        Tr_accessList a;
        for (l = f->params, t = e->u.fun.formals,
            a = Tr_formals(e->u.fun.level);
             l; l = l->tail, t = t->tail, a = a->tail)
          S_enter(venv, l->head->name, E_VarEntry(a->head, t->head));
      }
      struct expty actualReturn = transExp(e->u.fun.level, venv, tenv, f->body);
      Ty_ty expectedReturn = f->result ? S_look(tenv, f->result) : NULL;
      if (expectedReturn) {
        if (actualReturn.ty->kind != expectedReturn->kind)
          EM_error(f->pos, "function %s returns wrong type", S_name(f->name));
      } else {
        if (actualReturn.ty->kind != Ty_void)
          EM_error(f->pos, "procedure %s returns value", S_name(f->name));
      }
      // Construct and keep track of this proc frag.
      Tr_procEntryExit(e->u.fun.level, actualReturn.exp,
                       Tr_formals(e->u.fun.level));
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
