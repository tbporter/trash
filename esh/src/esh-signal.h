#ifndef _ESH_SIGNAL_H
#define _ESH_SIGNAL_H

#include "esh-sys-utils.h"
void esh_signal_init(void);
void esh_signal_handler_int(int, siginfo_t *, void *);
void esh_signal_handler_stop(int, siginfo_t *, void *);
void esh_signal_handler_chld(int, siginfo_t *, void *);
void esh_signal_fg(int sig);
void esh_signal_kill_pgrp(pid_t pgrp, int sig);
bool esh_signal_check_pipeline_isempty(struct esh_pipeline* pipeline);
#endif