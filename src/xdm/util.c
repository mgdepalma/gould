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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <ctype.h>
#include <execinfo.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <unistd.h>

#include "util.h"

#ifndef LOGFILE
#define LOGFILE "/var/log/gould.log"
#endif

/*
 * log_print
 * log_sigsegv
 */
void
log_print(char *fmt, ...)
{
  FILE *log = fopen(LOGFILE, "a");

  if (log) {
    va_list ap;

    va_start(ap, fmt);
    vfprintf(log, fmt, ap);
    va_end(ap);

    fclose(log);
  }
}

void
log_sigsegv(void)
{
  size_t size;
  void *array[40];
  int fd;

  fd = open(LOGFILE, O_WRONLY|O_CREAT|O_APPEND,S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
  if(fd == -1) return;

  size = backtrace(array, 40);
  backtrace_symbols_fd(array, size, fd);
  close(fd);
}

/*
 * saveconfig
 */
void
saveconfig(GKeyFile *config, const char *path)
{
  gsize len;
  char *data = g_key_file_to_data(config, &len, NULL);
  g_file_set_contents(path, data, len, NULL);
  g_free(data);
}

/*
 * session_desktop_get
 * session_desktop_put
 */
const char *
session_desktop_get(const char *resource)
{
  const char *name = NULL;
  static char result[MAX_PATHNAME];

  FILE *stream;
  char line[MAX_PATHNAME];
  int  mark;

  /* Search for "DESKTOP=<name>" in the resource file. */
  if ((stream = fopen(resource, "r"))) {
    while (fgets(line, MAX_PATHNAME, stream)) {
      if (strncmp(line, "DESKTOP=", 8) == 0) {
        if (line[8] == '"') {           /* <name> may be in quotes */
          line[strlen(line) - 2] = (char)0;
          mark = 9;
        }
        else {
          line[strlen(line) - 1] = (char)0;
          mark = 8;
        }
        strcpy(result, &line[mark]);
        name = (const char *)result;
        break;
      }
    }
    fclose(stream);
  }
  return name;
} /* session_desktop_get */

gboolean
session_desktop_put(const char *resource, const char *name)
{
  FILE *stream;
  char **buffer = NULL;
  char line[MAX_PATHNAME];
  int  count = 0;
  int  idx;

  /* Buffer contents of resource file. */
  if ( (stream = fopen(resource, "r")) ) {
    while (fgets(line, MAX_PATHNAME, stream)) ++count;
    buffer = calloc(count, sizeof (char *));

    if (buffer) {
      fseek(stream, 0L, SEEK_SET);

      for (idx=0; idx < count; idx++) {
        if (fgets(line, MAX_PATHNAME, stream) <= 0) break;
        buffer[idx] = strdup(line);
      }
    }
    else {
      fclose(stream);
      return FALSE;
    }
    fclose(stream);
  }
  
  /* Re-open resource file for writing. */
  if ( (stream = fopen(resource, "w")) ) {
    gboolean insert = TRUE;

    for (idx=0; idx < count; idx++) {
      if (strncmp(buffer[idx], "DESKTOP=", 8) == 0) {
        fprintf(stream, "DESKTOP=\"%s\"\n", name);
        insert = FALSE;
      }
      else {
        fputs(buffer[idx], stream);
      }
    }
    /* "DESKTOP=<name>" was not specified in the resource file. */
    if(insert) fprintf(stream, "DESKTOP=\"%s\"\n", name);
    fclose(stream);
  }

  for (idx=0; idx < count; idx++) free(buffer[idx]);
  free(buffer);

  return TRUE;
} /* session_desktop_put */

/*
 * session_lang_get
 * session_lang_put
 */
const char *
session_lang_get(const char *resource)
{
  const char *name = NULL;
  static char result[MAX_PATHNAME];

  FILE *stream;
  char line[MAX_PATHNAME];
  int  mark;

  /* Search for "LANG=<name>" in the resource file. */
  if ( (stream = fopen(resource, "r")) ) {
    while (fgets(line, MAX_PATHNAME, stream)) {
      if (strncmp(line, "LANG=", 5) == 0) {
        if (line[5] == '"') {           /* <name> may be in quotes */
          line[strlen(line) - 2] = (char)0;
          mark = 6;
        }
        else {
          line[strlen(line) - 1] = (char)0;
          mark = 5;
        }
        strcpy(result, &line[mark]);
        name = (const char *)result;
        break;
      }
    }
    fclose(stream);
  }
  return name;
} /* session_lang_get */

gboolean
session_lang_put(const char *resource, const char *name)
{
  FILE *stream;
  char **buffer = NULL;
  char line[MAX_PATHNAME];
  int  count = 0;
  int  idx;

  /* Buffer contents of resource file. */
  if ( (stream = fopen(resource, "r")) ) {
    while (fgets(line, MAX_PATHNAME, stream)) ++count;
    buffer = calloc(count, sizeof (char *));

    if (buffer) {
      fseek(stream, 0L, SEEK_SET);

      for (idx=0; idx < count; idx++) {
        if (fgets(line, MAX_PATHNAME, stream) <= 0) break;
        buffer[idx] = strdup(line);
      }
    }
    else {
      fclose(stream);
      return FALSE;
    }
    fclose(stream);
  }
  
  /* Re-open resource file for writing. */
  if ( (stream = fopen(resource, "w")) ) {
    gboolean insert[2] = { TRUE, TRUE };

    for (idx=0; idx < count; idx++) {
      if (strncmp(buffer[idx], "LANG=", 5) == 0) {
        fprintf(stream, "LANG=\"%s\"\n", name);
        insert[0] = FALSE;
      }
      else if (strncmp(buffer[idx], "LANGUAGE=", 9) == 0) {
        fprintf(stream, "LANGUAGE=\"%.2s\"\n", name);
        insert[1] = FALSE;
      }
      else {
        fputs(buffer[idx], stream);
      }
    }

    /* LANG/LANGUAGE not specified in the resource file */
    if(insert[0]) fprintf(stream, "LANG=\"%s\"\n", name);
    if(insert[1]) fprintf(stream, "LANGUAGE=\"%.2s\"\n", name);

    fclose(stream);
  }

  for (idx=0; idx < count; idx++) free(buffer[idx]);
  free(buffer);

  return TRUE;
} /* session_lang_put */

/*
 * stop_pid
 */
void
stop_pid(int pid)
{
  if (pid <= 0) return;

  if (killpg(pid, SIGTERM) < 0)
    killpg(pid, SIGKILL);

  if (kill(pid, 0) == 0) {
    if (kill(pid, SIGTERM) )
      kill(pid, SIGKILL);

    while( 1 ) {
      int wpid, status;
      wpid = wait(&status);
      if(pid == wpid) break;
    }
  }
  while(waitpid(-1, 0, WNOHANG) > 0) ;
} /* stop_pid */

/*
 * sysname
 */
char *
sysname(void)
{
  char *node = NULL;
  struct utsname name;

  if (uname(&name) == 0)
    node = strdup(name.nodename);

  return node;
}
