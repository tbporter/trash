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

#echo | grep | grep
#2nd grep will return 'much pipe', and not 'much wow', because of the first grep
c.sendline("echo -e \"wow\nso pipe\nvery echo\nmuch pipe\nwow\nmuch wow\" | grep pipe | grep much")
assert c.expect_exact("much pipe") == 0, "didn't multi-pipe to grep correctly"

shellio.success()
