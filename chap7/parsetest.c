#include <stdio.h>
#include <stdlib.h>
#include "util.h"
#include "errormsg.h"
#include "symbol.h"
#include "absyn.h"
#include "prabsyn.h"
#include "temp.h"
#include "tree.h"
#include "frame.h"
#include "semant.h"
#include "printtree.h"

extern int yyparse(void);
extern int yydebug;
extern A_exp absyn_root;

void parse(string fname) {
  EM_reset(fname);
  if (yyparse() == 0) /* parsing worked */
    fprintf(stderr, "Parsing successful!\n");
  else
    fprintf(stderr, "Parsing failed\n");
  fprintf(stdout, "Abstract Syntax Tree:\n");
  if (absyn_root) {
    pr_exp(stdout, absyn_root, 0);
    F_fragList frags = SEM_transProg(absyn_root);
    assert(frags);
    fprintf(stdout, "\nIR Tree:\n");
    while (frags) {
      F_frag f = frags->head;
      if (f->kind == F_procFrag) {
        T_stm body = f->u.proc.body;
        // Am I missing something here?
        // Why are we expecting a T_stmList?
        printStmList(stdout, T_StmList(body, NULL));
      }
      frags = frags->tail;
    }
  }
}

int main(int argc, char **argv) {
  // yydebug = 1;
  if (argc != 2) {
    fprintf(stderr, "usage: a.out filename\n");
    exit(1);
  }
  parse(argv[1]);
  return 0;
}
