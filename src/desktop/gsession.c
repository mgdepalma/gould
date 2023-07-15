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
#include "screensaver.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <signal.h>
#include <assert.h>
#include <sysexits.h>	/* exit status codes for system programs */
#include <sys/prctl.h>	/* operations on a process or thread */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <libgen.h>	/* definitions for pattern matching functions */
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

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
SessionMonitor monitor_[SessionMonitorCount];

char *_monitor_environ = "GOULD_ENVIRON=no-splash";
const int _monitor_seconds_interval = 2;

debug_t debug = 1;	/* debug verbosity (0 => none) {must be declared} */

FILE *_logstream = 0;	/* depends on getenv(LOGLEVEL) > 0 */
int _stream = -1;	/* stream socket descriptor */

pid_t _instance = 0;	/* singleton process ID */
pid_t _backend = 0;	/* backend process ID */
pid_t _monitor = 0;	/* monitor process ID */

/**
* prototypes (forward method declarations)
*/
int acknowledge(int connection);
int open_stream_socket(const char *sockname);

pid_t session_spawn(const int idx, bool async);
pid_t session_respawn(const int idx);

void session_monitor_request(char *request);
void signal_responder(int signum);

/*
* (private)sessionlog - write when debug >= {level}
* (private)sessionlog_stamp - sessionlog with timestamp()
* (private)session_monitor_tag - human readable tag
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

static int
sessionlog_stamp(debug_t level, const char *format, ...)
{
  int nbytes = 0;

  if (debug >= level) {
    char message[MAX_COMMAND];

    va_list args;
    va_start (args, format);
    vsprintf(message, format, args);
    va_end (args);

    nbytes = sessionlog(debug, "%s %s", timestamp(), message);
  }
  return nbytes;
} /* </sessionlog_stamp> */

static char *
session_monitor_tag(const int tag)
{
  switch (tag) {
    case _DESKTOP:
      return "DESKTOP";
    case _WINDOWMANAGER:
      return "WINDOWMANAGER";
    case _SCREENSAVER:
      return "SCREENSAVER";
    case _LAUNCHER:
      return "LAUNCHER";
  }
  return "<undefined>";
} /* </session_monitor_tag> */

/*
* session_graceful - signal _GSESSION_{BACKEND,MONITOR} to exit
*/
static void
session_graceful(int signum, ...)
{
  if (signum == SIGUNUSED) {	// prepare main program for exit
    close(_stream);
    remove(_GSESSION);
    sessionlog(1, "%s ended on %s\n", Program, timestamp());
    if(_logstream) fclose(_logstream);
  }
  else {
    int seconds = 0;
    void *data = NULL;

    killall (_GSESSION_BACKEND, signum);
    killall (_GSESSION_MONITOR, signum);

    va_list ptr;
    va_start(ptr, signum);
    seconds = va_arg(ptr, int);
    data = va_arg(ptr, void*);	// data => 0, at the end of va_list
    if(data == 0) seconds = 0;
    if(seconds > 0) sleep (seconds);
    va_end(ptr);
  }
} /* </session_graceful> */

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
    _backend = pid;
    sessionlog_stamp(1, "[%s] pid => %d\n", name, pid);
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
  bool gmonitor = false;
  const char *monitor = getenv("GOULD_MONITOR");

  int idx;		/* SessionMonitor monitor_[] index */
  char procfile[MAX_LABEL];
  pid_t pid = fork();

  if (pid < 0) {
    perror("session_monitor: fork() failed.");
    _exit (EX_OSERR);
  }

  if (monitor && strcasecmp(monitor, "yes") == 0) {
    sessionlog_stamp(1, "[%s] ping every %d seconds {DESKTOP}\n",
			name, _monitor_seconds_interval);
    gmonitor = true;
  }

  if (pid > 0) {	/* not in spawned process */
    _monitor = pid;
    sessionlog_stamp(1, "[%s] pid => %d\n", name, pid);
    return pid;
  }

  if (setsid() < 0) {	/* set new session */
    perror("session_monitor: setsid() failed.");
    _exit (EX_SOFTWARE);
  }
  prctl(PR_SET_NAME, (unsigned long)name, 0, 0);

  /* {WINDOWMANAGER}, {SCREENSAVER}, {LAUNCHER}, {DESKTOP} */
  for (idx = 0; idx < SessionMonitorCount; idx++) {
    monitor_[idx].enabled = (monitor_[idx].program) ? true : false;

    if (monitor_[idx].enabled) {  // may be turned off by request later on
      pid = session_spawn(idx, false);
      monitor_[idx].process = pid;
    }
  }

  /* if needed: session_spawn(_DESKTOP, async => true) */
  sprintf(procfile, "/proc/%d", monitor_[_DESKTOP].process);
  if(access(procfile, R_OK) != 0) session_spawn(_DESKTOP, true);

  close(STDIN_FILENO);	/* close stdin. stdout and stderr */
  close(STDOUT_FILENO);
  close(STDERR_FILENO);

  /* regularly check SessionMonitor monitor_[].process */
  for ( ;; ) {
    sleep (_monitor_seconds_interval);

    for (idx = 0; idx < SessionMonitorCount; idx++) {
      if (monitor_[idx].enabled) {
        sprintf(procfile, "/proc/%d", monitor_[idx].process);

        if (access(procfile, R_OK) != 0) {
          sessionlog_stamp(1, "[%s] pid => missing\n",
				session_monitor_tag (idx));
          session_respawn (idx);
        }
      }
    }
    if (gmonitor)	/* acknowledgement expected */
      kill(monitor_[_DESKTOP].process, SIGUSR3);
    else {
      killall (_GSESSION_DESKTOP, SIGUSR3); // gpanel may need refresh
      sleep (_monitor_seconds_interval);
    }
  }
  return 0;
} /* </session_monitor> */

/*
* session_monitor_request - _GSESSION_MONITOR request parser
*
* {_GSESSION_SERVICE} # b{_GSESSION_SERVICE_CALL} style request
*                     ^ ^  0 => disable, 1 => enable
*                     ` {_WINDOWMANAGER,_SCREENSAVER,_LAUNCHER,_DESKTOP}
*/
void
session_monitor_request(char *request)
{
  int mark = strlen(_GSESSION_SERVICE);
  int idx = (request[mark+1] - '0');

  monitor_[idx].enabled = (request[mark+3] == '1') ? true : false;

  sessionlog_stamp(1, "[%s] enabled => %s\n", session_monitor_tag(idx),
				(monitor_[idx].enabled) ? "true" : "false");

  if (monitor_[idx].enabled) {
    if(monitor_[idx].process == 0) session_respawn (idx);
    sprintf(request, "%d\n", monitor_[idx].process);
  }
  else {
    switch (idx) {
      case _SCREENSAVER:
        sessionlog_stamp(1, "[%s] exit\n", session_monitor_tag(idx));
        system (_SCREENSAVER_GRACEFUL);  // system(3) not recommended
        monitor_[idx].process = 0;
        strcpy(request, "0\n");
        break;

      default:
        sprintf(request, "%d\n", monitor_[idx].process);
        break;
    }
  }
} /* </session_monitor_request> */

/*
* session_spawn - spawn wrapper that uses sessionlog
*/
pid_t
session_spawn(const int idx, bool async)
{
  pid_t pid;

  if (async == false)
    pid = spawn( monitor_[idx].program );
  else {
    GError *err = NULL;
    gboolean res = g_spawn_command_line_async (monitor_[idx].program, &err);
    pid = get_process_id (monitor_[idx].program);
  }
  sessionlog_stamp(1, "[%s] pid => %d\n", session_monitor_tag(idx), pid);
  monitor_[idx].process = pid;

  return pid;
} /* </session_spawn> */

/*
* session_respawn - attempt to respawn well known program
*/
pid_t
session_respawn(const int idx)
{
  pid_t pid = 0;

  if (monitor_[idx].program) {
    putenv (_monitor_environ);
    pid = session_spawn(idx, false);
    sleep (_monitor_seconds_interval);
    monitor_[idx].process = pid;
  }
  else
    gould_error ("%s %s::%s monitor_[%d].program: not found.",
			timestamp(), Program, __func__, idx);
  return pid;
} /* </session_respawn> */

/*
* acknowledge - socket stream request delivery
*/
int
acknowledge(int connection)
{
  static char *sfmt = "pidof %s::%s => %d\n";

  int mark = strlen(_GSESSION_SERVICE); // 0123456789012345
					// =:gsession # b:=
  char request[MAX_COMMAND];

  pid_t pid = getpid(); // main process |_GSESSION_BACKEND |_GSESSION_MONITOR
  int nbytes;

  memset(request, 0, MAX_COMMAND);
  nbytes = read(connection, request, MAX_COMMAND);

  if (nbytes < 0)
    perror("reading request from stream socket");
  else if (nbytes > 0) {
    request[nbytes] = 0;	// chomp request to nbytes read

    if (strcmp(request, _GET_SESSION_PID) == 0) {
      if (pid == _instance)	// _GSESSION_MANAGER main process
        sessionlog_stamp(1, "pidof %s => %d\n", Program, pid);
      else {
        if (pid == _monitor)	// _GSESSION_MONITOR process thread
          sessionlog_stamp(1, sfmt, Program, _GSESSION_MONITOR, pid);
        else
          sessionlog_stamp(1, sfmt, Program, _GSESSION_BACKEND, pid);
      }
      sprintf(request, "%d\n", pid);
    }
    else if (strncmp(request, _GSESSION_SERVICE, mark) == 0) {
      if (pid == _monitor)
        session_monitor_request (request);
      else
        strcpy(request, "\n");
    }
    else {  // spawn( request )
      sessionlog_stamp(1, "spawn( %s )\n", request);
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

  switch (signum) {
    case SIGCHLD:		/* reap children */
      sessionlog_stamp(3, "%s: received SIGCHLD, pid => %d\n",Program, pid);
      while (waitpid(-1, NULL, WNOHANG) > 0) ;
      sleep (_monitor_seconds_interval);
      break;

    case SIGTERM:
      sessionlog_stamp(1, "%s: received SIGTERM, pid => %d\n", Program, pid);
      (pid == _instance) ? session_graceful (SIGUNUSED) : _exit (EX_OK);
      break;

    case SIGTTIN:
    case SIGTTOU:
      sessionlog_stamp(1,"%s: caught signal %d, ignoring.\n", Program, signum);
      break;

    case SIGUSR3:		/* acknowledgement from {DESKTOP} process */
      sessionlog_stamp(3, "%s gdesktop acknowledges\n", timestamp());
      sleep (_monitor_seconds_interval);
      break;

    default:
      sessionlog_stamp(3,"%s: signal => %d, pid => %d\n",Program, signum, pid);

      if (pid == _instance) {
        if (signum == SIGINT || signum == SIGTERM)
          printf("%s, exiting on signal: %d\n", Program, signum);
        else
          gould_diagnostics ("%s, caught signal: %d\n", Program, signum);

        session_graceful (SIGUNUSED);
        session_graceful (SIGTERM, _SIGTERM_GRACETIME);
        session_graceful (SIGKILL);

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
  signal(SIGHUP,  SIG_IGN);		/* 1 future use */
  signal(SIGINT,  signal_responder);	/* 2 Ctrl+C received */
  signal(SIGQUIT, signal_responder);	/* 3 internal program error */
  signal(SIGILL,  signal_responder);	/* 4 internal program error */
  signal(SIGABRT, signal_responder);	/* 6 internal program error */
  signal(SIGBUS,  signal_responder);	/* 7 internal program error */
  signal(SIGKILL, signal_responder);    /* 9 internal program error */
  signal(SIGUSR1, signal_responder);	/* 10 internal program error */
  signal(SIGSEGV, signal_responder);	/* 11 internal program error */
  signal(SIGALRM, signal_responder);	/* 14 internal program error */
  signal(SIGTERM, signal_responder);	/* 15 graceful exit on kill */
  signal(SIGCHLD, signal_responder);	/* 17 reap children */
  signal(SIGCONT, SIG_IGN);		/* 18 future use */
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

  /* {DESKTOP}, {WINDOWMANAGER}, {SCREENSAVER}, and {LAUNCHER} */
  monitor_[_DESKTOP].program = (desktop) ? desktop : "gdesktop";
  monitor_[_WINDOWMANAGER].program = (windowmanager) ? windowmanager : "twm";
  monitor_[_SCREENSAVER].program = screensaver;
  monitor_[_LAUNCHER].program = launcher;

  /* 20230620 session_spawn{..} done by _GSESSION_MONITOR
  for (int idx = 0; idx < SessionMonitorCount; idx++)
    if (monitor_[idx].program) {
      pid = session_spawn (monitor_[idx].program);
      monitor_[idx].process = pid;
    }
  */

  /* monitor SessionMonitor monitor_[].process */
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
  pid_t pid;

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

  _instance = getpid();		 /* singleton process ID */
  pid = get_process_id (Program);

  if (pid > 0 && pid != _instance) {
    printf("%s: %s (pid => %d)\n", Program, _(Singleton), pid);
    _exit (_RUNNING);
  }
  session_graceful (SIGTERM, _SIGTERM_GRACETIME);
  session_graceful (SIGKILL);	/* draconian approach */

  apply_signal_responder();	 /* trap and handle signals */
  interface (Program);		 /* program interface */

  return EX_OK;
} /* </gsession> */
