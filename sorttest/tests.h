/* 
 * tests.h -- test framework
 *
 * Ted Baker, FSU, 2010
 * This is a work in progress.
 *
 * ($Id$)
 */

#ifndef TESTS_H
#define TESTS_H

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>

#define RECOVER {exit (-1);}

#define CHECKIO(call, len) {int result; if ((result=(call)) != len) {\
      fprintf (stderr, " %s.%s * incomplete I/O : %s => %d : %s\n", test_name, step_name, #call, result, strerror (errno)); RECOVER;}}

#define CHECK(call) {int result; if ((result=(call)) == -1) {\
      fprintf (stderr, " %s.%s * failed %s %s\n", test_name, step_name, #call, strerror(errno)); RECOVER;}}

#define CHECKPTR(call) {void *result; if ((result=(call)) == (void *) -1) {\
      fprintf (stderr, " %s.%s * failed %s : %s\n", test_name, step_name, #call, strerror(errno)); RECOVER;}}

#define ASSERT(condition) { if (! (condition)) {\
      fprintf (stderr, " %s.%s * failed %s\n", test_name, step_name, #condition); RECOVER;}}

#define MESSAGE(format, arg) {\
    if (verbosity > 1) fprintf (stderr, " %s.%s : " format "\n", test_name, step_name, arg);}

#define FAILURE(format, arg) {fprintf (stderr, " %s.%s * failed " format " : %s\n",\
                                       test_name, step_name, arg, strerror (errno));}

#define ERROR(msg) {fprintf (stderr, " %s.%s * %s \n", test_name, step_name, msg);}

#define PUT_STATUS(buf,len,status) {\
          if (WIFEXITED (status)) CHECK (snprintf (buf, len, "exited with status %d", WEXITSTATUS (status)))\
          else if (WIFSIGNALED (status)) CHECK (snprintf (buf, len, "terminated by signal %s", strsignal (WTERMSIG (status))))\
          else if (WIFSTOPPED (status)) CHECK (snprintf (buf, len, "stopped by signal %s", strsignal (WSTOPSIG (status))))\
          else if (WIFCONTINUED (status)) CHECK (snprintf (buf, len, "continued by SIGCONT")) \
          else CHECK (snprintf (buf, len, "invalid status value"))}

int verbosity = 1;
const char *section_name = "*"; 
const char *test_name = "*"; 
const char *step_name = "*"; 

#define NEW_DEVICE(name) {\
        if (verbosity > 0) fprintf (stderr, " ===== device : %s =====\n", name);\
	section_name = name;}

#define NEW_TEST(name, msg) {\
        if (verbosity > 0) fprintf (stderr, " %s === %s ===\n", #name, msg);\
	test_name = #name;}

#define EXTRATEST(msg) {if (verbosity > 0) fprintf (stderr, " ====== %s ===\n", msg);}

#define NEW_STEP(name, msg) {\
        if (verbosity > 0) fprintf (stderr, " %s.%s : %s\n", test_name, #name, msg);\
	step_name = #name;}

#define GET_VERBOSITY {int i;\
        if (argc > 1) {\
                for (i = 1; i < argc; i++) {\
                        if (! strncmp (argv[i], "-q", 2)) verbosity = 0;\
                        else if (! strncmp (argv[i], "-v", 2)) verbosity = 10;\
                }}}

#endif
