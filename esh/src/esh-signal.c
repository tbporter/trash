#include "esh.h"
#include "esh-job.h"
#include "esh-signal.h"
#include "signal.h"
#include "esh-debug.h"
#include "esh-error.h"
#include "esh-sys-utils.h"

#include <sys/wait.h>
#include <unistd.h>

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
	//
	if(jobs.fg_job != NULL){
		esh_signal_kill_pgrp(jobs.fg_job->pgrp,sig);
	}
	else{
		DEBUG_PRINT(("No fg job to send signal: %d to\n",sig));
	}
}

void esh_signal_handler_chld(int sig){
	int status;
	pid_t pid;
	struct esh_command* cmd;

		//tcsetpgrp() for forground

	//wait on each pid to see what happened
	while((pid = waitpid(-1,&status, WNOHANG) > 0)){

		cmd = esh_get_cmd_from_pid(pid);
		if(jobs.fg_job->pgrp == pid){
			if(WIFEXITED(status) || WIFSTOPPED(status)){
				tcsetpgrp(esh_sys_tty_getfd(),getpgrp());
				jobs.fg_job = NULL;
			}
		}
		/*
		if(WIFEXITED(status)){
			list_remove(cmd->elem);
		} else if(WIFSIGNALED(status)) {
			//printf("killed by signal %d\n", WTERMSIG(status));
		} else if(WIFSTOPPED(status)) {
			cmd->pipeline->status = STOPPED;
			printf("stopped by signal %d\n", WSTOPSIG(status));
		} else if(WIFCONTINUED(status)) {
			//printf("continued\n");
		}*/
		if(esh_signal_check_pipeline_isempty(cmd->pipeline)){
			
			//list_remove()
			//free pipe memory
		}

	}
}

bool esh_signal_check_pipeline_isempty(struct esh_pipeline* pipeline){
	struct list_elem *i;
    for (i = list_begin(&(pipeline->commands)); i != list_end(&(pipeline->commands)); i = list_next(i)) {
    	struct esh_command* cmd = list_entry(i, struct esh_command, elem);
    	if(cmd->pid != 0)
    		return 0;
    }
    return 1;
}

void esh_signal_kill_pgrp(pid_t pgrp, int sig){
	DEBUG_PRINT(("Killing pgrp: %d with signal: %d\n", pgrp, sig));
	kill(-1*pgrp,sig);
}
