typedef struct Tr_access_ *Tr_access;
typedef struct Tr_accessList_ *Tr_accessList;
typedef struct Tr_level_ *Tr_level;
typedef struct Tr_exp_ *Tr_exp;
typedef struct Tr_expList_ *Tr_expList;

struct Tr_accessList_ {
  Tr_access head;
  Tr_accessList tail;
};

struct Tr_expList_ {
  Tr_exp head;
  Tr_expList tail;
};

Tr_accessList Tr_AccessList(Tr_access head, Tr_accessList tail);
Tr_expList Tr_ExpList(Tr_exp head, Tr_expList tail);

Tr_level Tr_outermost(void);
Tr_level Tr_newLevel(Tr_level parent, Temp_label name, U_boolList formals);
Tr_accessList Tr_formals(Tr_level level);
Tr_access Tr_allocLocal(Tr_level level, bool escape);

Tr_exp Tr_simpleVar(Tr_access, Tr_level);
Tr_exp Tr_fieldVar(Tr_exp, size_t);
Tr_exp Tr_subscriptVar(Tr_exp, Tr_exp);
Tr_exp Tr_arrayVar(Tr_exp, Tr_exp);
Tr_exp Tr_recordVar(size_t);
Tr_exp Tr_intExp(int);
Tr_exp Tr_stringExp(string);
Tr_exp Tr_callExp(Temp_label, Tr_expList);
Tr_exp Tr_binOpExp(A_oper, Tr_exp, Tr_exp);
Tr_exp Tr_relOpExp(A_oper, Tr_exp, Tr_exp);
Tr_exp Tr_relOpStringExp(A_oper, Tr_exp, Tr_exp);
Tr_exp Tr_seqExp(Tr_expList);
Tr_exp Tr_assignExp(Tr_exp, Tr_exp);
Tr_exp Tr_ifThenExp(Tr_exp, Tr_exp, Tr_exp);
Tr_exp Tr_ifThenElseExp(Tr_exp, Tr_exp, Tr_exp);
Tr_exp Tr_ifThenNoElseExp(Tr_exp, Tr_exp);
Tr_exp Tr_whileExp(Tr_exp, Tr_exp);
Tr_exp Tr_forExp(Tr_exp, Tr_exp, Tr_exp);

void Tr_procEntryExit(Tr_level level, Tr_exp body, Tr_accessList formals);
F_fragList Tr_getResult(void);
