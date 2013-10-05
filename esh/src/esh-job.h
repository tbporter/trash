#ifndef _ESH_JOB_H
#define _ESH_JOB_H
int esh_command_line_run(struct esh_command_line * cline);
int esh_pipeline_init(struct esh_pipeline * pipeline);
int esh_pipeline_run(struct esh_pipeline* pipeline);
pid_t esh_command_exec(struct esh_command* command, pid_t pgid);
#endif
