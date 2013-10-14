#!/usr/bin/python
#
# Test for rot13
# group394
#
import sys, imp, atexit
sys.path.append("/home/courses/cs3214/software/pexpect-dpty/");
import pexpect, shellio, signal, time, os, re, proc_check

#Ensure the shell process is terminated
def force_shell_termination(shell_process):
	c.close(force=True)

#pulling in the regular expression and other definitions
definitions_scriptname = sys.argv[1]
plugin_dir = sys.argv[2]
def_module = imp.load_source('', definitions_scriptname)
logfile = None
if hasattr(def_module, 'logfile'):
    logfile = def_module.logfile

#spawn an instance of the shell
c = pexpect.spawn(def_module.shell + plugin_dir, drainpty=True, logfile=logfile)

atexit.register(force_shell_termination, shell_process=c)

c.timeout = 5

#test correct input
c.sendline("rot13 derp")
assert c.expect_exact("qrec") == 0, "Incorrect encoding"

c.sendline("rot13 -n 2 derp")
assert c.expect_exact("derp") == 0, "Incorrect encoding"

c.sendline("rot13 -n 3 derp")
assert c.expect_exact("qrec") == 0, "Incorrect encoding"

#test incorrect input
c.sendline("rot13")
assert c.expect_exact("Not enough arguments.") == 0, "Case not handled"

c.sendline("rot13 dafadsfasd adsfsaf afddsaf") 
assert c.expect_exact("Invalid flag.") == 0, "Case not handled"

c.sendline("rot13 -n -3 derp") 
assert c.expect_exact("Need a positive number.") == 0, "Case not handled"

c.sendline("exit");

assert c.expect_exact("exit\r\n") == 0, "Shell output extraneous characters"

shellio.success()
