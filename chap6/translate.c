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
