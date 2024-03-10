##########################################################
#
# $Id:$
#
# $Log:$
#
#

CC=gcc
# use this for an older GNU C 2.7.2
#CFLAGS=-O2 -m486 -b elf -Wall -DNORMALUNIX -DSCOOS5
# use this for EGCS 1.0.1
CFLAGS=-O2 -mpentium -Wall -DNORMALUNIX -DSCOOS5
LDFLAGS=
LIBS=-lm

O=openserver5

all:	 $(O)/sndserver

clean:
	rm -f $(O)/*

# Target
$(O)/sndserver: \
	$(O)/soundsrv.o \
	$(O)/sounds.o \
	$(O)/wadread.o \
	$(O)/linux.o
	$(CC) $(CFLAGS) $(LDFLAGS) \
	$(O)/soundsrv.o \
	$(O)/sounds.o \
	$(O)/wadread.o \
	$(O)/linux.o -o $(O)/sndserver $(LIBS)
	echo make complete.

# Rule
$(O)/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
