#ifndef _ESH_MACROS_H
#define _ESH_MACROS_H

#include "esh-error.h"

#define RetError(X) if (X == -1) { DEBUG_PRINT(("Error on %s\n", "X")); return -1;}
#endif
