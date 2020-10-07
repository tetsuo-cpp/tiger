#include <stdio.h>

#include "util.h"
#include "symbol.h"
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

// Global fragments list.
static F_fragList frags = NULL;

static void Tr_pushFrag(F_frag frag) {
  // Put at the head of the list.
  F_fragList newEntry = F_FragList(frag, frags);
  frags = newEntry;
}

void Tr_procEntryExit(Tr_level level, Tr_exp body, Tr_accessList formals) {
  T_stm stm = unNx(body);
  F_frame f = level->frame;
  Tr_pushFrag(F_ProcFrag(stm, f));
}

F_fragList Tr_getResult(void) { return frags; }
