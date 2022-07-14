##
# Generations Linux <bugs@softcraft.org>
# data/Makefile.am for the GOULD project
#
NULL =
SUBDIRS =
MAINTAINERCLEANFILES = Makefile.in
CLEANFILES = gould.conf panel

##
# Directories where the DATA goes.
goulddir = $(datadir)/gould
gould_icondir = $(goulddir)/pixmaps
gould_themedir = $(goulddir)/themes/default
gould_pamdir = /etc/pam.d

##
# Definitions of the DATA
gould_DATA = \
        gould.conf \
        panel \
        $(NULL)

gould_icon_DATA = pixmaps/*.png

gould_pam_DATA = gould.pam

gould_theme_DATA= \
        themes/default/admin.png \
        themes/default/greeter.jpg \
        themes/default/greeter.ui \
        themes/default/gtkrc \
        themes/default/index.theme \
        themes/default/lang.png \
        themes/default/login.png \
        themes/default/quit.png \
        themes/default/reboot.png \
        themes/default/secure.png \
        themes/default/session.png \
        themes/default/shadow.png \
        themes/default/shutdown.png \
        themes/default/suspend.png \
        themes/default/user.png \
        $(NULL)

EXTRA_DIST= \
        $(gould_DATA) \
        $(gould_icon_DATA) \
        $(gould_theme_DATA) \
        $(NULL)

##
# Rules governing the DATA
gould.conf: gould.conf.in
	sed 's,@LIBEXECDIR@,$(bindir),' gould.conf.in > $@

panel: panel.xml
	cp -p panel.xml $@

install-gouldDATA: $(gould_DATA)
	@echo "Installing configuration file in $(goulddir)"
	$(mkinstalldirs) $(DESTDIR)$(goulddir)
	$(INSTALL_DATA) $(gould_DATA) $(DESTDIR)$(goulddir)

install-data-hook:
	$(INSTALL_DATA) $(gould_pam_DATA) $(DESTDIR)$(gould_pamdir)/gould

