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
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <signal.h>
#include <sysexits.h>	/* exit status codes for system programs */
#include <sys/prctl.h>	/* operations on a process or thread */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <libgen.h>	/* definitions for pattern matching functions */
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

#define SIGUSR3 SIGWINCH	/* SIGWINCH => 28, reserved */

#ifndef SIGUNUSED
#define SIGUNUSED 31
#endif

const char *Program = "gsession";
const char *Release = "1.2.2";

const char *Description =
"<!-- %s %s is a X Window System session manager.\n"
"  The program is developed for Generations Linux and distributed\n"
"  under the terms and condition of the GNU Public License. It is\n"
"  a central part of gould (http://www.softcraft.org/gould).\n"
"-->\n";

const char *Usage =
"usage: %s [-v | -h]\n"
"\n"
"\t-v print version information\n"
"\t-h print help usage (what you are reading)\n"
"\n"
"There can only be one instance running per display.\n"
"\n";

/* (protected) session monitor variables */
const char *_monitor_environ = "GOULD_ENVIRON=no-splash";
const int _monitor_seconds_interval = 5;

debug_t debug = 1;	/* debug verbosity (0 => none) {must be declared} */

pid_t _gdesktop = -1;	/* gdesktop (i.e. what we monitor) process ID */
pid_t _instance;	/* singleton process ID */

FILE *_logstream = 0;	/* depends on getenv(LOGLEVEL) > 0 */
int _stream = -1;	/* stream socket descriptor */

/**
* prototypes (forward method declarations)
*/
int acknowledge(int connection);
int open_stream_socket(const char *sockname);
void signal_responder(int signum);

/*
* (private)sessionlog - write when debug >= {level}
*/
static int
sessionlog(debug_t level, const char *format, ...)
{
  int nbytes = 0;

  if (debug >= level) {
    va_list args;
    va_start (args, format);

    if (_logstream) {
      nbytes = vfprintf(_logstream, format, args);
      fflush(_logstream);
    }
    else {
      nbytes = vprintf(format, args);
    }
    va_end (args);
  }
  return nbytes;
} /* </sessionlog> */

/*
* gsession backend process thread
*/
int
session_backend(const char *name)
{
  int connection;       /* connection stream socket */
  pid_t pid = fork();

  if (pid < 0) {
    perror("session_backend: fork() failed.");
    _exit (EX_OSERR);
  }

  if (pid > 0) {        /* not in spawned process */
    int stat;
    sessionlog(1, "%s [%s] pid => %d\n", timestamp(), name, pid);
    waitpid(pid, &stat, 0);
    return pid;
  }

  if (setsid() < 0) {   /* set new session */
    perror("session_backend: setsid() failed.");
    _exit (EX_SOFTWARE);
  }
  prctl(PR_SET_NAME, (unsigned long)name, 0, 0);

  close(STDIN_FILENO);	/* close stdin. stdout and stderr */
  close(STDOUT_FILENO);
  close(STDERR_FILENO);

  for ( ;; ) {		/* gsession main loop */
    if ((connection = accept(_stream, 0, 0)) < 0)
      perror("accept connection on socket");
    else {
      while (acknowledge (connection) > 0) ;
      close(connection);
    }
  }
  return 0;
} /* </session_backend> */

/*
* gsession monitor process thread
*/
int
session_monitor(const char *name)
{
  char procfile[MAX_LABEL];
  pid_t pid = fork();

  if (pid < 0) {
    perror("session_monitor: fork() failed.");
    _exit (EX_OSERR);
  }

  if (pid > 0) {	/* not in spawned process */
    sessionlog(1, "%s [%s] pid => %d\n", timestamp(), name, pid);
    return pid;
  }

  if (setsid() < 0) {	/* set new session */
    perror("session_monitor: setsid() failed.");
    _exit (EX_SOFTWARE);
  }
  prctl(PR_SET_NAME, (unsigned long)name, 0, 0);
  
  close(STDIN_FILENO);	/* close stdin. stdout and stderr */
  close(STDOUT_FILENO);
  close(STDERR_FILENO);

  /* regularly signal _gdesktop */
  sprintf(procfile, "/proc/%d", _gdesktop);

  for ( ;; ) {
    if (access(procfile, R_OK) == 0) {
      sessionlog(2, "%s %s (SIGUSR3 -> %d)\n", timestamp(), name, _gdesktop);
      kill (_gdesktop, SIGUSR3);
    }
    else {
      kill (_instance, SIGHUP);
      _gdesktop = get_process_id (GdesktopProcess);
      sprintf(procfile, "/proc/%d", _gdesktop);
    }
    sleep (_monitor_seconds_interval);
    signal(SIGHUP, signal_responder);
  }
  return 0;
} /* </session_monitor> */

/*
* session_spawn - spawn wrapper that uses sessionlog
*/
static pid_t
session_spawn(const char *command)
{
  pid_t pid = spawn (command);
  sessionlog(1, "%s [%s] pid => %d\n", timestamp(), command, pid);
  return pid;
} /* </session_spawn> */

/*
* session_graceful - signal _GSESSION_{BACKEND,MONITOR} to exit
*/
static void
session_graceful(int signum, int seconds)
{
  killall (_GSESSION_BACKEND, signum);
  killall (_GSESSION_MONITOR, signum);
  if(seconds > 0) sleep (seconds);
} /* </session_graceful> */

/*
* acknowledge - socket stream request delivery
*/
int
acknowledge(int connection)
{
  int nbytes;
  char request[MAX_STRING];

  memset(request, 0, MAX_STRING);
  nbytes = read(connection, request, MAX_STRING);

  if (nbytes < 0)
    perror("reading request from stream socket");
  else if (nbytes > 0) {
    const char *stamp = timestamp();
    request[nbytes] = 0;	/* trim request to the nbytes read */

    if (strcmp(request, _GET_SESSION_PID) == 0) {  /* [main]pidof {Program} */
      sessionlog(1, "%s pidof( %s ) => %d\n", stamp, Program, _instance);
      sprintf(request, "%d\n", _instance);
    }
    else {					   /* spawn( request ) */
      sessionlog(1, "%s spawn( %s )\n", stamp, request);
      sprintf(request, "%d\n", spawn( request ));
    }
    write (connection, request, strlen(request));
  }
  return nbytes;
} /* </acknowledge> */

/*
* open_stream_socket - open communication stream socket
*/
int
open_stream_socket(const char *sockname)
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
} /* </open_stream_socket> */

/*
* signal_responder - signal handler
*/
void
signal_responder(int signum)
{
  pid_t pid = getpid();
  const char *desktop = getenv("DESKTOP");

  switch (signum) {
    case SIGHUP:		/* spawn {DESKTOP} */
      if (pid == _instance) {
        signal(SIGHUP, SIG_IGN);
        putenv (_monitor_environ);
        session_spawn( (desktop) ? desktop : "gdesktop" );
      }
      break;

    case SIGTERM:
      if(pid != _instance) _exit (EX_OK);
      break;

    case SIGCHLD:		/* reap children */
      while (waitpid(-1, NULL, WNOHANG) > 0) ;
      break;

    case SIGCONT:
      break;

    case SIGTTIN:
    case SIGTTOU:
      sessionlog(1, "%s: caught signal %d, ignoring.\n", __func__, signum);
      break;

    case SIGUSR3:		/* acknowledge from gdesktop process */
      sessionlog(2, "%s gdesktop acknowledges\n", timestamp());
      sleep (_monitor_seconds_interval);
      break;

    default:
      if (pid == _instance) {
        close(_stream);
        remove(_GSESSION);
    
        if (! (signum == SIGINT || signum == SIGTERM)) {
          printf("%s, exiting on signal: %d\n", Program, signum);
          gould_diagnostics (Program);
        }
        sessionlog(1, "%s ended on %s\n", Program, timestamp());
        if(_logstream) fclose(_logstream);

        session_graceful (SIGTERM, 2);
        session_graceful (SIGKILL, 0);

        _exit (signum);
      }
  }
} /* </signal_responder> */

/*
* apply_signal_responder
*/
static void
apply_signal_responder(void)
{
  signal(SIGHUP,  signal_responder);	/* 1 spawn {DESKTOP} */
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
  signal(SIGUSR3, signal_responder);	/* 28 acknowleged */
} /* </apply_signal_responder> */

/*
* interface {program} stream socket 
*/
void
interface(const char *program)
{
  static char logfile[MAX_PATHNAME];

  const char *errorlog = getenv("ERRORLOG");
  const char *loglevel = getenv("LOGLEVEL"); /* 0 => none */

  const char *windowmanager = getenv("WINDOWMANAGER");
  const char *screensaver  = getenv("SCREENSAVER");

  const char *desktop  = getenv("DESKTOP");
  const char *launcher = getenv("LAUNCHER");

  if ((debug = (loglevel) ? atoi(loglevel) : 0) > 0) {
    sprintf(logfile, "%s/%s.log", getenv("HOME"), program);
    if (! (_logstream = fopen(logfile, "w"))) perror("cannot open logfile");
    sessionlog(1, "%s started on %s\n", program, timestamp());
  }

  if (errorlog) {
    remove (errorlog);
    int fd = creat (errorlog, 0644);
    close (fd);
  }

  if (open_stream_socket(_GSESSION) != 0)
    _exit (EX_PROTOCOL);

  /* spawn {WINDOWMANAGER}, {SCREENSAVER}, {DESKTOP}, and {LAUNCHER} */
  _gdesktop = session_spawn( (desktop) ? desktop : "gdesktop" );
  session_spawn( (windowmanager) ? windowmanager : "twm" );

  if(screensaver) session_spawn( screensaver );
  if(launcher) session_spawn( launcher );

  /* monitor the gdesktop process */
  session_monitor( _GSESSION_MONITOR );

  /* gsession::backend *must* be called last.. */
  session_backend( _GSESSION_BACKEND );
} /* </interface> */

/**
* gession main program
*/
int
main(int argc, char *argv[])
{
  int opt;

  /* disable invalid option messages */
  opterr = 0;

  while ((opt = getopt (argc, argv, "d:hv")) != -1) {
    switch (opt) {
      case 'd':
        debug = atoi(optarg);
        break;

      case 'h':
        printf(Usage, Program);
        return EX_OK;
        break;

      case 'v':
        printf(Description, Program, Release);
        return EX_OK;
        break;

      default:
        printf("%s: invalid option, use -h for help usage.\n", Program);
        return EX_USAGE;
    }
  }

  session_graceful (SIGTERM, _SIGTERM_GRACETIME);
  session_graceful (SIGKILL, 0); /* draconian approach */

  _instance = getpid();		 /* singleton process ID */
  apply_signal_responder();	 /* trap and handle signals */
  interface (Program);		 /* program interface */

  return EX_OK;
} /* </gsession> */
