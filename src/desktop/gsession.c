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
#include <string.h>
#include <signal.h>
#include <execinfo.h>	/* backtrace declarations */
#include <libgen.h>	/* definitions for pattern matching functions */
#include <sysexits.h>	/* exit status codes for system programs */
#include <sys/prctl.h>	/* operations on a process or thread */
#include <unistd.h>

const char *Program = "gsession";
const char *Release = "1.1.1";

debug_t debug = 0;  /* debug verbosity (0 => none) {must be declared} */
int _stream = -1;   /* stream socket descriptor */


/*
* daemonize
*/
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
* connection handler
*/
int
receive (int connection)
{
  int nbytes;
  char message[MAX_PATHNAME];

  memset(message, 0, MAX_PATHNAME);
  nbytes = read(connection, message, MAX_PATHNAME);

  if (nbytes < 0)
    perror("reading message from stream socket");
  else if (nbytes > 0)
  {
    message[nbytes] = 0;

    /* overload message[] with process id */
    if (strcmp(message, _GETPID) == 0)
      sprintf(message, "%d\n", getpid());
    else
      sprintf(message, "%d\n", spawn( message ));

    write(connection, message, strlen(message));
  }
  return nbytes;
} /* receive */

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
* responder - signal handler
*/
void
responder(int sig)
{
  const static debug_t BACKTRACE_SIZE = 69;

  switch (sig) {
    case SIGHUP:
    case SIGCONT:
      break;

    case SIGCHLD:		/* reap children */
      while (waitpid(-1, NULL, WNOHANG) > 0);
      break;

    default:
      close(_stream);
      unlink(_GSESSION);
    
      if (sig != SIGINT && sig != SIGTERM) {
        void *trace[BACKTRACE_SIZE];
        int nptrs = backtrace(trace, UNIX_PATH_MAX);

        printf("%s, exiting on signal: %d\n", Program, sig);
        backtrace_symbols_fd(trace, nptrs, STDOUT_FILENO);
      }
      exit (sig);
  }
} /* </responder> */

/*
* apply_signal_responder
*/
static inline void
apply_signal_responder(void)
{
  signal(SIGHUP,  responder);	/* 1 TBD reload */
  signal(SIGINT,  responder);	/* 2 Ctrl+C received */
  signal(SIGQUIT, responder);	/* 3 internal program error */
  signal(SIGILL,  responder);	/* 4 internal program error */
  signal(SIGABRT, responder);	/* 6 internal program error */
  signal(SIGBUS,  responder);	/* 7 internal program error */
  signal(SIGKILL, responder);   /* 9 internal program error */
  signal(SIGSEGV, responder);	/* 11 internal program error */
  signal(SIGALRM, responder);	/* 14 internal program error */
  signal(SIGTERM, responder);	/* 15 graceful exit on kill */
  signal(SIGCHLD, responder);	/* 17 reap children */
  signal(SIGCONT, responder);	/* 18 cancel request */
} /* </apply_signal_responder> */

/**
* gession main program
*/
int main(int argc, char *argv[])
{
  const char *launcher = getenv("LAUNCHER");
  const char *manager  = getenv("WINDOWMANAGER");
  const char *panel    = getenv("PANEL");

  int connection;

  if (stream_socket(_GSESSION) != 0)
    return EX_PROTOCOL;

  apply_signal_responder();

  /* spawn {PANEL}, {LAUNCHER} and {WINDOWMANAGER} */
  spawn( (panel) ? panel : "gpanel" );  /* spawn desktop [control] panel */

  if(launcher) spawn( launcher );	/* spawn application launcher */
  if(manager) spawn( manager );		/* spawn WINDOWMANAGER */

  /* gsession main loop */
  for ( ;; ) {
    if ((connection = accept(_stream, 0, 0)) < 0)
      perror("accept connection on socket");
    else {
      while (receive(connection) > 0) ;
      close(connection);
    }
  }
  return EX_OK;
} /* </gsession> */
