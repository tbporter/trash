#ifndef _ESH_SIGNAL_H
#define _ESH_SIGNAL_H

void esh_signal_init(void);
void esh_signal_handler_int(int sig);
void esh_signal_handler_stop(int sig);
void esh_signal_handler_chld(int sig);
void esh_signal_fg(int sig);
void esh_signal_kill_pgrp(pid_t pgrp, int sig);
void esh_signal_kill_pgrp_stop(pid_t pgrp);
void esh_signal_kill_pgrp_int(pid_t pgrp);
#endif