#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <sys/wait.h>
#include <errno.h>

#include "list.h"
#include "esh.h"
#include "esh-sys-utils.h"

#include "esh-builtin.h"

#include "esh-job.h"
#include "esh-signal.h"
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
        else if (!strcmp("fg", command->argv[0])) {
            DEBUG_PRINT(("Executing fg to %s\n", command->argv[1]));
            int status;
            /* Protect the list while we access the job list */
            esh_signal_block(SIGCHLD);
            /* Find the job if it exists */
            struct esh_pipeline* job = esh_get_job_from_jid(atoi(command->argv[1]));
            if (job == NULL) {
                errprintf("Invalid job number %s", command->argv[1]);
                esh_signal_unblock(SIGCHLD);
                return -1;
            }
            DEBUG_PRINT(("Setting foreground group\n"));
            if (tcsetpgrp(esh_sys_tty_getfd(), job->pgrp)) {
                DEBUG_PRINT(("Error on tcsetpgrp\n"));
                esh_signal_unblock(SIGCHLD);
                return -1;
            }
            /* Restore the fg group info */
            esh_sys_tty_restore(&job->saved_tty_state);
            /* Book keeping */
            job->status = FOREGROUND;
            jobs.fg_job = job;
            esh_print_job_status(job);
            DEBUG_PRINT(("Foregrounding %d\n", job->jid));
            if (kill(-1*job->pgrp, SIGCONT) == -1) {
                kill_error();
                return -1;
            }
            /* Unblock here */
            esh_signal_unblock(SIGCHLD);
            /* Wait on job */
            pid_t pid;
            pid_t pgrp = job->pgrp;
            bool running = 1;
            while (running) {
                if ((pid = waitpid(pgrp, &status, WUNTRACED)) == -1) {
                    if (errno == ECHILD) {
                        /* This means the signal handler picked this up */
                        break;
                    }
                }
                else if (pid > 0 && !WIFSTOPPED(status)) {
                    /* clean up */
                    esh_signal_block(SIGCHLD);
                    struct esh_command* cmd = esh_get_cmd_from_pid(pid);
                    running = !esh_signal_cleanup(cmd, status);
                    esh_signal_unblock(SIGCHLD);
                }
                else {
                    break;
                }
            }
            esh_signal_block(SIGCHLD);
            DEBUG_PRINT(("Finished waiting, return of %d\n", pid));
            /* Clear the foreground job */
            jobs.fg_job = NULL;
            if (WIFSTOPPED(status)) {
                /* It's a background job! */
                DEBUG_PRINT(("FG job was stopped\n"));
                job->status = STOPPED;
                esh_print_job_status(job);
                esh_sys_tty_save(&job->saved_tty_state);
                job->bg_job = true;
            }
            /* Reclaim control of the terminal */
            if (tcsetpgrp(esh_sys_tty_getfd(), getpgrp()) == -1) {
                DEBUG_PRINT(("Error on tcsetpgrp\n"));
                tcsetpgrp_error();
                return -1;
            }
            esh_sys_tty_restore(tty_state);
            return 1;
        }
        else if (!strcmp("kill", command->argv[0])) {
            DEBUG_PRINT(("Executing kill\n"));
            /* TODO: handle empty list */
            /* Protect the list while we access the job list */
            esh_signal_block(SIGCHLD);
            /* Find the job if it exists */
            struct esh_pipeline* job = esh_get_job_from_jid(atoi(command->argv[1]));
            if (job == NULL) {
                errprintf("Invalid job number %s", command->argv[1]);
                return -1;
            }
            if (job->status == NEEDSTERMINAL) {
                DEBUG_PRINT(("Sending SIGHUP\n"));
                if (kill(-1*job->pgrp, SIGHUP) == -1) {
                    kill_error();
                    return -1;
                }
            }
            else {
                DEBUG_PRINT(("Sending SIGTERM\n"));
                if (kill(-1*job->pgrp, SIGTERM) == -1) {
                    kill_error();
                    return -1;
                }
            }
            if (kill(-1*job->pgrp, SIGCONT) == -1) {
                kill_error();
                return -1;
            }

            /* Unblock here */
            esh_signal_unblock(SIGCHLD);
            return 1;
        }
        else if (!strcmp("jobs", command->argv[0])) {
            DEBUG_PRINT(("Executing jobs\n"));
            esh_signal_block(SIGCHLD);
            if (list_empty(&jobs.jobs)) return 1;
            /* Now the real work */
            struct list_elem* pipeline_elem = list_front(&jobs.jobs);
            for (; pipeline_elem != list_tail(&jobs.jobs); pipeline_elem = list_next(pipeline_elem)) {
                esh_print_job_status(list_entry(pipeline_elem, struct esh_pipeline, elem));
            }
            return 1;
            esh_signal_unblock(SIGCHLD);
        }
        else if (!strcmp("stop", command->argv[0])) {
            DEBUG_PRINT(("Executing stop on %d\n", atoi(command->argv[1])));
            struct esh_pipeline* job = esh_get_job_from_jid(atoi(command->argv[1]));
            if (job == NULL) {
                errprintf("Invalid job number %s", command->argv[1]);
                return -1;
            }
            if (kill(-1*job->pgrp, SIGTSTP) == -1) {
                kill_error();
                return -1;
            }
            job->status = STOPPED;
            /* Print status */
            esh_print_job_status(job);
            /* Unblock here */
            esh_signal_unblock(SIGCHLD);
            return 1;
        }

    }
    return 0;
}

void esh_print_job_status(struct esh_pipeline* pipeline) {
    /* Print out the job information first */
    printf("[%d]\t", pipeline->jid);
    /* Print status */
    if (pipeline->status == BACKGROUND || pipeline->status == FOREGROUND) {
        printf("Running\t(");
    }
    else {
        printf("Stopped\t(");
    }
    
    struct list_elem* command_elem = list_front(&pipeline->commands);
    struct esh_command* command = list_entry(command_elem, struct esh_command, elem);

    DEBUG_PRINT(("Printing command\n"));
    int i;
    printf("%s", command->argv[0]);
    for (i = 1; command->argv[i] != NULL; i++) {
        printf(" %s", command->argv[i]);
    }
    command_elem = list_next(command_elem);

    for (; command_elem != list_tail(&pipeline->commands); command_elem = list_next(command_elem)) {
        command = list_entry(command_elem, struct esh_command, elem);
        printf(" |");
        for (i = 0; command->argv[i] != NULL; i++) {
            printf(" %s", command->argv[i]);
        }
    }
    printf(")\n");

}
