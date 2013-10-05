#include <stdio.h>
/*
 * This can be used like:
 *     DEBUG_PRINT(("var1: %d; var2: %d; str: %s\n", var1, var2, str));
 */
#ifdef DEBUG
# define DEBUG_PRINT(x) printf x
#else
# define DEBUG_PRINT(x) do {} while (0)
#endif
