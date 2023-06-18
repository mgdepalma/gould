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

#include "gould.h"		/* common package declarations */
#include "gsession.h"
#include "util.h"

#include <stdio.h>
#include <sysexits.h>		/* exit status codes for system programs */
#include <sys/prctl.h>		/* operations on a process or thread */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

const char *Program = "gcommand";  /* (public) published program name    */
const char *Release = "0.8.1";     /* (public) published program version */

const char *Description =
"submits a command to the session manager.\n"
"  The program is developed for Generations Linux and distributed\n"
"  under the terms and condition of the GNU Public License. It is\n"
"  part of gould (http://www.softcraft.org/gould)." ;

const char *Usage =
"usage: %s [-v | -h | <command>]\n"
"\n"
"\t-v print version information\n"
"\t-h print help usage (what you are reading)\n"
"\n"
"\t<command> to relay the session manager.\n"
"\n";

debug_t debug = 0;	/* debug verbosity (0 => none) {must be declared} */


/*
* dispatch - dispatch request using session socket stream
*/
pid_t
dispatch (int stream, const char *command)
{
  pid_t pid;

  if (stream < 0)	/* stream socket must be > 0 */ 
    pid = spawn (command);
  else {
    int  nbytes;
    char reply[MAX_PATHNAME];

    write(stream, command, strlen(command));
    nbytes = read(stream, reply, MAX_PATHNAME);
    reply[nbytes] = 0;
    pid = atoi(reply);
  }
  return pid;
} /* </dispatch> */

/*
* open session stream socket
*/
static int
open_session_stream(const char *pathway)
{
  int stream = socket(PF_UNIX, SOCK_STREAM, 0);

  if (stream < 0)
    perror("opening stream socket"); 
  else {
    struct sockaddr_un address;
    memset(&address, 0, sizeof(struct sockaddr_un));

    address.sun_family = AF_UNIX;
    snprintf(address.sun_path, UNIX_PATH_MAX, pathway);

    if (connect(stream, (struct sockaddr *)&address,
					sizeof(struct sockaddr_un))) {
      perror("connect stream socket");
      stream = -1;
    }
  }
  return stream;
} /* open_session_stream */

/*
* main - gcommand main program
*/
int
main(int argc, char *argv[])
{
  char *command = argv[1];

  int stream = -1;	/* stream socket descriptor */
  int status = EX_OK;
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
        _exit (status);

      case 'v':
        printf("<!-- %s %s %s\n -->\n", Program, Release, Description);
        _exit (status);

      default:
        printf("%s: invalid option, use -h for help usage.\n", Program);
        _exit (EX_USAGE);
    }
  }
  printf("%s: command => %s\n", Program, command);

  if ((stream = open_session_stream (_GSESSION)) > 0) {
    dispatch (stream, command);
    close (stream);
  }
  return status;
} /* </gcommand> */

