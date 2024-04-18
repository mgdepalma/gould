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

#ifndef _SCREENSAVER_H
#define _SCREENSAVER_H 1

#define MAX_SCREENSAVER 256

#define _xscreensaver_daemon	    "xscreensaver"
#define _xscreensaver_command	    "xscreensaver-command"
#define _xscreensaver_system_config "/etc/default/.xscreensaver"
#define _xscreensaver_backup_config ".xscreensaver~"
#define _xscreensaver_user_config   ".xscreensaver"

#define _xscreensaver_config_directory "/usr/share/xscreensaver/config"
#define _xscreensaver_modes_directory "/usr/libexec/xscreensaver"

#define _SCREENSAVER_MODES    _xscreensaver_modes_directory
#define _SCREENSAVER_CONFIG   _xscreensaver_config_directory

#define _SCREENSAVER_COMMAND  _xscreensaver_command
#define _SCREENSAVER_DAEMON   _xscreensaver_daemon

#define _SCREENSAVER_ACTIVATE _SCREENSAVER_COMMAND " --activate"
#define _SCREENSAVER_DISABLE  _SCREENSAVER_COMMAND " --deactivate"
#define _SCREENSAVER_GRACEFUL _SCREENSAVER_COMMAND " --exit"

#define _SCREENSAVER_RESERVED "xscreensaver"

/* enumeration type for mode selection */
typedef enum _screensaver_modes screensaver_modes_t;

enum _screensaver_modes {
  SCREENSAVER_DISABLE,
  SCREENSAVER_SINGLE,
  SCREENSAVER_RANDOM,
  SCREENSAVER_BLANK,
  N_SCREENSAVER_MODES
};

const int screensaver_get_mode (const char *buffer);
const int screensaver_get_selected (const char *buffer);

pid_t screensaver_preview_pane (void);

#endif /* </_SCREENSAVER_H */
