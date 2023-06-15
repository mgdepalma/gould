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
#ifndef _GSESSION
#define _GSESSION "/tmp/gsession"

/* special request - get gsession main process ID */
#define _GET_SESSION_PID "(:_gsession_:)"

#define GsessionProcess  "gsession"  /* gsession published process name */
#define GdesktopProcess  "gpanel"    /* gdesktop published process name */

#define _GESSION_BACKEND "master"
#define _GESSION_MONITOR "anvil"

#include <sys/socket.h>
#include <sys/un.h>

#ifndef UNIX_PATH_MAX
#define UNIX_PATH_MAX 108
#endif
#endif /* </_GSESSION> */

