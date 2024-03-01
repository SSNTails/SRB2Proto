##########################################################
#
# $Id:$
#
# $Log:$
#
#

CC=gcc
CFLAGS=-O2 -m486 -Wall -DNORMALUNIX -DLINUX
LDFLAGS=
LIBS=-lm

O=linux

all:	 $(O)/llsndserv

clean:
	rm -f $(O)/*

# Target
$(O)/llsndserv: \
	$(O)/soundsrv.o \
	$(O)/sounds.o \
	$(O)/wadread.o \
	$(O)/linux.o
	$(CC) $(CFLAGS) $(LDFLAGS) \
	$(O)/soundsrv.o \
	$(O)/sounds.o \
	$(O)/wadread.o \
	$(O)/linux.o -o $(O)/llsndserv $(LIBS)
	echo make complete.

# Rule
$(O)/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
