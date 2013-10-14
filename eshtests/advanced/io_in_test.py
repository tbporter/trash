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

#create a file herp.txt with 'derp' in it
c.sendline("echo derp > herp.txt")
c.sendline("cat herp.txt")
assert c.expect_exact("derp") == 0, "file didn't recieve out correctly"

#give cat the file herp.txt
c.sendline("cat < herp.txt")
assert c.expect_exact("derp") == 0, "file input didn't work"

#give cat a nonexistant file
c.sendline("cat < ffdsaffdsafjasfsafdsadfsaf")
assert c.expect_exect("fopen error: No such file or directory") == 0, "did not handle incorrect file correctly"


shellio.success()
