#include <string.h>
#include <unistd.h>

#include "list.h"
#include "esh.h"
#include "esh-builtin.h"
#include "esh-error.h"
#include "esh-debug.h"

/* Return 1 if handled, else 0, -1 on error. */
int esh_builtin(struct esh_pipeline* pipeline) {
    struct list_elem* command_first = list_front(&pipeline->commands);

    /* Fastest way to check if size is 1 */
    if (list_back(&pipeline->commands) == command_first) {
        struct esh_command* command = list_entry(command_first, struct esh_command, elem);
        if (!strcmp("cd", command->argv[0])) {
            DEBUG_PRINT(("Executing cd to %s\n", command->argv[1]));
            if (chdir(command->argv[1]) == -1) {
                chdir_error();
                return -1;
            }
            return 1;
        }
    }
    return 0;
}
