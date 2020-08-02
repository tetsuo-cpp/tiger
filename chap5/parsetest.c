#include <stdio.h>
#include "util.h"
#include "errormsg.h"
#include "symbol.h"
#include "absyn.h"
#include "prabsyn.h"
#include "semant.h"

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
  pr_exp(stdout, absyn_root, 0);
  SEM_transProg(absyn_root);
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
