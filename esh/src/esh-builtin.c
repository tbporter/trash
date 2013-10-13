#include <string.h>
#include <unistd.h>
#include <signal.h>

#include "list.h"
#include "esh.h"
#include "esh-sys-utils.h"

#include "esh-builtin.h"

#include "esh-job.h"
#include "esh-error.h"
#include "esh-debug.h"
#include "esh-macros.h"

/* Return 1 if handled, else 0, -1 on error. */
int esh_builtin(struct esh_pipeline* pipeline) {
    struct list_elem* command_first = list_front(&pipeline->commands);

    /* Fastest way to check if size is 1 */
    if (list_back(&pipeline->commands) == command_first) {
        struct esh_command* command = list_entry(command_first, struct esh_command, elem);
        /* Background code */
        if (!strcmp("bg", command->argv[0])) {
            DEBUG_PRINT(("Executing bg to %s\n", command->argv[1]));
            /* Protect the list while we access the job list */
            esh_signal_block(SIGCHLD);
            /* Find the job if it exists */
            struct esh_pipeline* job = esh_get_job_from_jid(atoi(command->argv[1]));
            if (job == NULL) {
                errprintf("Invalid job number %s", command->argv[1]);
                return -1;
            }
            if (kill(-1*job->pgrp, SIGCONT) == -1) {
                kill_error();
                return -1;
            }

            /* Unblock here */
            esh_signal_unblock(SIGCHLD);
            DEBUG_PRINT(("bg not implemented yet\n"));
            return 1;
        }
        /* Foreground code */
        if (!strcmp("fg", command->argv[0])) {
            DEBUG_PRINT(("Executing fg to %s\n", command->argv[1]));
            /* Protect the list while we access the job list */
            esh_signal_block(SIGCHLD);
            /* Find the job if it exists */
            struct esh_pipeline* job = esh_get_job_from_jid(atoi(command->argv[1]));
            if (job == NULL) {
                errprintf("Invalid job number %s", command->argv[1]);
                esh_signal_unblock(SIGCHLD);
                return -1;
            }
            /* call sigcont on job->pgrp */
            esh_sys_tty_restore(&job->saved_tty_state);
            DEBUG_PRINT(("Foregrounding %d", job->jid));
            if (kill(-1*job->pgrp, SIGCONT) == -1) {
                kill_error();
                return -1;
            }
            if (tcsetpgrp(esh_sys_tty_getfd(), job->pgrp)) {
                DEBUG_PRINT(("Error on tcsetpgrp\n"));
                esh_signal_unblock(SIGCHLD);
                return -1;
            }
            job->status = FOREGROUND;
            jobs.fg_job = job;
            /* Unblock here */
            esh_signal_unblock(SIGCHLD);
            return 1;
        }
        /* */
    }
    return 0;
}
