#include <stdio.h>

#include "util.h"
#include "symbol.h"
#include "temp.h"
#include "tree.h"

#include "frame.h"

struct F_access_ {
  enum { inFrame, inReg } kind;
  union {
    int offset;    // InFrame.
    Temp_temp reg; // InReg.
  } u;
};

const int F_wordSize = 4;

static F_access InFrame(int offset) {
  F_access a = checked_malloc(sizeof(struct F_access_));
  a->kind = inFrame;
  a->u.offset = offset;
  return a;
}

static F_access InReg(Temp_temp reg) {
  F_access a = checked_malloc(sizeof(struct F_access_));
  a->kind = inReg;
  a->u.reg = reg;
  return a;
}

struct F_frame_ {
  Temp_label name;
  F_accessList formals;
  int localCount;
};

static F_accessList F_AccessList(F_access head, F_accessList tail) {
  F_accessList list = checked_malloc(sizeof(struct F_accessList_));
  list->head = head;
  list->tail = tail;
  return list;
}

static F_accessList makeAccessList(U_boolList list) {
  F_accessList head = NULL, tail = NULL;
  int formalOffset = -1; // Zero address is reserved for return value.
  while (list) {
    F_access current = NULL;
    if (list->head)
      current = InFrame(formalOffset * F_wordSize);
    else
      current = InReg(Temp_newtemp());
    assert(current);
    F_accessList newTail = F_AccessList(current, NULL);
    if (!head)
      head = newTail;
    else
      tail->tail = newTail;
    tail = newTail;
    list = list->tail;
    --formalOffset;
  }
  return head;
}

F_frame F_newFrame(Temp_label name, U_boolList formals) {
  F_frame frame = checked_malloc(sizeof(struct F_frame_));
  frame->name = name;
  frame->formals = makeAccessList(formals);
  frame->localCount = 0;
  return frame;
}

Temp_label F_name(F_frame f) { return f->name; }

F_accessList F_formals(F_frame f) { return f->formals; }

F_access F_allocLocal(F_frame f, bool escape) {
  ++f->localCount;
  F_access local = NULL;
  if (escape)
    local = InFrame(-f->localCount * F_wordSize);
  else
    local = InReg(Temp_newtemp());
  return local;
}

static Temp_temp fp = NULL;
Temp_temp F_FP(void) {
  if (!fp)
    fp = Temp_newtemp();
  return fp;
}

T_exp F_Exp(F_access acc, T_exp framePtr) {
  assert(acc->kind == inFrame);
  T_exp memoryAddress = T_Binop(T_plus, framePtr, T_Const(acc->u.offset));
  return T_Mem(memoryAddress);
}

T_exp F_externalCall(string s, T_expList args) {
  // TODO: Adjust for static links, underscores in labels, etc.
  return T_Call(T_Name(Temp_namedlabel(s)), args);
}

T_stm F_procEntryExit1(F_frame f, T_stm stm) {
  (void)f;
  return stm;
}

F_frag F_StringFrag(Temp_label label, string str) {
  F_frag frag = checked_malloc(sizeof(*frag));
  frag->kind = F_stringFrag;
  frag->u.string.label = label;
  frag->u.string.str = str;
  return frag;
}

F_frag F_ProcFrag(T_stm body, F_frame frame) {
  F_frag frag = checked_malloc(sizeof(*frag));
  frag->kind = F_procFrag;
  frag->u.proc.body = body;
  frag->u.proc.frame = frame;
  return frag;
}

F_fragList F_FragList(F_frag head, F_fragList tail) {
  F_fragList fragList = checked_malloc(sizeof(*fragList));
  fragList->head = head;
  fragList->tail = tail;
  return fragList;
}
