typedef struct Tr_access_ *Tr_access;
typedef struct Tr_accessList_ *Tr_accessList;
typedef struct Tr_level_ *Tr_level;
typedef struct Tr_exp_ *Tr_exp;

struct Tr_accessList_ {
  Tr_access head;
  Tr_accessList tail;
};

Tr_accessList Tr_AccessList(Tr_access head, Tr_accessList tail);

Tr_level Tr_outermost(void);
Tr_level Tr_newLevel(Tr_level parent, Temp_label name, U_boolList formals);
Tr_accessList Tr_formals(Tr_level level);
Tr_access Tr_allocLocal(Tr_level level, bool escape);

Tr_exp Tr_simpleVar(Tr_access, Tr_level);
Tr_exp Tr_fieldVar(Tr_exp, size_t);

void Tr_procEntryExit(Tr_level level, Tr_exp body, Tr_accessList formals);
F_fragList Tr_getResult(void);
