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

c.timeout = 5

#backgrounding a exclusive access job will be stopped
c.sendline("vim &")
[job, pid] = shellio.parse_regular_expression(c,def_module.bgjob_regex)
assert not proc_check.check_pid_fgpgrp(pid), "process is in the forground"

c.sendline("fg 1")
c.sendline("iderp"+chr(27)+":w derpderp")
c.sendline("cat derpderp")
assert c.expect_exact("derp") == 0, "typing to vim didn't work"

shellio.success()
