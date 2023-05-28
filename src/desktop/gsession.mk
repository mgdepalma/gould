##
# Copyright (C) Generations Linux <bugs@softcraft.org>
# gsession make description file
#
CFLAGS=-g -O

all: gsession

gsession: gsession.c gsession.h
	$(CC) $(CFLAGS) gsession.c -o gsession

clean:
	rm -f gsession
