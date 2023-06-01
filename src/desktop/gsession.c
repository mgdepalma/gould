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
#include "gould.h"
#include "gsession.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <libgen.h>

const char *Program = "gsession";
const char *Release = "1.1.1";

debug_t debug = 0;  /* debug verbosity (0 => none) {must be declared} */
int _stream = -1;   /* stream socket descriptor */

/**
* spawn child process
*/
pid_t spawn(const char *cmdline)
{
  pid_t pid = fork();

  if (pid == 0) {		// child process
    const static char *shell = "/bin/sh";

    setsid();
    execlp(shell, shell, "-f", "-c", cmdline, NULL);
    exit(0);
  }
  return pid;
} /* </spawn> */

/**
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

/**
* connection handler
*/
int receive (int connection)
{
  int nbytes;
  char message[MAX_PATHNAME];
  pid_t pid;

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

/**
* main program
*/
int main(int argc, char *argv[])
{
  const char *manager  = getenv("WINDOWMANAGER");
  const char *launcher = getenv("LAUNCHER");
  const char *desktop  = getenv("PANEL");

  struct sockaddr_un address;
  int connection;

  if ((_stream = socket(PF_UNIX, SOCK_STREAM, 0)) < 0) {
    perror("opening stream socket");
    return 1;
  } 

  /* start with a clean address structure */
  memset(&address, 0, sizeof(struct sockaddr_un));

  address.sun_family = AF_UNIX;
  snprintf(address.sun_path, UNIX_PATH_MAX, _GSESSION);
  unlink(_GSESSION);

  if (bind(_stream, (struct sockaddr *)&address, sizeof(struct sockaddr_un))) {
    perror("binding stream socket");
    return 1;
  }

  if (listen(_stream, 5)) {
    perror("listen stream socket");
    return 1;
  }

  signal(SIGCHLD, SIG_IGN);	// ignore SIGCHLD
  signal(SIGABRT, responder);	// internal program error
  signal(SIGALRM, responder);	// ..
  signal(SIGBUS,  responder);	// ..
  signal(SIGILL,  responder);	// ..
  signal(SIGIOT,  responder);   // ..
  signal(SIGKILL, responder);   // ..
  signal(SIGQUIT, responder);	// ..
  signal(SIGSEGV, responder);	// ..
  signal(SIGTERM, responder);	// graceful exit
  signal(SIGINT,  responder);	// ..
  signal(SIGCONT, responder);	// cancel request
  signal(SIGHUP,  responder);	// reload

  // spawn WINDOWMANAGER and TASKBAR
  if(manager) spawn( manager );
  if(launcher) spawn( launcher );
  spawn( (desktop) ? desktop : "gpanel" );

  // main loop
  for ( ;; ) {
    if ((connection = accept(_stream, 0, 0)) < 0)
      perror("accept connection on socket");
    else {
      while (receive(connection) > 0) ;
      close(connection);
    }
  }
  return 0;
}
