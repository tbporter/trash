trash
=====

best shell

Compiling
=========

Needs flex 2.5.36 or below, without it it generates warnings which are turned into errors through Werror. Install it and make sure libl.so links to libfl.a and lex links to flex.

Notes
=====

Need to handle SIGCHLD, in a queing system.
 * List is already implemented in list.c
 * Note cases surrounding SIGTIN and SIGTOUT

SIGINT, SIGSTOP should be handled by a different handler
 * It should check for foreground process and pass it along
 * Else ignore

Terminal information
 * There are already implemented tty functions in esh-sys-utils
 * This is important for background and foreground tricks
 * Will be needed in sigint and sigstop handling.
