#include <stdio.h>

#include "util.h"
#include "symbol.h"
#include "temp.h"
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
