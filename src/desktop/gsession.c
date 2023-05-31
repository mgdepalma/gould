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
#include <libgen.h>
#include <unistd.h>
#include <execinfo.h>	/* backtrace declarations */

const char *Program = "gsession";
const char *Release = "1.1.1";	/* must be declared (libgould.so) */

debug_t debug = 0;  /* debug verbosity (0 => none) {must be declared} */
int _stream = -1;   /* stream socket descriptor */


/**
* spawn child process
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

/**
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

/**
* responder - signal handler
*/
void
responder(int sig)
{
  const static debug_t BACKTRACE_LINES = 69;

  switch (sig) {
    case SIGHUP:
    case SIGCONT:
      break;

    default:
      close (_stream);
      remove (_GSESSION);
    
      if (sig != SIGINT && sig != SIGKILL && sig != SIGTERM) {
        void *trace[BACKTRACE_LINES];
        int nptrs = backtrace(trace, BACKTRACE_LINES);

        printf("%s, exiting on signal: %d\n", Program, sig);
        backtrace_symbols_fd(trace, nptrs, STDOUT_FILENO);
      }
      exit (sig);
      break;
  }
} /* </responder> */

/*
* apply_signal_responder
*/
static inline void
apply_signal_responder(void)
{
  signal(SIGCHLD, SIG_IGN);	/* ignore SIGCHLD */
  signal(SIGABRT, responder);	/* internal program error */
  signal(SIGALRM, responder);	/* .. */
  signal(SIGBUS,  responder);	/* .. */
  signal(SIGILL,  responder);	/* .. */
  signal(SIGIOT,  responder);   /* .. */
  signal(SIGKILL, responder);   /* .. */
  signal(SIGQUIT, responder);	/* .. */
  signal(SIGSEGV, responder);	/* .. */
  signal(SIGTERM, responder);	/* graceful exit on kill */
  signal(SIGINT,  responder);	/* Ctrl+C received */
  signal(SIGCONT, responder);	/* cancel request */
  signal(SIGHUP,  responder);	/* TBD reload */
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
    return 1;

  apply_signal_responder();

  /* spawn {PANEL}, {LAUNCHER} and {WINDOWMANAGER} */
  spawn( (panel) ? panel : "gpanel" );  /* spawn desktop [control] panel */

  if(launcher) spawn( launcher );       /* spawn application launcher */
  if(manager) spawn( manager );	        /* spawn WINDOWMANAGER */

  /* gsession main loop */
  for ( ;; ) {
    if ((connection = accept(_stream, 0, 0)) < 0)
      perror("accept connection on socket");
    else {
      while (receive(connection) > 0) ;
      close(connection);
    }
  }
  return 0;
} /* </gsession> */
