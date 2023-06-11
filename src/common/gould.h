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

#ifndef _GOULD
#define _GOULD "/tmp/gould"

#include <string.h>
#include <libintl.h>
#include <unistd.h>

#define GETTEXT_PACKAGE "gould"
#define _(string) gettext(string)

#define BACKTRACE_SIZE  69

#define MAX_BUFFER_SIZE 512
#define MAX_PATHNAME    256
#define MAX_STRING      128
#define MAX_LABEL       64
#define MAX_STAMP       32

#define MAXSTR(a,b) MAX(strlen(a),strlen(b))
#define MINSTR(a,b) MIN(strlen(a),strlen(b))

#define Authors "Generations Linux (bugs@softcraft.org)"
//"Generations Linux <bugs@softcraft.org>" <= problem with <>

#define Singleton "another instance of this program is already running."

typedef unsigned short debug_t;

enum {
  _SUCCESS,
  _RUNNING,
  _MISSING,
  _INVALID,
};

/* Methods exported by implementation */
const char *timestamp(void);
pid_t get_process_id(const char *program);
int gould_diagnostics(const char *program);
pid_t spawn(const char *program);

#endif

