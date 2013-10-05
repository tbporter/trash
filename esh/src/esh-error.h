#ifndef _ESH_ERROR_H
#define _ESH_ERROR_H
/* Handles an error from open(2) */
void open_error(void);
void close_error(void);
void pipe_error(void);
void fork_error(void);
void setpgid_error(void);
void dup2_error(void);
void execvp_error(void);
void waitpid_error(void);
#endif
