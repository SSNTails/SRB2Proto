#
# Makefile for musserver 1.4
#

CC	= gcc
CFLAGS	= -I. -Wall -O2 -m486 -DSCOUW2
LDFLAGS	=

#############################################
# Nothing below this line should be changed #
#############################################

O=unixware2

all: ${O}/musserver

${O}/musserver: \
	${O}/musserver.o \
	${O}/readwad.o \
	${O}/playmus.o \
	${O}/sequencer.o \
	${O}/usleep.o
	${CC} ${CFLAGS} ${LDFLAGS} \
	${O}/musserver.o \
	${O}/readwad.o \
	${O}/playmus.o \
	${O}/sequencer.o \
	${O}/usleep.o -o ${O}/musserver

clean:
	rm -f ${O}/*

${O}/%.o: %.c
	${CC} ${CFLAGS} -c $< -o $@
