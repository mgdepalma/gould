dnl Template file for GOULD (GNOME Ultra Light Desktop)
dnl
dnl Copyright (C) Generations Linux <bugs@softcraft.org>
dnl
dnl This program is free software; you can redistribute it and/or modify
dnl it under the terms of the GNU General Public License as published by
dnl the Free Software Foundation; either version 3 of the License, or
dnl (at your option) any later version.
dnl
dnl This program is distributed in the hope that it will be useful,
dnl but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
dnl GNU General Public License for more details.
dnl
dnl You should have received a copy of the GNU General Public License
dnl along with this program; if not, write to the Free Software Foundation,
dnl Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
dnl

AC_INIT(gould, 1.2.3, http://softcraft.org/projects/gould/)
AC_CONFIG_HEADER([config.h])
AM_INIT_AUTOMAKE

dnl Increment if the interface has additions, changes, removals.
LIBGOULD_CURRENT=3
LIBGOULD_REVISION=3
LIBGOULD_AGE=2

AC_SUBST(LIBGOULD_CURRENT)
AC_SUBST(LIBGOULD_REVISION)
AC_SUBST(LIBGOULD_AGE)

dnl Locate required external software
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_LIBTOOL
#AC_PROG_INTLTOOL
AC_PROG_MAKE_SET
AC_PROG_RANLIB

dnl Check for required libraries.
AC_CHECK_LIB([crypt], [crypt])

dnl Check for Pluggable Authentication Modules
AC_ARG_WITH(pam,AC_HELP_STRING([--without-pam],[build without pam]),
[],[AC_CHECK_LIB([pam], [pam_open_session])])

pkg_modules="gtk+-2.0 >= 2.18.9 libxml-2.0 >= 2.7.6"
##pkg_modules="gtk+-3.0 >= 3.8.2, libxml-2.0 >= 2.7.6"
PKG_CHECK_MODULES(PACKAGE, [$pkg_modules])
AC_SUBST(GTK_CFLAGS)
AC_SUBST(GTK_LIBS)

# Check for ISO Codes (taken from gdm)
AC_MSG_CHECKING([whether iso-codes has iso-639 domain])
if $PKG_CONFIG --variable=domains iso-codes | grep 639 >/dev/null ; then
  AC_MSG_RESULT([yes])
else
  AC_MSG_RESULT([no])
fi
ISO_CODES=iso-codes

dnl Check for system header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([dirent.h locale.h shadow.h stdlib.h string.h unistd.h])
AC_HEADER_TIME

dnl Check for X headers and libraries.
AC_PATH_X([X11], [X11/Xlib.h], [XOpenDisplay(NULL)])
AC_PATH_XTRA

test x$no_x = "xyes" && AC_MSG_ERROR([gould requires the X Window System libraries and headers.])

test x$x_includes = "x" && x_includes="/usr/include"
test x$x_libraries = "x" && x_libraries="/usr/lib"

CFLAGS="$CFLAGS $X_CFLAGS"
CXXFLAGS="$CXXFLAGS $X_CFLAGS"
LIBS="$LIBS $X_LIBS"
LDFLAGS="$LDFLAGS $LIBS"

dnl Determine the return type of signal handlers.
AC_TYPE_SIGNAL

dnl Determine if maintainer portions of the Makefiles should be included.
dnl AM_MAINTAINER_MODE

dnl Determine default prefix.
if test "x$prefix" = "xNONE"; then
  prefix="/usr/local"
fi
if test "x$exec_prefix" = "xNONE"; then
 exec_prefix=$prefix
fi
includedir=$prefix/include/gould

dnl Default installation prefix.
DEFAULT_PREFIX="$prefix"
AC_SUBST(DEFAULT_PREFIX)

dnl Native Language Support using GNU gettext.
if test x"$USE_NLS" = "xyes"; then
  AC_DEFINE([USE_NLS], [1], [Native Language Support])
fi

AC_SUBST(USE_NLS)
ALL_LINGUAS="de es fr id it ja"

dnl Generated configuration files.
AC_CONFIG_FILES([
  Makefile
  src/Makefile
  src/common/Makefile
  src/modules/Makefile
  src/programs/Makefile
  src/system/Makefile
  src/xdm/Makefile
  po/Makefile.in
  data/Makefile
  libgould.pc
])
AC_OUTPUT

dnl Print resulting build configuration.
echo "
Configuration for $PACKAGE $VERSION
-----------------------------------

  GTK+ version .................... : `pkg-config --modversion gtk+-2.0`
  GLIB version .................... : `pkg-config --modversion glib-2.0`
  libxml version .................. : `pkg-config --modversion libxml-2.0`
  NLS/gettext ..................... : $USE_NLS
  prefix .......................... : $prefix
  sysconfdir ...................... : $sysconfdir
  datarootdir ..................... : $datarootdir
  localstatedir ................... : $localstatedir

  Now type 'make' to build $PACKAGE $VERSION,
  and then type 'make install' for installation.
"
