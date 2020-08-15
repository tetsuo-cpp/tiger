#include <stdio.h>

#include "util.h"
#include "symbol.h"
#include "temp.h"

#include "frame.h"

struct F_access_ {
  enum { inFrame, inReg } kind;
  union {
    int offset;    // InFrame.
    Temp_temp reg; // InReg.
  } u;
};

static F_access InFrame(int offset);
static F_access InReg(Temp_temp reg);
