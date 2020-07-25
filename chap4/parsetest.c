#include <stdio.h>
#include "util.h"
#include "errormsg.h"

extern int yyparse(void);
extern int yydebug;

void parse(string fname) {
  EM_reset(fname);
  if (yyparse() == 0) /* parsing worked */
    fprintf(stderr, "Parsing successful!\n");
  else
    fprintf(stderr, "Parsing failed\n");
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
