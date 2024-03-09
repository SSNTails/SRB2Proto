##########################################################
#
# $Id:$
#
# $Log:$
#
#

CC=cc
CFLAGS=-O2 -K pentium -DNORMALUNIX -DSCOUW2
LDFLAGS=
LIBS=-lm

O=unixware7

all:	 $(O)/sndserver

clean:
	rm -f $(O)/*

# Target
$(O)/sndserver: \
	$(O)/soundsrv.o \
	$(O)/sounds.o \
	$(O)/wadread.o \
	$(O)/strcmp.o \
	$(O)/linux.o
	$(CC) $(CFLAGS) $(LDFLAGS) \
	$(O)/soundsrv.o \
	$(O)/sounds.o \
	$(O)/wadread.o \
	$(O)/strcmp.o \
	$(O)/linux.o -o $(O)/sndserver $(LIBS)
	echo make complete.

# Rule
$(O)/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
