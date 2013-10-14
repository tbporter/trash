#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>

#include "esh-debug.h"
#include "list.h"

#include "esh.h"
#include "esh-sys-utils.h"
#include "esh-job.h"
#include "esh-error.h"
#include "esh-macros.h"
#include "esh-builtin.h"
#include "esh-signal.h"

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
    struct list_elem* pipeline_elem;
    int count = 0;
    DEBUG_PRINT(("List size %d of %d\n", list_size(&jobs.jobs), pid));
    for (pipeline_elem = list_front(&jobs.jobs); pipeline_elem !=
            list_tail(&jobs.jobs); pipeline_elem = list_next(pipeline_elem)) {
        count++;
        struct esh_pipeline* pipeline = list_entry(pipeline_elem, struct esh_pipeline, elem);
        int count2 = 0;
        struct list_elem* command_elem;
        for (command_elem = list_front(&pipeline->commands); command_elem !=
                list_tail(&pipeline->commands); command_elem = list_next(command_elem)) {
            struct esh_command* command = list_entry(command_elem, struct esh_command, elem);
            count2++;

            if (command->pid == pid) {
                DEBUG_PRINT(("Found pid of command %d\n", pid));
                return command;
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
    while (!list_empty(&cline->pipes)) {
        struct list_elem* pipeline_elem = list_pop_front(&cline->pipes);
        struct esh_pipeline* pipeline = list_entry(pipeline_elem, struct esh_pipeline, elem);
        /* run it if it's a builtin */
        int builtin = esh_builtin(pipeline);
        if (builtin == -1) {
            return -1;
        }
        else if (builtin) {
            DEBUG_PRINT(("builtin executed, continuing\n"));
            /* If this was handled by a builtin carry on but clean up since
             * signals won't do that */
            esh_pipeline_free(pipeline);
            continue;
        }
        else if (!list_empty(&esh_plugin_list) && list_front(&pipeline->commands) == list_back(&pipeline->commands)){
            struct esh_command* command = list_entry(list_front(&pipeline->commands),
                    struct esh_command, elem);
            struct list_elem* plugin_elem = list_front(&esh_plugin_list);
            bool success = false;
            DEBUG_PRINT(("Checking plugins for builtins\n"));
            for (; plugin_elem != list_tail(&esh_plugin_list); plugin_elem =
                    list_next(plugin_elem)) {
                struct esh_plugin* plugin = list_entry(plugin_elem, struct
                        esh_plugin, elem);
                if (plugin->process_builtin != NULL && plugin->process_builtin(command)) {
                    DEBUG_PRINT(("Found builtin\n"));
                    esh_pipeline_free(pipeline);
                    success = true;
                    break;
                }
            }
            if (success) continue;
        }

        /* Protect the list while we access the job list */
        esh_signal_block(SIGCHLD);
        list_push_back(&jobs.jobs, pipeline_elem);

        /* Set up signal queuing */
        DEBUG_PRINT(("Initilizing pipeline\n"));
        RetError(esh_pipeline_init(pipeline));
        DEBUG_PRINT(("Executing pipeline\n"));
        RetError(esh_pipeline_run(pipeline));

        /* If foreground job */
        if (!pipeline->bg_job) {
            int status;

            DEBUG_PRINT(("tty_fd %d\n", esh_sys_tty_getfd()));
            if (tcsetpgrp(esh_sys_tty_getfd(), pipeline->pgrp) == -1) {
                DEBUG_PRINT(("Error on tcsetpgrp\n"));
                tcsetpgrp_error();
                return -1;
            }
            pipeline->status = FOREGROUND;
            jobs.fg_job = pipeline;
            DEBUG_PRINT(("fg_job->pgrp: %d\n", jobs.fg_job->pgrp));         
            /* Run queue */
            esh_signal_unblock(SIGCHLD);
            /* Wait on job */
            pid_t pid;
            bool running = 1;
            while (running) {
                if ((pid = waitpid(jobs.fg_job->pgrp, &status, WUNTRACED)) == -1) {
                    DEBUG_PRINT(("Failed on waitpid on fg job\n"));
                    waitpid_error();
                    return -1;
                }
                else if (pid > 0 && !WIFSTOPPED(status)) {
                    /* clean up */
                    struct esh_command* cmd = esh_get_cmd_from_pid(pid);
                    running = !esh_signal_cleanup_fg(cmd, status);
                }
                else {
                    break;
                }
            }
            /* Lock down our stuff */
            esh_signal_block(SIGCHLD);
            DEBUG_PRINT(("Finished waiting, return of %d\n", pid));
            /* Clear the foreground job */
            jobs.fg_job = NULL;
            /* Reclaim control of the terminal */
            if (tcsetpgrp(esh_sys_tty_getfd(), getpgrp()) == -1) {
                DEBUG_PRINT(("Error on tcsetpgrp\n"));
                tcsetpgrp_error();
                return -1;
            }
            if (WIFSTOPPED(status)) {
                /* It's a background job! */
                esh_sys_tty_save(&pipeline->saved_tty_state);
                pipeline->bg_job = true;
            }
            esh_sys_tty_restore(tty_state);
            DEBUG_PRINT(("Reclaimed terminal\n"));
        }
        /* If background job */
        else {
            pipeline->status = BACKGROUND;
            printf("[%d] (%d)\n", pipeline->jid, pipeline->pgrp);
            DEBUG_PRINT(("Setting up backgrounding\n"));
        }
        esh_signal_unblock(SIGCHLD);
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
    DEBUG_PRINT(("Setting jid to %d\n", jobs.current_jid + 1));
    pipeline->jid = ++jobs.current_jid;

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
            if (!command->pipeline->bg_job) {
                /* TODO: Get this working! */
                DEBUG_PRINT(("This should get the terminal!\n"));
                /*if (tcsetpgrp(esh_sys_tty_getfd(), pgid) == -1) {
                    tcsetpgrp_error();
                    return -1;
                }*/
            }
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
        if (pgid == 0) {
            setpgrp();
        }
        /* Could probably handle EINTR also check to make sure you're not
         * trying to dup2 itself */
        if (command->input_fd != STDIN_FILENO && dup2(command->input_fd,
                    STDIN_FILENO) == -1) {
            DEBUG_PRINT(("Error on setting input\n"));
            dup2_error();
            return -1;
        }
        DEBUG_PRINT(("Setting child output\n"));
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
        /* Something want horribly wrong */
        exit(EXIT_FAILURE);

    }
    /* This return statement allows just passing the return of this function to
     * every other function */
    return pgid;
}
