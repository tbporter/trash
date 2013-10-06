#include <string.h>

#include "list.h"
#include "esh.h"
#include "esh-builtin.h"
#include "esh-debug.h"

/* Return 1 if handled, else 0 */
int esh_builtin(struct esh_pipeline* pipeline) {
    struct list_elem* command = list_front(&pipeline->commands);

    /* Fastest way to check if size is 1 */
    if (list_back(&pipeline->commands) == command) {
        if (return 0;
    }
    return 0;
}
