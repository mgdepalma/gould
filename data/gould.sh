##
# /etc/profile.d/ GOULD additions
#
GNOME=/opt/gnome

if [ -n "$LD_LIBRARY_PATH" ]; then
  echo $LD_LIBRARY_PATH | grep -q $GNOME/lib || \
  LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$GNOME/lib
else
  LD_LIBRARY_PATH=$GNOME/lib
fi

echo $PATH | grep -q $GNOME/bin || PATH="$PATH:$GNOME/bin"
## 
