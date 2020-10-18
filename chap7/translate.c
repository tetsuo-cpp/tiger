#include <stdio.h>

#include "util.h"
#include "symbol.h"
#include "absyn.h"
#include "temp.h"
#include "tree.h"
#include "frame.h"

#include "translate.h"

struct Tr_access_ {
  Tr_level level;
  F_access access;
};

struct Tr_level_ {
  Tr_level parent;
  Temp_label name;
  F_frame frame;
  Tr_accessList formals;
};

Tr_access Tr_Access(Tr_level level, F_access fAccess) {
  Tr_access access = checked_malloc(sizeof(struct Tr_access_));
  access->level = level;
  access->access = fAccess;
  return access;
}

Tr_accessList Tr_AccessList(Tr_access head, Tr_accessList tail) {
  Tr_accessList list = checked_malloc(sizeof(struct Tr_accessList_));
  list->head = head;
  list->tail = tail;
  return list;
}

Tr_expList Tr_ExpList(Tr_exp head, Tr_expList tail) {
  Tr_expList list = checked_malloc(sizeof(*list));
  list->head = head;
  list->tail = tail;
  return list;
}

Tr_level Tr_Level(Tr_level parent, Temp_label name, F_frame frame,
                  Tr_accessList formals) {
  Tr_level level = checked_malloc(sizeof(struct Tr_level_));
  level->parent = parent;
  level->name = name;
  level->frame = frame;
  level->formals = formals;
  return level;
}

static Tr_level outerLevel = NULL;

Tr_level Tr_outermost(void) {
  if (!outerLevel)
    outerLevel = Tr_newLevel(NULL, Temp_newlabel(), NULL);
  return outerLevel;
}

static Tr_accessList makeAccessList(F_accessList fAccessList, Tr_level level) {
  Tr_accessList head = NULL, tail = NULL;
  while (fAccessList) {
    F_access current = fAccessList->head;
    Tr_accessList trAccess = Tr_AccessList(Tr_Access(level, current), NULL);
    if (!head)
      head = trAccess;
    else
      tail->tail = trAccess;
    tail = trAccess;
    fAccessList = fAccessList->tail;
  }
  return head;
}

Tr_level Tr_newLevel(Tr_level parent, Temp_label name, U_boolList formals) {
  F_frame frame = F_newFrame(name, formals);
  Tr_level newLevel = Tr_Level(parent, name, frame, NULL);
  newLevel->formals = makeAccessList(F_formals(frame), newLevel);
  return newLevel;
}

Tr_accessList Tr_formals(Tr_level level) { return level->formals; }

Tr_access Tr_allocLocal(Tr_level level, bool escape) {
  F_access a = F_allocLocal(level->frame, escape);
  return Tr_Access(level, a);
}

typedef struct patchList_ *patchList;
struct patchList_ {
  Temp_label *head;
  patchList tail;
};
static patchList PatchList(Temp_label *head, patchList tail) {
  patchList patch = checked_malloc(sizeof(*patch));
  patch->head = head;
  patch->tail = tail;
  return patch;
}

void doPatch(patchList tList, Temp_label label) {
  for (; tList; tList = tList->tail)
    *(tList->head) = label;
}

patchList joinPatch(patchList first, patchList second) {
  if (!first)
    return second;
  for (; first->tail; first = first->tail)
    ; /* go to end of list */
  first->tail = second;
  return first;
}

struct Cx {
  patchList trues;
  patchList falses;
  T_stm stm;
};

struct Tr_exp_ {
  enum {
    Tr_ex, // Evalutes to a value.
    Tr_nx, // No value (while, function call).
    Tr_cx, // Conditional.
  } kind;
  union {
    T_exp ex;
    T_stm nx;
    struct Cx cx;
  } u;
};

static Tr_exp Tr_Ex(T_exp ex) {
  Tr_exp newEx = checked_malloc(sizeof(*newEx));
  newEx->kind = Tr_ex;
  newEx->u.ex = ex;
  return newEx;
}

static Tr_exp Tr_Nx(T_stm nx) {
  Tr_exp newNx = checked_malloc(sizeof(*newNx));
  newNx->kind = Tr_nx;
  newNx->u.nx = nx;
  return newNx;
}

static Tr_exp Tr_Cx(patchList trues, patchList falses, T_stm stm) {
  Tr_exp newCx = checked_malloc(sizeof(*newCx));
  newCx->kind = Tr_cx;
  newCx->u.cx.trues = trues;
  newCx->u.cx.falses = falses;
  newCx->u.cx.stm = stm;
  return newCx;
}

static T_exp unEx(Tr_exp e) {
  switch (e->kind) {
  case Tr_ex:
    return e->u.ex;
  case Tr_cx: {
    Temp_temp r = Temp_newtemp();
    Temp_label t = Temp_newlabel(), f = Temp_newlabel();
    doPatch(e->u.cx.trues, t);
    doPatch(e->u.cx.falses, f);
    return T_Eseq(
        T_Move(T_Temp(r), T_Const(1)),
        T_Eseq(e->u.cx.stm,
               T_Eseq(T_Label(f), T_Eseq(T_Move(T_Temp(r), T_Const(0)),
                                         T_Eseq(T_Label(t), T_Temp(r))))));
  }
  case Tr_nx:
    return T_Eseq(e->u.nx, T_Const(0));
  }
  assert(0);
}

static T_stm unNx(Tr_exp e) {
  switch (e->kind) {
  case Tr_ex: {
    return T_Exp(e->u.ex);
  }
  case Tr_cx: {
    Temp_temp r = Temp_newtemp();
    Temp_label t = Temp_newlabel(), f = Temp_newlabel();
    doPatch(e->u.cx.trues, t);
    doPatch(e->u.cx.falses, f);
    return T_Exp(T_Eseq(
        T_Move(T_Temp(r), T_Const(1)),
        T_Eseq(e->u.cx.stm,
               T_Eseq(T_Label(f), T_Eseq(T_Move(T_Temp(r), T_Const(0)),
                                         T_Eseq(T_Label(t), T_Temp(r)))))));
  }
  case Tr_nx: {
    return e->u.nx;
  }
  }
  assert(0);
}

static struct Cx unCx(Tr_exp e) {
  switch (e->kind) {
  case Tr_ex: {
    struct Cx cx;
    cx.stm = T_Cjump(T_eq, e->u.ex, T_Const(0), NULL, NULL);
    cx.trues = PatchList(&cx.stm->u.CJUMP.false, NULL);
    cx.falses = PatchList(&cx.stm->u.CJUMP.true, NULL);
    return cx;
  }
  case Tr_nx: {
    // We know that this isn't allowed.
    assert(0);
  }
  case Tr_cx: {
    return e->u.cx;
  }
  }
  assert(0);
}

Tr_exp Tr_simpleVar(Tr_access access, Tr_level level) {
  // Travel up each level and find the one that this access belongs to.
  T_exp addr = T_Temp(F_FP());
  while (level != access->level) {
    F_access staticLink = F_formals(level->frame)->head;
    addr = F_Exp(staticLink, addr);
    level = level->parent;
  }
  return Tr_Ex(F_Exp(access->access, addr));
}

Tr_exp Tr_fieldVar(Tr_exp record, size_t fieldNum) {
  T_exp recordMem = unEx(record);
  T_exp memoryAddress =
      T_Binop(T_plus, recordMem, T_Const(fieldNum * F_wordSize));
  return Tr_Ex(T_Mem(memoryAddress));
}

Tr_exp Tr_subscriptVar(Tr_exp array, Tr_exp index) {
  T_exp arrayMem = unEx(array);
  T_exp offset = T_Binop(T_mul, unEx(index), T_Const(F_wordSize));
  T_exp memoryAddress = T_Binop(T_plus, arrayMem, offset);
  return Tr_Ex(T_Mem(memoryAddress));
}

Tr_exp Tr_arrayVar(Tr_exp sizeExp, Tr_exp initExp) {
  T_expList args = T_ExpList(unEx(sizeExp), T_ExpList(unEx(initExp), NULL));
  return Tr_Ex(F_externalCall("initArray", args));
}

Tr_exp Tr_recordVar(size_t numFields) {
  T_expList args = T_ExpList(T_Const(numFields * F_wordSize), NULL);
  return Tr_Ex(F_externalCall("initRecord", args));
}

Tr_exp Tr_intExp(int val) { return Tr_Ex(T_Const(val)); }

// Global fragments list.
static F_fragList frags = NULL;

static void Tr_pushFrag(F_frag frag) {
  // Put at the head of the list.
  F_fragList newEntry = F_FragList(frag, frags);
  frags = newEntry;
}

Tr_exp Tr_stringExp(string val) {
  Temp_label stringLabel = Temp_newlabel();
  F_frag stringFrag = F_StringFrag(stringLabel, val);
  Tr_pushFrag(stringFrag);
  return Tr_Ex(T_Name(stringLabel));
}

Tr_exp Tr_callExp(Temp_label functionLabel, Tr_expList args) {
  // TODO: Pass in the static link?
  T_expList convertedHead = NULL, convertedTail = NULL;
  while (args) {
    T_expList convertedArg = T_ExpList(unEx(args->head), NULL);
    if (convertedHead)
      convertedTail->tail = convertedArg;
    else
      convertedHead = convertedArg;
    args = args->tail;
  }
  return Tr_Ex(T_Call(T_Name(functionLabel), convertedHead));
}

Tr_exp Tr_binOpExp(A_oper op, Tr_exp left, Tr_exp right) {
  T_binOp binOp;
  switch (op) {
  case A_plusOp:
    binOp = T_plus;
    break;
  case A_minusOp:
    binOp = T_minus;
    break;
  case A_timesOp:
    binOp = T_mul;
    break;
  case A_divideOp:
    binOp = T_div;
    break;
  default:
    assert(0);
    return NULL;
  }
  return Tr_Ex(T_Binop(binOp, unEx(left), unEx(right)));
}

Tr_exp Tr_relOpExp(A_oper op, Tr_exp left, Tr_exp right) {
  T_relOp relOp;
  switch (op) {
  case A_eqOp:
    relOp = T_eq;
    break;
  case A_neqOp:
    relOp = T_ne;
    break;
  case A_ltOp:
    relOp = T_lt;
    break;
  case A_leOp:
    relOp = T_le;
    break;
  case A_gtOp:
    relOp = T_gt;
    break;
  case A_geOp:
    relOp = T_ge;
    break;
  default:
    assert(0);
    return NULL;
  }
  T_stm cond = T_Cjump(relOp, unEx(left), unEx(right), NULL, NULL);
  patchList trues = PatchList(&cond->u.CJUMP.false, NULL);
  patchList falses = PatchList(&cond->u.CJUMP.true, NULL);
  return Tr_Cx(trues, falses, cond);
}

Tr_exp Tr_relOpStringExp(A_oper oper, Tr_exp left, Tr_exp right) {
  assert(oper == T_eq || oper == T_ne);
  T_expList args = T_ExpList(unEx(left), T_ExpList(unEx(right), NULL));
  T_exp equals = F_externalCall("stringEqual", args);
  if (oper == T_eq)
    return Tr_Ex(equals);
  else {
    // If we're checking that it's NOT equal, we'll need to negate the result.
    assert(oper == T_ne);
    return Tr_Ex(T_Binop(T_minus, T_Const(1), equals));
  }
}

Tr_exp Tr_seqExp(Tr_expList expList) {
  T_exp convertedHead = NULL, convertedTail = NULL;
  while (expList) {
    T_exp convertedNode = T_Eseq(unNx(expList->head), NULL);
    expList = expList->tail;
    if (convertedHead)
      convertedTail->u.ESEQ.exp = convertedNode;
    else
      convertedHead = convertedNode;
    convertedTail = convertedNode;
  }
  return Tr_Ex(convertedHead);
}

Tr_exp Tr_assignExp(Tr_exp left, Tr_exp right) {
  T_exp assignMem = T_Mem(unEx(left));
  return Tr_Nx(T_Move(assignMem, unEx(right)));
}

Tr_exp Tr_ifThenExp(Tr_exp condExp, Tr_exp thenExp, Tr_exp elseExp) {
  if (elseExp)
    return Tr_ifThenElseExp(condExp, thenExp, elseExp);
  else
    return Tr_ifThenNoElseExp(condExp, thenExp);
}

Tr_exp Tr_ifThenElseExp(Tr_exp condExp, Tr_exp thenExp, Tr_exp elseExp) {
  struct Cx c = unCx(condExp);
  T_exp t = unEx(thenExp), e = unEx(elseExp);
  Temp_label trueLabel = Temp_newlabel(), falseLabel = Temp_newlabel(),
             joinLabel = Temp_newlabel();
  Temp_temp r = Temp_newtemp();
  T_stm seq =
      T_Seq(c.stm, T_Seq(T_Label(trueLabel),
                         T_Seq(T_Move(T_Temp(r), t),
                               T_Seq(T_Jump(T_Name(joinLabel),
                                            Temp_LabelList(joinLabel, NULL)),
                                     T_Seq(T_Label(falseLabel),
                                           T_Seq(T_Move(T_Temp(r), e),
                                                 T_Seq(T_Label(joinLabel),
                                                       T_Exp(T_Temp(r)))))))));
  doPatch(c.trues, trueLabel);
  doPatch(c.falses, falseLabel);
  return Tr_Nx(seq);
}

Tr_exp Tr_ifThenNoElseExp(Tr_exp condExp, Tr_exp thenExp) {
  struct Cx c = unCx(condExp);
  T_exp t = unEx(thenExp);
  Temp_label trueLabel = Temp_newlabel(), falseLabel = Temp_newlabel();
  T_stm seq =
      T_Seq(c.stm, T_Seq(T_Label(trueLabel),
                         T_Seq(T_Exp(t), T_Seq(T_Label(falseLabel), NULL))));
  doPatch(c.trues, trueLabel);
  doPatch(c.falses, falseLabel);
  return Tr_Nx(seq);
}

Tr_exp Tr_whileExp(Tr_exp condExp, Tr_exp bodyExp) {
  struct Cx c = unCx(condExp);
  T_exp b = unEx(bodyExp);
  Temp_label condLabel = Temp_newlabel(), trueLabel = Temp_newlabel(),
             falseLabel = Temp_newlabel();
  T_stm seq = T_Seq(
      T_Label(condLabel),
      T_Seq(c.stm,
            T_Seq(T_Label(trueLabel),
                  T_Seq(T_Exp(b), T_Seq(T_Jump(T_Name(condLabel),
                                               Temp_LabelList(condLabel, NULL)),
                                        T_Seq(T_Label(falseLabel), NULL))))));
  doPatch(c.trues, trueLabel);
  doPatch(c.falses, falseLabel);
  return Tr_Nx(seq);
}

Tr_exp Tr_forExp(Tr_exp lowExp, Tr_exp highExp, Tr_exp bodyExp) {
  Temp_temp counter = Temp_newtemp();
  T_exp low = unEx(lowExp), high = unEx(highExp), body = unEx(bodyExp);
  Temp_label trueLabel = Temp_newlabel(), falseLabel = Temp_newlabel();

  // Put together the cond.
  T_stm condStm = T_Cjump(T_lt, T_Temp(counter), high, NULL, NULL);
  patchList trues = PatchList(&condStm->u.CJUMP.false, NULL);
  patchList falses = PatchList(&condStm->u.CJUMP.true, NULL);
  struct Cx c = unCx(Tr_Cx(trues, falses, condStm));

  // Put together the loop.
  T_stm seq =
      T_Seq(T_Move(T_Temp(counter), low),
            T_Seq(condStm, T_Seq(T_Label(trueLabel),
                                 T_Seq(T_Exp(body), T_Label(falseLabel)))));

  // Patch up the cond labels.
  doPatch(trues, trueLabel);
  doPatch(falses, falseLabel);
  return Tr_Nx(seq);
}

void Tr_procEntryExit(Tr_level level, Tr_exp body, Tr_accessList formals) {
  T_stm stm = unNx(body);
  F_frame f = level->frame;
  Tr_pushFrag(F_ProcFrag(stm, f));
}

F_fragList Tr_getResult(void) { return frags; }
