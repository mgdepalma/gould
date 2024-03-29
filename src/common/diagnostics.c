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

#include "gould.h"	/* common package declarations */

#include <ctype.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <execinfo.h>	/* backtrace declarations */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

/*
* timestamp - yield a "%Y-%m-%d %H:%M:%S" string
*/
const char *
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
* gould_diagnostics
*/
void
gould_diagnostics(const char *format, ...)
{
  void *trace[BACKTRACE_SIZE];
  const char *errorlog = getenv("ERRORLOG");

  int nptrs = backtrace(trace, BACKTRACE_SIZE);
  char caption[MAX_COMMAND];

  va_list args;
  va_start (args, format);
  vsprintf(caption, format, args);
  va_end (args);

  printf("%s %s\n", timestamp(), caption);
  backtrace_symbols_fd(trace, nptrs, STDOUT_FILENO);

  if (errorlog) {
    int errfd = open(errorlog, O_APPEND | O_WRONLY, 0644);

    if (errfd > 0) {
      strcat(caption, "\n");
      write(errfd, caption, strlen(caption));
      backtrace_symbols_fd(trace, nptrs, errfd);
      close(errfd);
    }
  }
} /* </gould_diagnostics> */

/*
* gould_error - variadic {ERRORLOG} write
*/
void
gould_error(const char *format, ...)
{
  const char *errorlog = getenv("ERRORLOG");

  if (errorlog) {
    FILE *stream = fopen(errorlog, "a");

    if (stream) {
      va_list args;
      va_start (args, format);
      vfprintf (stream, format, args);
      va_end (args);

      fclose(stream);
    }
  }
} /* </gould_error> */
