/**
 * Copyright (C) Generations Linux <bugs@softcraft.org>
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
/*
* gsession - GOULD session manager.
*/
#include "gould.h"	/* common package declarations */
#include "gsession.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <signal.h>
#include <execinfo.h>	/* backtrace declarations */
#include <libgen.h>	/* definitions for pattern matching functions */
#include <sysexits.h>	/* exit status codes for system programs */
#include <sys/prctl.h>	/* operations on a process or thread */
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>

const char *Program = "gsession";
const char *Release = "1.2.1";

FILE *_logstream = 0;	/* depends on getenv(LOGLEVEL) > 0 */
debug_t debug = 1;	/* debug verbosity (0 => none) {must be declared} */
int _stream = -1;	/* stream socket descriptor */
pid_t _instance;	/* singleton process ID */

/**
* prototypes (forward method declarations)
*/
int consign(int connection);
int sessionlog(unsigned short level, const char *format, ...);
int stream_socket(const char *sockname);
void signal_responder(int signum);
pid_t spawn(const char *cmdline);
static char *timestamp(void);


/*
* daemonize
*/
#ifdef DAEMONIZE
static void daemonize(void)
{
  pid_t pid = fork();

  if (pid < 0) {
    perror("fork() failed: %m");
    _exit (EX_OSERR);
  }

  if (pid != 0)		/* not in spawned process - exit */
    _exit (EX_OK);

  if (setsid() < 0) {
    perror("setsid() failed: %m");
    _exit (EX_SOFTWARE);
  }
  //hostpid_init();
} /* <daemonize> */
#endif

/*
* consign - socket stream request delivery
*/
int
consign(int connection)
{
  int nbytes;
  char request[MAX_PATHNAME];

  memset(request, 0, MAX_PATHNAME);
  nbytes = read(connection, request, MAX_PATHNAME);

  if (nbytes < 0)
    perror("reading request from stream socket");
  else if (nbytes > 0) {
    request[nbytes] = 0;

    if (strcmp(request, _GETPID) == 0) {  /* request => pidof( gsession ) */
      sessionlog(1, "pidof( %s ) => %d\n", Program, _instance);
      sprintf(request, "%d\n", _instance);
    }
    else {				  /* request => spawn( <command> ) */
      sessionlog(1, "spawn( %s )\n", request);
      sprintf(request, "%d\n", spawn( request ));
    }
    write(connection, request, strlen(request));
  }
  return nbytes;
} /* </consign> */

/*
* signal_responder - signal handler
*/
void
signal_responder(int signum)
{
  const static debug_t BACKTRACE_SIZE = 69;

  switch (signum) {
    case SIGHUP:
    case SIGCONT:
      break;

    case SIGCHLD:		/* reap children */
      while (waitpid(-1, NULL, WNOHANG) > 0) ;
      break;

    case SIGTTIN:
    case SIGTTOU:
      sessionlog(1, "%s: caught signal %d, ignoring.\n", __func__, signum);
      break;

    default:
      close(_stream);
      unlink(_GSESSION);
    
      if (signum != SIGINT && signum != SIGTERM) {
        void *trace[BACKTRACE_SIZE];
        int nptrs = backtrace(trace, UNIX_PATH_MAX);

        printf("%s, exiting on signal: %d\n", Program, signum);
        backtrace_symbols_fd(trace, nptrs, STDOUT_FILENO);
      }

      if (debug) {
        sessionlog(1, "%s ended on %s\n", Program, timestamp());
        fclose(_logstream);
      }
      _exit (signum);
  }
} /* </signal_responder> */

/*
* sessionlog - write to _logstream when debug >= {level}
*/
int
sessionlog(unsigned short level, const char *format, ...)
{
  if(! _logstream) return -1;	/* sanity check - return if no _logstream */

  va_list args;
  va_start (args, format);

  if (debug >= level) {
    vfprintf(_logstream, format, args);
  }
  va_end (args);

  return fflush(_logstream);
} /* </sessionlog> */

/*
* stream_socket
*/
int
stream_socket(const char *sockname)
{
  struct sockaddr_un address;

  if ((_stream = socket(PF_UNIX, SOCK_STREAM, 0)) < 0) {
    perror("opening stream socket");
    return 1;
  } 

  /* start with a clean address structure */
  memset(&address, 0, sizeof(struct sockaddr_un));

  address.sun_family = AF_UNIX;
  snprintf(address.sun_path, UNIX_PATH_MAX, sockname);
  remove (_GSESSION);

  if (bind(_stream, (struct sockaddr *)&address, sizeof(struct sockaddr_un))) {
    perror("binding stream socket");
    return 1;
  }

  if (listen(_stream, 5)) {
    perror("listen stream socket");
    return 1;
  }
  return 0;
} /* </stream_socket> */

/*
* spawn child process - override libgould.so implementation
*/
pid_t
spawn(const char *cmdline)
{
  pid_t pid = fork();

  if (pid == 0) {	/* child process */
    const static char *shell = "/bin/sh";

    setsid();
    execlp(shell, shell, "-f", "-c", cmdline, NULL);
    exit(0);
  }
  return pid;
} /* </spawn> */

/*
* timestamp - yield a "%Y-%m-%d %H:%M:%S" string
*/
static char *
timestamp(void)
{
  static char stamp[MAX_STAMP];
  struct tm* tinfo;
  time_t clock;

  clock = time(NULL);
  tinfo = localtime(&clock);
  strftime(stamp, MAX_STAMP-1, "%Y-%m-%d %H:%M:%S", tinfo);

  return stamp;
} /* </timestamp> */

/*
* apply_signal_responder
*/
static inline void
apply_signal_responder(void)
{
  signal(SIGHUP,  signal_responder);	/* 1 TBD reload */
  signal(SIGINT,  signal_responder);	/* 2 Ctrl+C received */
  signal(SIGQUIT, signal_responder);	/* 3 internal program error */
  signal(SIGILL,  signal_responder);	/* 4 internal program error */
  signal(SIGABRT, signal_responder);	/* 6 internal program error */
  signal(SIGBUS,  signal_responder);	/* 7 internal program error */
  signal(SIGKILL, signal_responder);    /* 9 internal program error */
  signal(SIGSEGV, signal_responder);	/* 11 internal program error */
  signal(SIGALRM, signal_responder);	/* 14 internal program error */
  signal(SIGTERM, signal_responder);	/* 15 graceful exit on kill */
  signal(SIGCHLD, signal_responder);	/* 17 reap children */
  signal(SIGCONT, signal_responder);	/* 18 cancel request */
} /* </apply_signal_responder> */

/**
* gession main program
*/
int main(int argc, char *argv[])
{
  const char *loglevel = getenv("LOGLEVEL");	/* 0 => none */

  const char *launcher = getenv("LAUNCHER");
  const char *manager  = getenv("WINDOWMANAGER");
  const char *panel    = getenv("PANEL");

  int connection;

  if ((debug = (loglevel) ? atoi(loglevel) : 0)) {
    static char logfile[MAX_PATHNAME];
    sprintf(logfile, "%s/%s.log", getenv("HOME"), Program);
    if (! (_logstream = fopen(logfile, "w"))) perror("cannot open logfile");
    sessionlog(1, "%s started on %s\n", Program, timestamp());
  }

  if (stream_socket(_GSESSION) != 0)
    return EX_PROTOCOL;

  apply_signal_responder();
  _instance = getpid();			/* singleton process ID */

  /* spawn {PANEL}, {LAUNCHER} and {WINDOWMANAGER} */
  spawn( (panel) ? panel : "gpanel" );  /* spawn desktop [control] panel */

  if(launcher) spawn( launcher );	/* spawn application launcher */
  if(manager) spawn( manager );		/* spawn WINDOWMANAGER */

  /* gsession main loop */
  for ( ;; ) {
    if ((connection = accept(_stream, 0, 0)) < 0)
      perror("accept connection on socket");
    else {
      while (consign (connection) > 0) ;
      close(connection);
    }
  }
  return EX_OK;
} /* </gsession> */
