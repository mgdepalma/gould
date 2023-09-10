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

#define _GNU_SOURCE

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <shadow.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/ipc.h>
#include <sys/ioctl.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/vt.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <utmp.h>

#include <glib.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>

#include <X11/Xlib.h>
#include <X11/XKBlib.h>

#if HAVE_LIBPAM
#include <security/pam_appl.h>
#endif

#if HAVE_LIBCK_CONNECTOR
#include <ck-connector.h>
#endif

#if HAVE_LIBXAU
#include <X11/Xauth.h>
#include <sys/utsname.h>
#endif

#include "gould.h"
#include "util.h"

#ifndef RUNDIR
#define RUNDIR "/var/run/gould"
#endif

#ifndef XSESSION
#define XSESSION "/etc/X11/xinit/xsession"
#endif

const char *License = "(GNU General Public License version 3)";
const char *Usage =
"%s version %s %s\n"
"\n"
"  usage: %s [-d | -h | -v]\n"
"\n"
"\t-d  run as a daemon\n"
"\t-h  display a brief help message\n"
"\t-v  display version information\n"
"\n";

const char *Program;            /* published program name */
const char *Release;            /* program release version */

const key_t AUTOLOGIN = 0xbade; /* named semaphore (key_t)arg */

#if HAVE_LIBCK_CONNECTOR
static CkConnector *ckc;
#endif

GKeyFile *cache;                /* configuration data cache */
GKeyFile *config;               /* configuration system data */

static pid_t child;             /* user session process */
static pid_t server;
static guint server_watch;

static Window *self_xid;
static unsigned int self_xid_n;
static char *self_;

static int reason;              /* reason for reloading */
static int semid;               /* <PACKAGE_NAME> semaphore */

#ifndef DISABLE_XAUTH
static char mcookie[33];
#endif

#if HAVE_LIBXAU
static Xauth x_auth;
#endif

/*
 * create_server_auth
 */
void
create_server_auth(void)
{
#ifndef DISABLE_XAUTH
  GRand *h;
  const char *digits = "0123456789abcdef";
  char *authfile, *cmd;
  int i, r, hex = 0;
#if HAVE_LIBXAU
  FILE *fp;
#endif

  h = g_rand_new();
#if HAVE_LIBXAU
  for (i = 0; i < 16; i++)
    mcookie[i] = (char)g_rand_int(h);
#else
  for (i = 0; i < 31; i++) {
    r = g_rand_int(h) % 16;
    mcookie[i] = digits[r];
    if (r > 9) hex++;
  }
  if ((hex % 2) == 0)
    r = g_rand_int(h) % 10;
  else
    r = g_rand_int(h) % 5 + 10;

  mcookie[31] = digits[r];
  mcookie[32] = 0;
#endif

  g_rand_free(h);
  authfile = g_key_file_get_string(config, "core", "authfile", 0);
  if(!authfile) authfile = g_strdup_printf("%s/auth", RUNDIR);
  remove(authfile);

  cmd = g_strdup_printf("XAUTHORITY=%s", authfile);
  putenv(cmd);
  g_free(cmd);

#if HAVE_LIBXAU
  if (fp = fopen(authfile, "wb")) {
    static char xau_address[80];
    static char xau_number[16];
    static char xau_name[]="MIT-MAGIC-COOKIE-1";
    struct utsname uts;
    uname(&uts);
    sprintf(xau_address, "%s", uts.nodename);
    strcpy(xau_number,getenv("DISPLAY")+1); /* DISPLAY always exists with gould */
    x_auth.family = FamilyLocal;
    x_auth.address = xau_address;
    x_auth.number = xau_number;
    x_auth.name = xau_name;
    x_auth.address_length = strlen(xau_address);
    x_auth.number_length = strlen(xau_number);
    x_auth.name_length = strlen(xau_name);
    x_auth.data = mcookie;
    x_auth.data_length = 16;
    XauWriteAuth(fp,&x_auth);
    fclose(fp);
  }
#else
  cmd = g_strdup_printf("xauth -q -f %s add %s . %s",
                        authfile, getenv("DISPLAY"), mcookie);
  system(cmd);
  g_free(cmd);
#endif
  g_free(authfile);
#endif /* !DISABLE_XAUTH */
} /* create_server_auth */

/*
 * create_client_auth
 */
void
create_client_auth(char *home)
{
#ifndef DISABLE_XAUTH
  char *authfile, *cmd;
#if HAVE_LIBXAU
  FILE *fp;
#endif

  if (getuid() == 0)      /* root don't need it */
    return;

  authfile = g_strdup_printf("%s/.Xauthority", home);
  remove(authfile);

#if HAVE_LIBXAU
  if (fp = fopen(authfile, "wb")) {
    XauWriteAuth(fp, &x_auth);
    fclose(fp);
  }
#else
  cmd = g_strdup_printf("xauth -q -f %s add %s . %s",
                         authfile, getenv("DISPLAY"), mcookie);
  system(cmd);
  g_free(cmd);
#endif
  g_free(authfile);
#endif /* !DISABLE_XAUTH */
} /* create_client_auth */

/*
 * get_active_vt
 * set_active_vt
 */
static int
get_active_vt(void)
{
  struct vt_stat state = { 0 };
  int fd = open("/dev/console", O_RDONLY | O_NOCTTY);

  if (fd >= 0) {
    ioctl(fd, VT_GETSTATE, &state);
    close(fd);
  }

  return state.v_active;
}

#if 0
static void
set_active_vt(int vt)
{
  int fd = open("/dev/console", O_RDWR);

  if (fd < 0) fd = 0;
  ioctl(fd, VT_ACTIVATE, vt);
  if (fd != 0) close(fd);
}
#endif

#if HAVE_LIBPAM
static char *user_pass[2];

static int 
do_conv(int num, const struct pam_message **msg,
          struct pam_response **resp, void *arg)
{
  int idx;
  int result = PAM_SUCCESS;
  *resp = (struct pam_response *) calloc(num, sizeof(struct pam_response));

  for (idx=0; idx< num; idx++) {
    switch(msg[idx]->msg_style) {
      case PAM_PROMPT_ECHO_ON:
        resp[idx]->resp=strdup(user_pass[0]);
        break;

      case PAM_PROMPT_ECHO_OFF:
        resp[idx]->resp=strdup(user_pass[1]);
        break;
    }
  }
  return result;
}

static struct pam_conv conv = {.conv=do_conv, .appdata_ptr=user_pass};
static pam_handle_t *pamh;

void
setup_pam_session(struct passwd *pw, char *session_name)
{
  char tty[8];
  int  err;
 
  if (!pamh && PAM_SUCCESS != pam_start(Program, pw->pw_name, &conv, &pamh)) {
    pamh = NULL;
    return;
  }

  if(!pamh) return;
  sprintf(tty, "tty%d", get_active_vt());
  pam_set_item(pamh, PAM_TTY, tty);
#ifdef PAM_XDISPLAY
  pam_set_item( pamh, PAM_XDISPLAY, getenv("DISPLAY") );
#endif

  if (session_name && session_name[0]) {
    char *env;
    env = g_strdup_printf ("DESKTOP=%s", session_name);
    pam_putenv (pamh, env);
    g_free (env);
  }
  err = pam_open_session(pamh, 0); /* FIXME pam session failed */
  if (err != PAM_SUCCESS )
    log_print("pam open session error \"%s\"\n", pam_strerror(pamh, err));
} /* setup_pam_session */

void
close_pam_session(void)
{
  int err;
  if(!pamh) return;
  err = pam_close_session(pamh, 0);
  pam_end(pamh, err);
  pamh = NULL;
}

void
append_pam_environ(char **env)
{
  char **penv;
  int i, j, n;

  if(!pamh) return;
  penv=pam_getenvlist(pamh);
  if(!penv) return;

  for (i=0; penv[i] != NULL; i++) {
    //printf("PAM %s\n",penv[i]);
    n = strcspn(penv[i],"=")+1;

    for (j=0; env[j] != NULL; j++) {
      if (!strncmp(penv[i],env[j],n))
        break;

      if (env[j+1] == NULL) {
        env[j+1] = g_strdup(penv[i]);
         env[j+2] = NULL;
        break;
      }
    }
    free(penv[i]);
  }
  free(penv);
} /* append_pam_environ */
#endif /* HAVE_LIBPAM */

/*
 * switch_user
 */
void
switch_user(struct passwd *pw, char *session, char *lang, char **env)
{
  char *path;
  int  fd;

  if (!pw || initgroups(pw->pw_name, pw->pw_gid) ||
        setgid(pw->pw_gid) || setuid(pw->pw_uid) || setsid() == -1)
    exit(EXIT_FAILURE);

  if (session) {
    path = g_strdup_printf("%s/.config/desktop", pw->pw_dir);

    if (strcmp(session, PREVIOUS) == 0) {
      if (!(session = (char *)session_desktop_get(path)))
        session = (char *)session_desktop_get("/etc/sysconfig/desktop");
    }
    else {
      const char *oldsession = session_desktop_get(path);

      if (oldsession) {
        if (strcmp(oldsession, session))
          session_desktop_put(path, session);
      }
      else /* resource file does not exist or DESKTOP=<session> is not set */
        session_desktop_put(path, session);
    }
    g_free(path);
  }

  if (lang) {
    path = g_strdup_printf("%s/.config/i18n", pw->pw_dir);

    if (strcmp(lang, PREVIOUS) == 0) {
      if (!(lang = (char *)session_lang_get(path)))
        lang = (char *)session_lang_get("/etc/sysconfig/i18n");
    }
    else {
      const char *oldlang = session_lang_get(path);

      if (oldlang) {
        if (strcmp(oldlang, lang))
          session_lang_put(path, lang);
      }
      else  /* resource file does not exist or LANG=<lang> is not set */
        session_lang_put(path, lang);
    }
    g_free(path);
  }

  chdir(pw->pw_dir);
  fd = open(".xsession-errors", O_WRONLY|O_CREAT|O_TRUNC,S_IRUSR|S_IWUSR);

  if (fd != -1) {
    dup2(fd, STDERR_FILENO);
    close(fd);
  }

  create_client_auth(pw->pw_dir);
  execle(XSESSION, XSESSION, session, NULL, env);

  exit(EXIT_FAILURE);
} /* switch_user */

/*
 * get_runlevel
 */
static int
get_runlevel(void)
{
  int res = 0;
  struct utmp *ut,tmp;

  setutent();
  tmp.ut_type=RUN_LVL;
  ut = getutid(&tmp);

  if (ut) {
    res = ut->ut_pid & 0xff;
    endutent();
  }

  //log_print("runlevel %c\n",res);
  return res;
} /* get_runlevel */

/*
 * graceful exit
 */
void
graceful(int code)
{
  reason = code;
  exit(0);
}

/*
 * self_xid_{get,free,check}
 */
static inline void
self_xid_get(void)
{
  Window dummy, parent;
  Display *Dpy = gdk_x11_get_default_xdisplay();
  Window Root = gdk_x11_get_default_root_xwindow();
  XQueryTree(Dpy, Root, &dummy, &parent, &self_xid, &self_xid_n);
}

static inline void
self_xid_free(void)
{
  XFree(self_xid);
  self_xid = 0;
}

static inline int
self_xid_check(XID id)
{
  int idx;

  if (self_xid)
    for (idx = 0; idx < self_xid_n; idx++)
      if (idx == self_xid[idx])
        return 1;

  return 0;
}

/*
 * stop_clients
 */
int
CatchErrors(Display *dpy, XErrorEvent *ev)
{
  return 0;
}

static void
stop_clients(void)
{
  Window dummy, parent;
  Window *children;
  unsigned int nchildren;
  unsigned int idx;
  Display *Dpy = gdk_x11_get_default_xdisplay();
  Window Root = gdk_x11_get_default_root_xwindow();

  XSync(Dpy, 0);
  XSetErrorHandler(CatchErrors);

  nchildren = 0;
  XQueryTree(Dpy, Root, &dummy, &parent, &children, &nchildren);

  for (idx = 0; idx < nchildren; idx++)
    if (children[idx] && !self_xid_check(children[idx]))
      XKillClient(Dpy, children[idx]);

  //printf("kill %d\n",i);
  XFree((char *)children);
  XSync(Dpy, 0);
  XSetErrorHandler(NULL);
} /* stop_clients */

/*
 * on_session_stop
 * on_xserver_stop
 */
static void
on_session_stop(GPid pid, gint status, gpointer data)
{
  killpg(pid, SIGHUP);
  stop_pid(pid);
  child = -1;
  int level;

  if (server > 0) {
    /* FIXME just work around bug of focus can't set */
    stop_clients();
    self_xid_free();
  }
#if HAVE_LIBPAM
  close_pam_session();
#endif

#if HAVE_LIBCK_CONNECTOR
  if (ckc != NULL) {
    DBusError error;
    dbus_error_init(&error);
    ck_connector_close_session(ckc, &error);
    unsetenv("XDG_SESSION_COOKIE");
  }
#endif
  level = get_runlevel();

  if (level=='0' || level=='6')
    graceful(2);

  interface_init();
} /* on_session_stop */

static void
on_xserver_stop(GPid pid, gint status, gpointer data)
{
  stop_pid(server);
  server = -1;
  graceful(0);
}

/*
 * {set,clear}_lock
 */
void
set_lock(void)
{
  FILE *fp;
  char *pidfile;

  pidfile = g_key_file_get_string(config, "core", "lock", 0);
  if(!pidfile) pidfile = g_strdup_printf("%s/pid", RUNDIR);
  fp = fopen(pidfile, "r");

  if (fp) {
    int pid, ret;

    ret = fscanf(fp, "%d", &pid);
    fclose(fp);

    if (ret == 1 && pid != getpid()) {
      if (kill(pid, 0) == 0 || (ret == -1 && errno == EPERM)) {
#ifdef __linux__
         /* we should only quit if the pid running is self_ */
         char path[64], buf[128];

         sprintf(path, "/proc/%d/exe", pid);
         ret = readlink(path, buf, 128);

         if (ret < 128 && ret > 0 && strstr(buf, Program))
           graceful(1);
#else
         graceful(1);
#endif	
      }
    }
  }

  if ((fp = fopen(pidfile, "w")) == NULL) {
    log_print("open lock file %s fail\n", pidfile);
    graceful(255);
  }

  fprintf(fp, "%d", getpid());
  fclose(fp);

  g_free(pidfile);
} /* set_lock */

void
clear_lock(void)
{
  FILE *fp;
  char *pidfile;

  pidfile = g_key_file_get_string(config, "core", "lock", 0);
  if(!pidfile) pidfile = g_strdup_printf("%s/pid", RUNDIR);
  fp = fopen(pidfile, "r");

  if (fp) {
    int pid, ret;

    ret = fscanf(fp, "%d", &pid);
    fclose(fp);

    if (ret == 1 && pid == getpid())
      remove(pidfile);
  }
  g_free(pidfile);
} /* clear_lock */

/*
 * set_numlock
 */
static void
set_numlock(void)
{
  Display *dpy;
  XkbDescPtr xkb;
  unsigned int mask;
  int on;
  int i;

  if (!g_key_file_has_key(config, "core", "numlock", NULL))
     return;

  on = g_key_file_get_integer(config, "core", "numlock", 0);
  dpy = gdk_x11_get_default_xdisplay();
  if(!dpy) return;

  xkb = XkbGetKeyboard( dpy, XkbAllComponentsMask, XkbUseCoreKbd );
  if(!xkb) return;

  if(!xkb->names) {
     XkbFreeKeyboard(xkb,0,True);
     return;
  }
  for(i = 0; i < XkbNumVirtualMods; i++) {
    char *s = XGetAtomName( xkb->dpy, xkb->names->vmods[i]);

    if(!s) continue;
    if(strcmp(s,"NumLock")) continue;

    XkbVirtualModsToReal( xkb, 1 << i, &mask );
    break;
  }

  XkbFreeKeyboard( xkb, 0, True );
  XkbLockModifiers( dpy, XkbUseCoreKbd, mask, (on ? mask : 0) );
} /* set_numlock */

#if HAVE_LIBCK_CONNECTOR
void
init_ck(void)
{
  ckc = ck_connector_new();
}
#endif

/*
 * auto_login
 */
int
auto_login(void)
{
  struct passwd *pw;
  char *user;
  char *pass=NULL;
  int ret;

  /* return if autologin[core] is not specified */
  user = g_key_file_get_string(config, "core", "autologin", 0);
  if(!user) return 0;

  if (semid > 0) {                /* AUTOLOGIN semaphore */
    struct sembuf sbuf = {0, 1, 0};
    int semval = semctl(semid, 0, GETVAL, 0);

    if(semval != 0) return 0;     /* allow only one AUTOLOGIN session */
    semop(semid, &sbuf, 1);
  }

#ifdef ENABLE_PASSWORD
  pass = g_key_file_get_string(config, "core", "password", 0);
#endif
  ret = gould_auth_user(user, pass, &pw);
  g_free(user);
  g_free(pass);

  if (ret != AUTH_SUCCESS)
    return 0;

  gould_do_login(pw, NULL, NULL);
  return 1;
} /* auto_login */

/*
 * stty_sane - no switching that is done by X, just conditioning
 */
void
stty_sane(void)
{
  struct termios tio;
  char device[16];
  int fd;

  sprintf(device, "/dev/tty%d", get_active_vt());
  fd = open(device, O_RDWR);

  if (fd < 0) fd = 0;
  ioctl(fd, TCGETS, &tio);
  tio.c_oflag |= (ONLCR | OPOST);
  ioctl(fd, TCSETSW, &tio);    /* fix staircase: map NL in output to CR-NL */
  if (fd != 0) close(fd);
} /* stty_sane */

/*
 * startx
 */
void
startx(void)
{
  char *cmd;
  char **argv;

  if (!getenv("DISPLAY"))
    putenv("DISPLAY=:0");

  create_server_auth();
  cmd = g_key_file_get_string(config, "core", "server", 0);

  if (!cmd) {
    log_print("server[core] not found, using: /usr/bin/X\n");
    cmd = g_strdup("/usr/bin/X");
  }
  argv = g_strsplit(cmd, " ", -1);
  g_free(cmd);

  switch( server = vfork() ) {
    case 0:
      execvp(argv[0], argv);
      log_print("exec %s fail\n", argv[0]);
      graceful(255);
      break;

    case -1:
      /* fatal error, should not restart self_ */
      log_print("fork proc fail\n");
      graceful(255);
      break;
  }
  g_strfreev(argv);
  server_watch = g_child_watch_add(server, on_xserver_stop, 0);
} /* startx */

static void
replace_env(char** env, const char* name, const char* new_val)
{
    register char** penv;
    for(penv = env; *penv; ++penv)
    {
        if(g_str_has_prefix(*penv, name))
        {
            g_free(*penv);
            *penv = g_strconcat(name, new_val, NULL);
            return;
        }
    }
    *penv = g_strconcat(name, new_val, NULL);
    *(penv + 1) = NULL;
}

/*
 * Reload
 */
void
Reload(void)
{
  if (child > 0) {
    killpg(child, SIGHUP);
    stop_pid(child);
    child = -1;
  }

  interface_clean();
#if HAVE_LIBPAM
  close_pam_session();
#endif

  if (server_watch > 0)
    g_source_remove(server_watch);

  if (server > 0) {
    stop_pid(server);
    server = -1;
  }

  clear_lock();              /* remove pidfile if self_ == getpid() */

  if (reason == 0)	     /* restart only if reason == 0 */
    execlp(self_, self_, NULL);
} /* Reload */

/*
 * sig_handler
 * set_signal
 */
static void
sig_handler(int sig)
{
  log_print("catch signal %d\n", sig);

  switch (sig) {
    case SIGTERM:
    case SIGINT:
      graceful(255);
      break;

    case SIGSEGV:
      log_sigsegv();
      graceful(255);
      break;
  }
}

void
set_signal(void)
{
    signal(SIGQUIT, sig_handler);
    signal(SIGTERM, sig_handler);
    signal(SIGKILL, sig_handler);
    signal(SIGINT, sig_handler);
    signal(SIGHUP, sig_handler);
    signal(SIGPIPE, sig_handler);
    signal(SIGUSR1, sig_handler);
    signal(SIGALRM, sig_handler);
    signal(SIGSEGV, sig_handler);
}

/*
 * gould_cur_session
 */
int
gould_cur_session(void)
{
  return child;
}

/*
 * gould_auth_user
 * gould_do_login
 */
int
gould_auth_user(char *user, char *pass, struct passwd **ppw)
{
    struct passwd *pw;
    struct spwd *sp;
    char *real;
    char *enc;
    if( !user )
        return AUTH_ERROR;
    if( !user[0] )
        return AUTH_BAD_USER;
    pw = getpwnam(user);
    endpwent();
    if( !pw )
        return AUTH_BAD_USER;
    if( !pass )
    {
        *ppw = pw;
        return AUTH_SUCCESS;
    }
    sp = getspnam(user);
    if( !sp )
        return AUTH_FAILURE;
    endspent();
    real = sp->sp_pwdp;
    if( !real || !real[0] )
    {
        if( !pass[0] )
        {
            *ppw = pw;
            return AUTH_SUCCESS;
        }
        else
            return AUTH_FAILURE;
    }
    enc = crypt(pass, real);
    if( strcmp(real, enc) )
        return AUTH_FAILURE;
    if( strstr(pw->pw_shell, "nologin") )
        return AUTH_PRIV;
    *ppw = pw;
#if HAVE_LIBPAM
    if(pamh) pam_end(pamh,0);
    if(PAM_SUCCESS != pam_start(Program, pw->pw_name, &conv, &pamh))
        pamh=NULL;
    else
    {
        user_pass[0]=user;user_pass[1]=pass;
        pam_authenticate(pamh,PAM_SILENT);
        user_pass[0]=0;user_pass[1]=0;
    }
#endif
    return AUTH_SUCCESS;
} /* gould_auth_user */

void
gould_do_login(struct passwd *pw, char *session, char *lang)
{
  gboolean alloc_session = FALSE, alloc_lang = FALSE;
  int pid;

  if (!session || !session[0] || !lang || !lang[0]) {
    GKeyFile *dmrc = g_key_file_new();
    char *path = g_strdup_printf("%s/.dmrc", pw->pw_dir);

    g_key_file_load_from_file(dmrc, path, G_KEY_FILE_NONE, 0);
    g_free(path);

    if (!session || !session[0]) {
      session = g_key_file_get_string(dmrc, "Desktop", "Session", NULL);

      if (strcmp(session, "default") != 0)
        alloc_session = TRUE;
      else {
        g_free(session);
        session = PREVIOUS;
      }
    }

    if (!lang || !lang[0]) {
      lang = g_key_file_get_string(dmrc, "Desktop", "Language", NULL);
      alloc_lang = TRUE;
    }
    g_key_file_free(dmrc);
  }
    
  if (pw->pw_shell[0] == '\0') {
    setusershell();
    strcpy(pw->pw_shell, getusershell());
    endusershell();
  }
#if HAVE_LIBPAM
  setup_pam_session(pw, session);
#endif
#if HAVE_LIBCK_CONNECTOR
  if (ckc != NULL) {
    DBusError error;
    char device[16], *d, *n;
    sprintf(device, "/dev/tty%d", get_active_vt());
    dbus_error_init(&error);
    d = device; n = getenv("DISPLAY");
    if (ck_connector_open_session_with_parameters(ckc, &error,
                                                  "unix-user", &pw->pw_uid,
                                                  "x11-display-device", &d,
                                                  "x11-display", &n,
                                                  NULL))
      setenv("XDG_SESSION_COOKIE", ck_connector_get_cookie(ckc), 1);
  }
#endif
  self_xid_get();
  child = pid = fork();

  g_key_file_set_string(cache, "display", "username", pw->pw_name);
  saveconfig(cache, CONFIG_CACHE);

  if (child == 0) {
    char **env, *path;
    int  idx, n_env = g_strv_length(environ);

    /* copy all environment variables and override some of them */
    env = g_new(char*, n_env + 1 + 13);
    for (idx = 0; idx < n_env; idx++) env[idx] = g_strdup(environ[idx]);
    env[idx] = NULL;

    replace_env(env, "HOME=", pw->pw_dir);
    replace_env(env, "SHELL=", pw->pw_shell);
    replace_env(env, "USER=", pw->pw_name);
    replace_env(env, "LOGNAME=", pw->pw_name);

    /* override $PATH if needed */
    path = g_key_file_get_string(config, "core", "path", 0);

    if (G_UNLIKELY(path) && path[0])  /* if PATH is specified in config file */
      replace_env(env, "PATH=", path); /* override $PATH with config value */
    g_free(path);

    /* optionally override $LANG, $LC_MESSAGES, and $LANGUAGE */
    if (lang && lang[0]) {
      if (strcmp(lang, PREVIOUS) != 0) {
        replace_env(env, "LANG=", lang);
        replace_env(env, "LC_MESSAGES=", lang);
        replace_env(env, "LANGUAGE=", lang);
      }
    } 
#if HAVE_LIBPAM
    append_pam_environ(env);
    pam_end(pamh, 0);
#endif
    switch_user(pw, session, lang, env);
    graceful(4);
  }

  if (alloc_session)
    g_free(session);

  if (alloc_lang)
    g_free(lang);

  g_child_watch_add(pid, on_session_stop, 0);
} /* gould_do_login */

/*
 * gould_do_suspend
 * gould_do_shutdown
 * gould_do_reboot
 * gould_do_exit
 */
void
gould_do_suspend(void)
{
  char *cmd;
  cmd = g_key_file_get_string(config, "cmd", "suspend", 0);
  if(!cmd) cmd = g_strdup("/sbin/suspend");
  g_spawn_command_line_sync(cmd, 0, 0, 0, 0);
  g_free(cmd);
}

void
gould_do_shutdown(void)
{
  char *cmd;
  cmd = g_key_file_get_string(config, "cmd", "shutdown", 0);
  if(!cmd) cmd = g_strdup("/sbin/shutdown -h now");
  g_spawn_command_line_async(cmd, 0);
  g_free(cmd);
  graceful(1);
}

void
gould_do_reboot(void)
{
  char *cmd;
  cmd = g_key_file_get_string(config, "cmd", "reboot", 0);
  if(!cmd) cmd = g_strdup("/sbin/reboot");
  g_spawn_command_line_async(cmd, 0);
  g_free(cmd);
  graceful(1);
}

void
gould_do_exit(void)
{
  if(semid > 0) semctl(semid, 0, IPC_RMID);
  graceful(2);
}

/*
 * showtime
 */
void
showtime()
{
  int idx, maxtry = 20; /* maxtry => max times we try to connect to X */

  mkdir(RUNDIR, 0700);
  cache = g_key_file_new();
  g_key_file_load_from_file(cache, CONFIG_CACHE, G_KEY_FILE_NONE, NULL);

  config = g_key_file_new();
  g_key_file_load_from_file(config, CONFIG_FILE, G_KEY_FILE_NONE, NULL);
  set_lock();

  set_signal();
  atexit(Reload);
  stty_sane();
  startx();		/* start X and check that we got started */

  for (idx = 0; idx < maxtry; idx++) {
    if(gdk_init_check(0, 0)) break;
    g_usleep(50 * 1000);
  }

  if (idx >= maxtry) {
    printf("%s: cannot connect to X server.. exiting.\n", Program);
    exit(EXIT_FAILURE);
  }

  set_numlock();
#if HAVE_LIBCK_CONNECTOR
  init_ck();
#endif

  /* create and initialize AUTOLOGIN semaphore */
  if ((semid = semget(AUTOLOGIN, 0, 0)) == -1) {
    if ((semid = semget(AUTOLOGIN, 1, IPC_CREAT | IPC_EXCL |
                                      S_IRUSR | S_IWUSR | S_IRGRP)) == -1)
      log_print("AUTOLOGIN semaphore creation failure\n");
    else {
      struct sembuf sbuf = {0, -1, SEM_UNDO | IPC_NOWAIT};

      if (semop(semid, &sbuf, 1) == -1)
        log_print("AUTOLOGIN semaphore initialization failure\n");
    }
  }

  auto_login();
  interface_main();
  graceful(0);
} /* showtime */

/*
 * main method implementation
 */
int
main(int argc, char *argv[])
{
  int flag;

  self_ = argv[0];
  Program = basename(self_);    /* strip leading path for progname */
  Release = "0.3.1";
  opterr = 0;                   /* disable invalid option messages */

  while ((flag = getopt (argc, argv, "dhv")) != -1)
    switch (flag) {
      case 'd':
        (void)daemon(1, 1);
        break;

      case 'v':
        printf("%s version %s %s\n", Program, Release, License);
        exit(0);

      case 'h':
      default:
        printf(Usage, Program, Release, License, Program);
        exit(0);
    }

  if (getuid() != 0) {   /* starting here only root is allowed */
    printf("%s: only root is allowed to use this program\n", Program);
    exit(EXIT_FAILURE);
  }

  showtime();			/* show the interface */
  return 0;
} /* main */

