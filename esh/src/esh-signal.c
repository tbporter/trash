#include "esh.h"
#include "esh-job.h"
#include "esh-signal.h"
#include <signal.h>
#include "esh-debug.h"
#include "esh-error.h"
#include "esh-sys-utils.h"
#include <stdio.h>
#include <assert.h>
#include <sys/wait.h>
#include <unistd.h>

struct esh_jobs jobs;

//Initialize all the signal handlers
void esh_signal_init(){
	signal(SIGTSTP,esh_signal_handler_stop);
	signal(SIGINT,esh_signal_handler_int);
	esh_signal_sethandler(SIGCHLD, esh_signal_handler_chld);
	esh_signal_block(SIGTTIN);
	esh_signal_block(SIGTTOU);
}

//Ctrl-c
void esh_signal_handler_int(int sig){
	assert (sig == SIGINT);
	esh_signal_fg(sig);
}
//ctrl-z
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

	//wait on each pid to see what happened
	while((pid = waitpid(-1,&status, WUNTRACED | WNOHANG)) > 0){
		
		//finds the cmd based on the pid
		struct esh_command* cmd = esh_get_cmd_from_pid(pid);
			
		if(cmd){
			esh_signal_cleanup(cmd,status);;
		}	
	
	}
}


//will change the status of a esh_command and kill the pipe if it needs to 
bool esh_signal_cleanup(struct esh_command* cmd, int status){

	
	//It ended! set the pid to 0, so the pipe cleaner knows what to do
	if(WIFSIGNALED(status) || WIFEXITED(status)){
		cmd->pid = 0;
	} 
	//It stopped
	else if(WIFSTOPPED(status)){

		cmd->pipeline->status = STOPPED;

		//If it needs terminal
		int stopsig = WSTOPSIG(status);
		if(stopsig == SIGTTIN || stopsig == SIGTTOU){
			cmd->pipeline->status = NEEDSTERMINAL;
		}
	}
	//It continued, lets set it to background
	else if(WIFCONTINUED(status)){
		if(jobs.fg_job != NULL && jobs.fg_job->pgrp != cmd->pipeline->pgrp){
			cmd->pipeline->status = BACKGROUND;
		}
	} 
	else{
	}

	//If all the pipe's cmds are dead, kill the pipe
	if(esh_signal_check_pipeline_isempty(cmd->pipeline)){
		
		if(jobs.fg_job != NULL && jobs.fg_job->pgrp == cmd->pipeline->pgrp){
			jobs.fg_job = NULL;
		}
		//lets "pop pop"
		list_remove(&(cmd->pipeline->elem));
		esh_pipeline_free(cmd->pipeline);
		return 1;
	}
	return 0;
}

//If the whole pipeline has a pid's of 0, return 1.
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
