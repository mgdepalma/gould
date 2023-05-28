#!/bin/sh
##
# gsession - X Window session startup script.
#
# 2008-08-08 Generations Linux <bugs@softcraft.org>
#
PROGRAM=$(basename $0)
VERSION="1.0.5"

# exit on error condition
abend()
{
  echo "$PROGRAM ERROR $*" | tee -a $logfile
  exit 1
}

# show program usage
usage()
{
  cat << EOF
usage: $PROGRAM [-v] [-h]

 $PROGRAM sets up a desktop environment by consulting parameters
 set in /etc/sysconfig/desktop system-wide adding or overriding each
 parameter with $HOME/.config/desktop individual if available.

 OPTIONS
  -h		show usage and exit; what you are reading
  -o <logfile>	error log file (default $HOME/.xsession-errors)
  -v		show program version and exit

 PARAMETERS (/etc/sysconfig/desktop [$HOME/.config/desktop])

  AUTOSTART       list of application to start before WINDOWMANAGER
  BACKGROUND      graphic file to use for screen background
  WINDOWMANAGER   window manager application

 $PROGRAM is provided under the terms and conditions of the GNU
 Public License (GPL) version 3.  
EOF
}

# show program version
version()
{
  cat << EOF

 $PROGRAM version $VERSION [GNU Public License (GPL) version 3]

EOF
}

# process command line options
OPTERR=0

while getopts "hvo:" flag
do
  case "$flag" in
    h) usage; exit 0 ;;
    o) logfile=$OPTARG ;;
    v) version; exit 0 ;;
    *) options=$@ ;;
  esac
done

shift $((OPTIND-1)); OPTIND=1
last=$@

# parse options and complement local variables
[ -n "$logfile" ] || logfile=$HOME/.xsession-errors

##
# main program method
[ -r /etc/sysconfig/desktop ] && . /etc/sysconfig/desktop
[ -r $HOME/.config/desktop ] && . $HOME/.config/desktop

[ -n "$WINDOWMANAGER" ] || WINDOWMANAGER="openbox"
[ `type -p $WINDOWMANAGER` ] || abend "$WINDOWMANAGER: command not found."

for APPLICATION in $AUTOSTART; do
  if [ `type -p $APPLICATION` ]; then 
    $APPLICATION &
  fi
done

[ -n "$BACKGROUND" -a -r $BACKGROUND ] && \
gconftool -t string -s /desktop/gnome/background/picture_filename $BACKGROUND
${GPANEL:-gpanel} &

# exec window manager
$WINDOWMANAGER

