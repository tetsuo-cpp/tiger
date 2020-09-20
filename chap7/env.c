#include <stddef.h>
#include <stdio.h>

#include "util.h"
#include "symbol.h"
#include "types.h"
#include "temp.h"
#include "translate.h"

#include "env.h"

E_enventry E_VarEntry(Tr_access access, Ty_ty ty) {
  E_enventry e = checked_malloc(sizeof(*e));
  e->kind = E_varEntry;
  e->u.var.access = access;
  e->u.var.ty = ty;
  return e;
}

E_enventry E_FunEntry(Tr_level level, Temp_label label, Ty_tyList formals,
                      Ty_ty result) {
  E_enventry e = checked_malloc(sizeof(*e));
  e->kind = E_funEntry;
  e->u.fun.level = level;
  e->u.fun.label = label;
  e->u.fun.formals = formals;
  e->u.fun.result = result;
  return e;
}

S_table E_base_tenv(void) {
  S_table tenv = S_empty();
  S_enter(tenv, S_Symbol("int"), Ty_Int());
  S_enter(tenv, S_Symbol("string"), Ty_String());
  return tenv;
}

S_table E_base_venv(void) {
  S_table venv = S_empty();
  S_enter(venv, S_Symbol("print"),
          E_FunEntry(Tr_outermost(), Temp_newlabel(),
                     Ty_TyList(Ty_String(), NULL), Ty_Void()));
  S_enter(venv, S_Symbol("flush"),
          E_FunEntry(Tr_outermost(), Temp_newlabel(), NULL, Ty_Void()));
  S_enter(venv, S_Symbol("getchar"),
          E_FunEntry(Tr_outermost(), Temp_newlabel(), NULL, Ty_String()));
  S_enter(venv, S_Symbol("ord"),
          E_FunEntry(Tr_outermost(), Temp_newlabel(),
                     Ty_TyList(Ty_String(), NULL), Ty_Int()));
  S_enter(venv, S_Symbol("chr"),
          E_FunEntry(Tr_outermost(), Temp_newlabel(), Ty_TyList(Ty_Int(), NULL),
                     Ty_String()));
  S_enter(venv, S_Symbol("size"),
          E_FunEntry(Tr_outermost(), Temp_newlabel(),
                     Ty_TyList(Ty_String(), NULL), Ty_Int()));
  S_enter(venv, S_Symbol("substring"),
          E_FunEntry(Tr_outermost(), Temp_newlabel(),
                     Ty_TyList(Ty_String(),
                               Ty_TyList(Ty_Int(), Ty_TyList(Ty_Int(), NULL))),
                     Ty_String()));
  S_enter(venv, S_Symbol("concat"),
          E_FunEntry(Tr_outermost(), Temp_newlabel(),
                     Ty_TyList(Ty_String(), Ty_TyList(Ty_String(), NULL)),
                     Ty_String()));
  S_enter(venv, S_Symbol("not"),
          E_FunEntry(Tr_outermost(), Temp_newlabel(), Ty_TyList(Ty_Int(), NULL),
                     Ty_Int()));
  S_enter(venv, S_Symbol("exit"),
          E_FunEntry(Tr_outermost(), Temp_newlabel(), Ty_TyList(Ty_Int(), NULL),
                     Ty_Void()));
  return venv;
}
