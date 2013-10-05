#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "debug.h"
#include "list.h"

#include "esh.h"
#include "esh-job.h"
#include "esh-error.h"
#include "esh-macros.h"

/*
 * Execute the command line and the pipes
 */
int esh_command_line_run(struct esh_command_line * cline) {
    /* Set up signal queuing */
    /* using shell or something?*/
    struct list_elem* pipeline;
    for (pipeline = list_front(&cline->pipes);pipeline !=
            list_tail(&cline->pipes); pipeline = list_next(pipeline)) {
        /* for each pipeline in command_line */
        DEBUG_PRINT(("Initilizing pipeline"));
        RetError(esh_pipeline_init(list_entry(pipeline, struct esh_pipeline, elem)));
        RetError(esh_pipeline_run(list_entry(pipeline, struct esh_pipeline, elem)));
    }

    /* Run queue */
    /*signal_queue_process*/
    return 0;
}

/*
 * Right now this only inits only iored
 * returns 0 on success and -1 on failure and calls appropriate error function.
 */
int esh_pipeline_init(struct esh_pipeline * pipeline) {
    /* Assume list is in correct state and non-empty (needed to perform
     * list_front) */

    /*=========== SHOULD IMPLEMENT CHECK FOR EMPTY PIPELINE */

    /* IO redirection init */
    DEBUG_PRINT(("Executing pipeline iored code\n"));
    /* Execute if not NULL */
    if (pipeline->iored_input) {
        int input_fd = open(pipeline->iored_input, 0x0);
        if (input_fd == -1) {
            DEBUG_PRINT(("Error opening input file %s\n", pipeline->iored_input));
            /* open_error should switch on errno and print and appropriate
             * message */
            open_error();
            return -1;
        }
    }
    else {
        /* If we set this here we can treat io redirection and
         * non-ioredirection the same later on */
        list_entry(list_front(&pipeline->commands), struct esh_command, elem)->input_fd = STDIN_FILENO;
    }

    /* Again for output, but slightly different*/
    if (pipeline->iored_output) {
        /* Bit magic to set the flags for append if it's needed
         * ======== DOESN'T WORK!!!! ======================= */
        int output_fd = open(pipeline->iored_output, 0x0 ^ (O_APPEND & ~!pipeline->append_to_output));
        if (output_fd == -1) {
            open_error();
            return -1;
        }
    }
    else {
        list_entry(list_back(&pipeline->commands), struct esh_command, elem)->output_fd = STDOUT_FILENO;
    }
    return 0;
}

/* Execute an initialized pipeline, must iterate and forkexec for each process.
 * Before the fork it must pipe.  After the fork, dup2 stdin and stdout in the
 * child and the parent sets the information in the command and closes the pipe
 * fds. 
 *
 * returns 0 on success and -1 on error */
int esh_pipeline_run(struct esh_pipeline* pipeline) {
    /* Assume list is in correct state and non-empty */
    struct list_elem* command = list_front(&pipeline->commands);

    /* WAT DO IF PULLING FROM STDIN AND IT'S BG? */

    /* Handle single command case seperate, it gets too nasty later on 
     * using list_back is faster than list_size: O(1) vs O(N)*/
    if (list_back(&pipeline->commands) == command) {
        return esh_command_exec(list_entry(command, struct esh_command, elem), 0);
    }
    
    /* This will handle the multi command case */
    int pipefd[2];
    
    if (pipe(pipefd) == -1) {
        DEBUG_PRINT(("Error creating pipe\n"));
        pipe_error();
        return -1;
    }

    list_entry(command, struct esh_command, elem)->output_fd = pipefd[1];

    pipeline->pgrp = esh_command_exec(list_entry(command, struct esh_command, elem), 0);
    /* If something went horribly wrong */
    if (pipeline->pgrp == -1) {
        return -1;
    }

    /* Close input fd if it's a file */
    if (pipeline->iored_input) {
        if (close(list_entry(command, struct esh_command, elem)->input_fd) == -1) {
            DEBUG_PRINT(("Error after close\n"));
            close_error();
            return -1;
        }
    }
    
    /* This is here because I don't know if the compiler will be able to
     * optimize this */
    struct list_elem* last_command = list_prev(list_back(&pipeline->commands));
    while (command != last_command) {

        if (close(list_entry(command, struct esh_command, elem)->output_fd) == -1) {
            DEBUG_PRINT(("Error after close\n"));
            close_error();
            return -1;
        }

        command = list_next(command);

        /* Hook up the input from the last pipe */
        list_entry(command, struct esh_command, elem)->input_fd = pipefd[0];

        /* Meaningful values of pipefd are now all in other locations */
        if (pipe(pipefd) == -1) {
            DEBUG_PRINT(("Error creating pipe\n"));
            pipe_error();
            return -1;
        }

        /* Hook the output to the new pipe, now this command is piped up */
        list_entry(command, struct esh_command, elem)->output_fd = pipefd[1];

        /* Nothing more will be done on this command, GOGOGOGOG */
        if (esh_command_exec(list_entry(command, struct esh_command, elem), pipeline->pgrp) == -1) {
            /* Something went wrong */
            DEBUG_PRINT(("Error on esh_command_exec in loop: %s", list_entry(command, struct esh_command, elem)->argv[0]));
            return -1;
        }

        if (close(list_entry(command, struct esh_command, elem)->input_fd) == -1) {
            DEBUG_PRINT(("Error after close\n"));
            close_error();
            return -1;
        }
    }
    /* Close the last commands output */
    if (close(list_entry(command, struct esh_command, elem)->input_fd) == -1) {
        DEBUG_PRINT(("Error after close\n"));
        close_error();
        return -1;
    }
    /* This is the last command */
    command = list_next(command);
    
    if (esh_command_exec(list_entry(command, struct esh_command, elem), pipeline->pgrp) == -1) {
        return -1;
    }

    /* Close the last commands output */
    if (close(list_entry(command, struct esh_command, elem)->input_fd) == -1) {
        DEBUG_PRINT(("Error after close\n"));
        close_error();
        return -1;
    }
    /* Close if it's a file */
    if (pipeline->iored_output) {
        if (close(list_entry(command, struct esh_command, elem)->output_fd) == -1) {
            DEBUG_PRINT(("Error after close\n"));
            close_error();
            return -1;
        }
    }
    return 0;
}

/* 
 * Forking 
 * Parent: close fd, setgpid,
 * Child : dup2, exec
 */
pid_t esh_command_exec(struct esh_command* command, pid_t pgid) {
    command->pid = fork();
    if (command->pid == -1) {
        DEBUG_PRINT(("Error forking job %s\n", command->argv[0]));
        fork_error();
        return -1;
    }
    else if (command->pid != 0) {
        /* This is the parent and will be the shell */
        /* setpgid */
        if (pgid == 0) {
            if (setpgid(command->pid, command->pid) == -1) {
                DEBUG_PRINT(("Error in setpgid\n"));
                setpgid_error();
                return -1;
            }
            pgid = command->pid;
        }
        else {
            if (setpgid(command->pid, pgid) == -1) {
                DEBUG_PRINT(("Error in setpgid\n"));
                setpgid_error();
                return -1;
            }
        }
    }
    else {
        /* This is the child and will only be reached by the parent if fork
         * didn't return -1 */
        /* disable signal handling for child */

        /* Could probably handle EINTR also check to make sure you're not
         * trying to dup2 itself */
        if (command->input_fd != STDIN_FILENO && dup2(command->input_fd,
                    STDIN_FILENO) == -1) {
            return -1;
        }
        if (command->output_fd != STDOUT_FILENO && dup2(command->output_fd,
                    STDOUT_FILENO) == -1) {
            return -1;
        }
        /* No longer need these open might as well close for cleaniness */
        if (close(command->input_fd) == -1) {
            DEBUG_PRINT(("Error after close\n"));
            close_error();
            return -1;
        }
        if (close(command->output_fd) == -1) {
            DEBUG_PRINT(("Error after close\n"));
            close_error();
            return -1;
        }
        
        /* Should be good lets actually run the program now */
        execvp(command->argv[0], command->argv);
        /* if this returns something is messed up, not sure how to let the main
         * shell know */
        DEBUG_PRINT(("Error trying to execvp\n"));
        execvp_error();
        return -1;

    }
    /* This return statement allows just passing the return of this function to
     * every other function */
    return pgid;
}
