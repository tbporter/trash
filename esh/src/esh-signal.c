#include "esh.h"
#include "esh-job.h"
#include "esh-signal.h"
#include <signal.h>
#include "esh-debug.h"
#include "esh-error.h"
#include "esh-sys-utils.h"

#include <assert.h>
#include <sys/wait.h>
#include <unistd.h>

struct esh_jobs jobs;

void esh_signal_init(){
	signal(SIGTSTP,esh_signal_handler_stop);
	signal(SIGINT,esh_signal_handler_int);
	esh_signal_sethandler(SIGCHLD, esh_signal_handler_chld);
	esh_signal_block(SIGTTIN);
	esh_signal_block(SIGTTOU);
}
void esh_signal_handler_int(int sig){
	assert (sig == SIGINT);
	esh_signal_fg(sig);
}

void esh_signal_handler_stop(int sig){
	assert (sig == SIGTSTP);
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

void esh_signal_handler_chld(int sig, siginfo_t* info, void* _ctxt){
	assert (sig == SIGCHLD);

	int status;
	pid_t pid;
	struct esh_command* cmd;

		//tcsetpgrp() for forground

	//wait on each pid to see what happened
	while((pid = waitpid(-1,&status, WNOHANG) > 0)){

		cmd = esh_get_cmd_from_pid(pid);
		if(jobs.fg_job->pgrp == pid){
			//write(0,"fg job signal",);
			if(WIFEXITED(status) || WIFSTOPPED(status)){
				//write(0,"fg job signal: exit or stopped");
				tcsetpgrp(esh_sys_tty_getfd(),getpgrp());
				jobs.fg_job = NULL;
			}
		}
		
		if(WIFEXITED(status)){
			//list_remove(cmd->elem);
		} else if(WIFSIGNALED(status)) {
			DEBUG_PRINT(("killed by signal %d\n", WTERMSIG(status)));
		} else if(WIFSTOPPED(status)) {
			cmd->pipeline->status = STOPPED;
			
			int stopsig = WSTOPSIG(status);
			if(stopsig == SIGTTIN || stopsig == SIGTTOU){
				cmd->pipeline->status = NEEDSTERMINAL;
			}
			DEBUG_PRINT(("stopped by signal %d\n", stopsig));
		} else if(WIFCONTINUED(status)) {
			//printf("continued\n");
		} else{

		}
		if(esh_signal_check_pipeline_isempty(cmd->pipeline)){
			list_remove(&(cmd->pipeline->elem));
			esh_pipeline_free(cmd->pipeline);
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
