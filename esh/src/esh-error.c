#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include "esh-error.h"
#include "esh-debug.h"
/* These functions handle the corresponding syscall's errors and print an
 * appropriate message. */
void open_error() {
    perror("open error");
}

void close_error() {
    perror("close error");
}

void pipe_error() {
    perror("pipe error");
}

void fork_error() {
    perror("fork error");
}

void setpgid_error() {
    perror("setpgid error");
}

void dup2_error() {
    perror("dup2 error");
}

void execvp_error() {
    perror("execvp error");
}

void waitpid_error() {
    perror("waitpid error");
}

void chdir_error() {
    perror("chdir error");
}

void tcsetpgrp_error() {
    perror("tcsetpgrp error");
}

void kill_error() {
    perror("kill error");
}

void setpgrp_error() {
    perror("setpgrp error");
}
