#!/bin/sh -f
##
# X Window System wrapper script.
#
# 2008-08-08 Generations Linux <info@softcraft.org>
#
[ -r /etc/sysconfig/xserver ] && . /etc/sysconfig/xserver
[ -r $HOME/.config/xserver ] && . $HOME/.config/xserver

[ -n "$XSERVER" ] || XSERVER="Xorg"
[ `type -p $XSERVER` ] || XSERVER="Xvesa"

[ -z "$XOPTION" -a "$XSERVER" = "Xvesa" ] && \
XOPTION="-screen 1024x768x16 -shadow -3button"
#XOPTION="-br -screen 1024x768x32 -shadow -2button"

exec $XSERVER $XOPTION
