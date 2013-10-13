#include "esh.h"
#include "esh-job.h"
#include "esh-signal.h"
#include "signal.h"
#include "esh-debug.h"
#include "esh-error.h"
struct esh_jobs jobs;

void esh_signal_init(){
	signal(SIGINT, esh_signal_handler_int);
	signal(SIGSTOP, esh_signal_handler_stop);
	signal(SIGCHLD, esh_signal_handler_chld);
}

void esh_signal_handler_int(int sig){
	esh_signal_fg(sig);
}

void esh_signal_handler_stop(int sig){
	esh_signal_fg(sig);
}

void esh_signal_fg(int sig){
	if(jobs.fg_job != NULL){
		esh_signal_kill_pgrp(jobs.fg_job->pgrp,sig);
	}
	else{
		DEBUG_PRINT(("No fg job to send signal: %d to\n",sig));
	}
}

void esh_signal_handler_chld(int sig){
	int status;
	pid_t = pid;
	while((pid = waitpid(-1,&status, WNOHANG) > 0){
		
	}
}

void esh_signal_kill_pgrp_int(pid_t pgrp){
	esh_signal_kill_pgrp(pgrp,SIGINT);
}

void esh_signal_kill_pgrp_stop(pid_t pgrp){
	esh_signal_kill_pgrp(pgrp,SIGSTOP);
}

void esh_signal_kill_pgrp_cont(pid_t pgrp){
	esh_signal_kill_pgrp(pgrp,SIGCONT);
}

void esh_signal_kill_pgrp(pid_t pgrp, int sig){
	DEBUG_PRINT(("Killing pgrp: %d with signal: %d\n", pgrp, sig));
	kill(-1*pgrp,sig);
}
