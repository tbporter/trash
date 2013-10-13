#ifndef _ESH_SIGNAL_H
#define _ESH_SIGNAL_H

void esh_signal_init(void);
void esh_signal_handler_int(int sig);
void esh_signal_handler_stop(int sig);
void esh_signal_handler_chld(int sig);
void esh_signal_fg(int sig);
void esh_signal_kill_pgrp(pid_t pgrp, int sig);
bool esh_signal_check_pipeline_isempty(struct esh_pipeline* pipeline);
#endif