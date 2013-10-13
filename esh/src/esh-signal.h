#ifndef _ESH_SIGNAL_H
#define _ESH_SIGNAL_H

#include "esh-sys-utils.h"
void esh_signal_init(void);
void esh_signal_handler_int(int);
void esh_signal_handler_stop(int);
void esh_signal_handler_chld(int, siginfo_t *, void *);
void esh_signal_fg(int sig);
void esh_signal_kill_pgrp(pid_t pgrp, int sig);
bool esh_signal_check_pipeline_isempty(struct esh_pipeline* pipeline);
bool esh_signal_cleanup_fg(struct esh_command* cmd, int status);
bool esh_signal_cleanup(struct esh_command* cmd, int status);
#endif