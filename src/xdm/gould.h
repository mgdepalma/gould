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

#ifndef _GOULD_H_
#define _GOULD_H_

#include <glib.h>
#include <pwd.h>

G_BEGIN_DECLS

extern GKeyFile *config;

int  gould_auth_user(char *user, char *pass, struct passwd **ppw);
int  gould_cur_session(void);

void gould_do_login(struct passwd *pw, char *session, char *lang);
void gould_do_suspend(void);
void gould_do_shutdown(void);
void gould_do_reboot(void);
void gould_do_exit(void);

void interface_clean(void);
void interface_drop(void);
void interface_init(void);

int  interface_do_login(void);
int  interface_main(void);

enum AuthResult
{
  AUTH_SUCCESS,
  AUTH_FAILURE,
  AUTH_BAD_USER,
  AUTH_PRIV,
  AUTH_ERROR
};

typedef struct {
  char *desktop;
  char *name;
  char *exec;
} Session;

G_END_DECLS
#endif /*_GOULD_H_*/

