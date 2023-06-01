##
# Generations Linux <bugs@softcraft.org>
# src/modules/Makefile.am for the GOULD project
#
MAINTAINERCLEANFILES = Makefile.in

PKG_DEPS   = "gtk+-2.0 libxml-2.0"
PKG_CFLAGS = `pkg-config --cflags $(PKG_DEPS)`
PKG_LIBS   = `pkg-config --libs $(PKG_DEPS)`

ALL_CFLAGS = -I../common -I../desktop -Wno-deprecated-declarations
AM_CFLAGS  = $(PKG_CFLAGS) $(ALL_CFLAGS) -Wall -g -O -pipe
AM_LDFLAGS = $(PKG_LIBS) -module -avoid-version 

LIBS = -L../common -lgould

pkglib_LTLIBRARIES = \
clock.la \
battery.la \
mixer.la \
mountd.la \
showdesktop.la \
splash.la \
$(NULL)

# program source dependencies
mixer_la_LIBADD = -lasound

##
# Override the install-binPROGRAM target.
pkglibdir = /usr/libexec/@PACKAGE@

install-binPROGRAMS: $(bin_PROGRAMS)
	@echo "Installing plugin modules in $(pkglibdir)"
	$(mkinstalldirs) $(DESTDIR)$(pkglibdir)
	$(INSTALL_PROGRAM) $(bin_PROGRAMS) "$(DESTDIR)$(pkglibdir)"
#/
