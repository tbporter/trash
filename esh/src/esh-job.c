#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>

#include "esh-debug.h"
#include "list.h"

#include "esh.h"
#include "esh-job.h"
#include "esh-error.h"
#include "esh-macros.h"
#include "esh-builtin.h"

struct esh_jobs jobs;

void esh_jobs_init() {
    list_init(&jobs.jobs);
    jobs.current_jid = 0;
    jobs.fg_job = NULL;
}
/* Expose the jobs in here to other files */
struct list* esh_get_jobs() {
    return &jobs.jobs;
}

/* Return pointer to pipeline or NULL */
struct esh_pipeline* esh_get_job_from_jid(int jid) {
    struct list_elem* pipeline;
    for (pipeline = list_front(&jobs.jobs); pipeline !=
            list_tail(&jobs.jobs); pipeline = list_next(pipeline)) {
        if (list_entry(pipeline, struct esh_pipeline, elem)->jid == jid) {
            return list_entry(pipeline, struct esh_pipeline, elem);
        }
    }
    return NULL;
}

/* Return pointer to pipeline or NULL */
struct esh_pipeline* esh_get_job_from_pgrp(pid_t pgrp) {
    struct list_elem* pipeline;
    for (pipeline = list_front(&jobs.jobs); pipeline !=
            list_tail(&jobs.jobs); pipeline = list_next(pipeline)) {
        if (list_entry(pipeline, struct esh_pipeline, elem)->pgrp == pgrp) {
            return list_entry(pipeline, struct esh_pipeline, elem);
        }
    }
    return NULL;
}

/* Return pointer to pipeline or NULL */
struct esh_command* esh_get_cmd_from_pid(pid_t pid) {
    struct list_elem* pipeline;
    for (pipeline = list_front(&jobs.jobs); pipeline !=
            list_tail(&jobs.jobs); pipeline = list_next(pipeline)) {
        struct list_elem* command;
        for (command = list_front(&list_entry(pipeline, struct esh_pipeline,
                        elem)->commands); command !=
                list_tail(&list_entry(pipeline, struct esh_pipeline,
                        elem)->commands); list_next(command)) {
            if (list_entry(command, struct esh_command, elem)->pid == pid) {
                return list_entry(pipeline, struct esh_command, elem);
            }
        }
    }
    return NULL;
}

/*
 * Execute the command line and the pipes
 */
int esh_command_line_run(struct esh_command_line * cline) {

    /* Run through each pipeline */
    struct list_elem* pipeline;
    for (pipeline = list_front(&cline->pipes);pipeline !=
            list_tail(&cline->pipes); pipeline = list_next(pipeline)) {
        /* run it if it's a builtin */
        if (esh_builtin(list_entry(pipeline, struct esh_pipeline, elem))) {
            /* If this was handled by a builtin carry on but clean up since
             * signals won't do that */
            struct list_elem* new_pipeline = list_prev(pipeline);
            list_remove(pipeline);
            esh_pipeline_free(list_entry(pipeline, struct esh_pipeline, elem));
            pipeline = new_pipeline;
            continue;
        }

        /* Set up signal queuing */
        DEBUG_PRINT(("Initilizing pipeline\n"));
        RetError(esh_pipeline_init(list_entry(pipeline, struct esh_pipeline, elem)));
        DEBUG_PRINT(("Executing pipeline\n"));
        RetError(esh_pipeline_run(list_entry(pipeline, struct esh_pipeline, elem)));

        /* If foreground job */
        if (!list_entry(pipeline, struct esh_pipeline, elem)->bg_job) {
            int status;
            DEBUG_PRINT(("Waiting on %s %d\n",
                        list_entry(list_back(&list_entry(pipeline, struct
                                    esh_pipeline, elem)->commands), struct
                            esh_command, elem)->argv[0],
                        list_entry(list_back(&list_entry(pipeline, struct
                                    esh_pipeline, elem)->commands), struct
                            esh_command, elem)->pid));
            /* After that ugly debugging statement... */
            jobs.fg_job = list_entry(pipeline, struct esh_pipeline, elem);
            DEBUG_PRINT(("fg_job->pggrp: %d\n",jobs.fg_job->pgrp));         
            list_entry(pipeline, struct esh_pipeline, elem)->status = FOREGROUND;
            /* Run queue */
            /* TODO: signal_queue_process */
            if (waitpid(list_entry(list_back(&list_entry(pipeline, struct
                                    esh_pipeline, elem)->commands), struct
                            esh_command, elem)->pid, &status, WUNTRACED) == -1)
            {
                DEBUG_PRINT(("Error on waitpid\n"));
                waitpid_error();
            }
            jobs.fg_job = NULL;
            /* If process is still running, just bg */
            if (WIFSTOPPED(status)) {
                DEBUG_PRINT(("Process stopped in background\n"));
                list_push_back(&jobs.jobs, pipeline);
            }

            DEBUG_PRINT(("Finished waiting\n"));
            /* TODO: Add code to use status to determine if this process group was
             * stopped or needs to be killed tervis return 1 if it's
             * backgrounded and 0 if it's not*/
        }
        /* If background job */
        else {
            list_entry(pipeline, struct esh_pipeline, elem)->status = BACKGROUND;
            DEBUG_PRINT(("Setting up backgrounding\n"));
            list_push_back(&jobs.jobs, pipeline);
            /* Run queue */
            /*signal_queue_process*/
        }
    }

    return 0;
}

/*
 * Right now this only inits only iored
 * returns 0 on success and -1 on failure and calls appropriate error function.
 */
int esh_pipeline_init(struct esh_pipeline * pipeline) {
    /* Assume list is in correct state and non-empty (needed to perform
     * list_front) */

    /* Set up jids */
    if (pipeline->bg_job) {
        DEBUG_PRINT(("Setting jid to %d\n", jobs.current_jid + 1));
        pipeline->jid = ++jobs.current_jid;
    }
    else {
        DEBUG_PRINT(("Foreground job, setting jid to -1\n"));
        pipeline->jid = -1;
    }

    /* IO redirection init */
    DEBUG_PRINT(("Executing pipeline iored code\n"));
    /* Execute if not NULL */
    if (pipeline->iored_input) {
        /* Do we need to free the file pointer here? */
        int input_fd = fileno(fopen(pipeline->iored_input, "r"));
        DEBUG_PRINT(("Setting input to be %s\n", pipeline->iored_input));
        if (input_fd == -1) {
            DEBUG_PRINT(("Error opening input file %s\n", pipeline->iored_input));
            /* open_error should switch on errno and print and appropriate
             * message */
            open_error();
            return -1;
        }
        list_entry(list_front(&pipeline->commands), struct esh_command, elem)->input_fd = input_fd;
    }
    else {
        /* If we set this here we can treat io redirection and
         * non-ioredirection the same later on */
        list_entry(list_front(&pipeline->commands), struct esh_command, elem)->input_fd = STDIN_FILENO;
    }

    /* Again for output, but slightly different*/
    if (pipeline->iored_output) {
        DEBUG_PRINT(("Setting output to be %s\n", pipeline->iored_output));
        int output_fd = fileno(fopen(pipeline->iored_output, (pipeline->append_to_output ? "a" : "w")));
        if (output_fd == -1) {
            open_error();
            return -1;
        }
        list_entry(list_back(&pipeline->commands), struct esh_command, elem)->output_fd = output_fd;
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
        DEBUG_PRINT(("Only one command found, short cutting\n"));
        pipeline->pgrp =  esh_command_exec(list_entry(command, struct esh_command,
                        elem), 0);
        RetError(pipeline->pgrp);
        /* Close input fd if it's a file */
        if (pipeline->iored_input) {
            DEBUG_PRINT(("Closing %d in parent\n", list_entry(command, struct
                            esh_command, elem)->input_fd));
            if (close(list_entry(command, struct esh_command, elem)->input_fd)
                    == -1) {
                DEBUG_PRINT(("Error after close\n"));
                close_error();
                return -1;
            }
        }
        /* Close if it's a file */
        if (pipeline->iored_output) {
            DEBUG_PRINT(("Closing %d in parent\n", list_entry(command, struct
                            esh_command, elem)->output_fd));
            if (close(list_entry(command, struct esh_command, elem)->output_fd) == -1) {
                DEBUG_PRINT(("Error after close\n"));
                close_error();
                return -1;
            }
        }
        return 0;
    }

    DEBUG_PRINT(("Multiple commands found, running loop\n"));
    
    /* This will handle the multi command case */
    int pipefd[2];
    
    DEBUG_PRINT(("Initializing first pipeline\n"));
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
        DEBUG_PRINT(("Closing %d in parent\n", list_entry(command, struct esh_command, elem)->input_fd));
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
    if (close(list_entry(command, struct esh_command, elem)->output_fd) == -1) {
        DEBUG_PRINT(("Error after close\n"));
        close_error();
        return -1;
    }
    /* This is the last command */
    command = list_next(command);

    list_entry(command, struct esh_command, elem)->input_fd = pipefd[0];
    
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
 * Parent: setgpid,
 * Child : dup2, exec
 */
pid_t esh_command_exec(struct esh_command* command, pid_t pgid) {
    DEBUG_PRINT(("About to fork and exec %s\n", command->argv[0]));
    DEBUG_PRINT(("File descriptors: %d %d\n", command->input_fd, command->output_fd));
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
            DEBUG_PRINT(("Error on setting input\n"));
            dup2_error();
            return -1;
        }
        DEBUG_PRINT(("Setting output\n"));
        if (command->output_fd != STDOUT_FILENO && dup2(command->output_fd,
                    STDOUT_FILENO) == -1) {
            DEBUG_PRINT(("Error on setting output\n"));
            dup2_error();
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
