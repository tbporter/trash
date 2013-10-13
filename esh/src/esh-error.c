#include <errno.h>
#include <unistd.h>
#include "esh-error.h"
#include "esh-debug.h"
/* These functions handle the corresponding syscall's errors and print an
 * appropriate message. */
void open_error() {
    DEBUG_PRINT(("esh-error.c:open_error() not yet implemented\n"));
    switch (errno) {
        case EACCES:    DEBUG_PRINT(("Permission denied\n"));
                        break;
        case EDQUOT:    DEBUG_PRINT(("Hit block quota\n"));
                        break;
        case EFAULT:    DEBUG_PRINT(("Outside accessible space\n"));
                        break;
        case EINTR:     DEBUG_PRINT(("open interrupted by signal\n"));
                        break;
    }
}

void close_error() {
    DEBUG_PRINT(("esh-error.c:close_error() not yet implemented\n"));
}

void pipe_error() {
    DEBUG_PRINT(("esh-error.c:pipe_error() not yet implemented\n"));
}

void fork_error() {
    DEBUG_PRINT(("esh-error.c:pipe_error() not yet implemented\n"));
}

void setpgid_error() {
    DEBUG_PRINT(("esh-error.c:setpgid_error() not yet implemented\n"));
}

void dup2_error() {
    switch (errno) {
        case EBADF: DEBUG_PRINT(("oldfd isn't an open file descriptor.\n"));
                    break;
        case EBUSY: DEBUG_PRINT(("open and dup race condition\n"));
                    break;
        case EINTR: DEBUG_PRINT(("Interrupted by signal\n"));
                    break;
        case EINVAL:DEBUG_PRINT(("Invalid flags or oldfd equal to newfd\n"));
                    break;
        case EMFILE:DEBUG_PRINT(("Maximum number of file descriptors open\n"));
                    break;
    }
}

void execvp_error() {
    DEBUG_PRINT(("esh-error.c:execvp_error() not yet implemented\n"));
}

void waitpid_error() {
    DEBUG_PRINT(("esh-error.c:waitpid_error() not yet implemented\n"));
}

void chdir_error() {
    DEBUG_PRINT(("esh-error.c:chdir_error() not yet implemented\n"));
}

void tcsetpgrp_error() {
    DEBUG_PRINT(("esh-error.c:tcsetpgrp_error() not yet implemented\n"));
}

void kill_error() {
    DEBUG_PRINT(("esh-error.c:kill_error() not yet implemented\n"));
}
