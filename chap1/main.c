#include "util.h"
#include "slp.h"
#include "prog1.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct table *Table_;
struct table {
  string id;
  int value;
  Table_ tail;
};

Table_ Table(string id, int value, struct table *tail) {
  Table_ t = malloc(sizeof(*t));
  t->id = id;
  t->value = value;
  t->tail = tail;
  return t;
}

int lookup(Table_ t, string key) {
  assert(t);
  if (strcmp(t->id, key) == 0)
    return t->value;
  return lookup(t->tail, key);
}

Table_ interpStm(A_stm s, Table_ t);

struct IntAndTable {
  int i;
  Table_ t;
};
struct IntAndTable interpExp(A_exp e, Table_ t);

Table_ interpStm(A_stm s, Table_ t) {
  switch (s->kind) {
  case A_compoundStm: {
    t = interpStm(s->u.compound.stm1, t);
    t = interpStm(s->u.compound.stm2, t);
    return t;
  }
  case A_assignStm: {
    struct IntAndTable result = interpExp(s->u.assign.exp, t);
    return Table(s->u.assign.id, result.i, result.t);
  }
  case A_printStm: {
    A_expList current = s->u.print.exps;
    for (; current->kind == A_pairExpList; current = current->u.pair.tail) {
      struct IntAndTable it = interpExp(current->u.pair.head, t);
      t = it.t;
      printf("%d ", it.i);
    }
    struct IntAndTable it = interpExp(current->u.last, t);
    t = it.t;
    printf("%d\n", it.i);
    return t;
  }
  }
}

struct IntAndTable interpExp(A_exp e, Table_ t) {
  switch (e->kind) {
  case A_idExp: {
    struct IntAndTable it = {lookup(t, e->u.id), t};
    return it;
  }
  case A_numExp: {
    struct IntAndTable it = {e->u.num, t};
    return it;
  }
  case A_opExp: {
    struct IntAndTable lhs = interpExp(e->u.op.left, t);
    t = lhs.t;
    struct IntAndTable rhs = interpExp(e->u.op.right, t);
    t = rhs.t;
    int result;
    switch (e->u.op.oper) {
    case A_plus:
      result = lhs.i + rhs.i;
      break;
    case A_minus:
      result = lhs.i - rhs.i;
      break;
    case A_times:
      result = lhs.i * rhs.i;
      break;
    case A_div:
      result = lhs.i / rhs.i;
      break;
    }
    struct IntAndTable it = {result, t};
    return it;
  }
  case A_eseqExp:
    t = interpStm(e->u.eseq.stm, t);
    return interpExp(e->u.eseq.exp, t);
  }
}

void interp(A_stm stm) { interpStm(stm, NULL); }

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

typedef struct tree *T_tree;
struct tree {
  T_tree left;
  string key;
  T_tree right;
  void *binding;
};

T_tree Tree(T_tree l, string k, void *binding, T_tree r) {
  T_tree t = checked_malloc(sizeof(*t));
  t->left = l;
  t->key = k;
  t->binding = binding;
  t->right = r;
  return t;
}

T_tree insert(string key, void *binding, T_tree t) {
  if (t == NULL)
    return Tree(NULL, key, binding, NULL);
  else if (strcmp(key, t->key) < 0)
    return Tree(insert(key, binding, t->left), t->key, t->binding, t->right);
  else if (strcmp(key, t->key) > 0)
    return Tree(t->left, t->key, t->binding, insert(key, binding, t->right));
  else
    return Tree(t->left, key, binding, t->right);
}

T_tree createTree() {
  T_tree t = NULL;
  t = insert("aa", "aa binding", t);
  t = insert("ab", "ab binding", t);
  t = insert("bb", "bb binding", t);
  t = insert("bc", "bc binding", t);
  t = insert("zz", "zz binding", t);
  return t;
}

bool member(string key, T_tree t) {
  if (!t)
    return false;
  int cmp = strcmp(key, t->key);
  if (cmp > 0)
    return member(key, t->right);
  else if (cmp < 0)
    return member(key, t->left);
  else
    return true;
}

void *lookupTree(string key, T_tree t) {
  if (!t)
    return NULL;
  int cmp = strcmp(key, t->key);
  if (cmp > 0)
    return lookupTree(key, t->right);
  else if (cmp < 0)
    return lookupTree(key, t->left);
  else
    return t->binding;
}

int main() {
  A_stm slpProgram = prog();
  printf("=== maxargs ===\n");
  printf("The max number of print args is %d.\n", maxargs(slpProgram));

  printf("=== interp ===\n");
  interp(slpProgram);

  printf("=== tree ===\n");
  T_tree tree = createTree();
  // Testing member.
  assert(member("aa", tree));
  assert(member("ab", tree));
  assert(member("bb", tree));
  assert(member("bc", tree));
  assert(member("zz", tree));
  assert(!member("za", tree));
  assert(!member("az", tree));
  assert(!member("cb", tree));
  assert(!member("az", tree));
  // Testing lookupTree.
  assert(strcmp(lookupTree("aa", tree), "aa binding") == 0);
  assert(strcmp(lookupTree("ab", tree), "ab binding") == 0);
  assert(strcmp(lookupTree("bb", tree), "bb binding") == 0);
  assert(strcmp(lookupTree("bc", tree), "bc binding") == 0);
  assert(strcmp(lookupTree("zz", tree), "zz binding") == 0);
  assert(lookupTree("za", tree) == NULL);
  assert(lookupTree("az", tree) == NULL);
  assert(lookupTree("cb", tree) == NULL);
  assert(lookupTree("az", tree) == NULL);
  printf("Success.\n");
  return 0;
}
