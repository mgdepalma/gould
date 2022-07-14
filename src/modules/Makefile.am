##
# Generations Linux <bugs@softcraft.org>
# src/modules/Makefile.am for the GOULD project
#
MAINTAINERCLEANFILES = Makefile.in
PKG_CFLAGS = `pkg-config --cflags gtk+-2.0 libxml-2.0`
PKG_LIBS = `pkg-config --libs gtk+-2.0 libxml-2.0`

AM_CFLAGS = $(PKG_CFLAGS) -I../common -I../programs -Wall -g -O -pipe
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
## Override the install-binPROGRAM target.
##
ModulesDir = $(libdir)/modules/@PACKAGE@

install-binPROGRAMS: $(bin_PROGRAMS)
	@echo "Installing plugin modules in $(ModulesDir)"
	$(mkinstalldirs) $(DESTDIR)$(ModulesDir)
	$(INSTALL_PROGRAM) $(bin_PROGRAMS) "$(DESTDIR)$(ModulesDir)"
