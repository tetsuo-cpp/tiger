#include "util.h"
#include "slp.h"
#include "prog1.h"

#include <stdio.h>

int maxargs(A_stm stm);

int maxargsExp(A_exp exp) {
  switch (exp->kind) {
  case A_idExp:
  case A_numExp:
    return 0;
  case A_opExp: {
    int lhs = maxargsExp(exp->u.op.left);
    int rhs = maxargsExp(exp->u.op.right);
    return lhs > rhs ? lhs : rhs;
  }
  case A_eseqExp: {
    int stm = maxargs(exp->u.eseq.stm);
    int innerExp = maxargsExp(exp->u.eseq.exp);
    return stm > innerExp ? stm : innerExp;
  }
  }
}

int maxargs(A_stm stm) {
  switch (stm->kind) {
  case A_compoundStm: {
    int lhs = maxargs(stm->u.compound.stm1);
    int rhs = maxargs(stm->u.compound.stm2);
    return lhs > rhs ? lhs : rhs;
  }
  case A_assignStm:
    return maxargsExp(stm->u.assign.exp);
  case A_printStm: {
    A_expList current = stm->u.print.exps;
    int count = 1;
    while (current->kind == A_pairExpList) {
      current = current->u.pair.tail;
      ++count;
    }
    return count;
  }
  }
}

int main() {
  printf("The max number of print args is %d.\n", maxargs(prog()));
  return 0;
}
