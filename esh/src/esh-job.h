#ifndef _ESH_JOB_H
#define _ESH_JOB_H
struct esh_jobs {
    struct list/* <esh_pipeline> */ jobs;
    struct esh_pipeline* fg_job;
    int current_jid;
};
void esh_jobs_init(void);
struct list* esh_get_jobs(void);
struct esh_pipeline* esh_get_job_from_jid(int jid);
struct esh_pipeline* esh_get_job_from_pgrp(pid_t pgrp);
struct esh_command* esh_get_cmd_from_pid(pid_t pid);

/* Running stuff */
int esh_command_line_run(struct esh_command_line * cline);
int esh_pipeline_init(struct esh_pipeline * pipeline);
int esh_pipeline_run(struct esh_pipeline* pipeline);
pid_t esh_command_exec(struct esh_command* command, pid_t pgid);
#endif
