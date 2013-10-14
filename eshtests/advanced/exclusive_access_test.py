#!/usr/bin/python
#
# Block header comment
#
#
import sys, imp, atexit
sys.path.append("/home/courses/cs3214/software/pexpect-dpty/");
import pexpect, shellio, signal, time, os, re, proc_check

#Ensure the shell process is terminated
def force_shell_termination(shell_process):
	c.close(force=True)

#pulling in the regular expression and other definitions
definitions_scriptname = sys.argv[1]
def_module = imp.load_source('', definitions_scriptname)
logfile = None
if hasattr(def_module, 'logfile'):
    logfile = def_module.logfile

#spawn an instance of the shell
c = pexpect.spawn(def_module.shell, drainpty=True, logfile=logfile)

atexit.register(force_shell_termination, shell_process=c)

#backgrounding a exclusive access job will be stopped
c.sendline("vim &")
(jobid, status_message, command_line) = shellio.parse_regular_expression(c, def_module.job_status_regex)
assert status_message == def_module.jobs_status_msg['stopped'] and \
		'vim' in command_line, "Vim did not stop when backgrounded"

#c.sendline("fg 1")
#c.sendline()
shellio.success()
