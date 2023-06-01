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

#include <libintl.h>
#include <string.h>

#define GETTEXT_PACKAGE "gould"
#define _(string) gettext(string)

#define MAX_BUFFER_SIZE 512
#define MAX_PATHNAME    256
#define MAX_STRING      128
#define MAX_LABEL       64

#define MAXSTR(a,b) MAX(strlen(a),strlen(b))
#define MINSTR(a,b) MIN(strlen(a),strlen(b))

typedef unsigned short debug_t;

#define RUNNING_ 1
#define MISSING_ 2
#define INVALID_ 3
#define VALID_   0
#endif
