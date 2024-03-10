#
# Makefile for musserver 1.4
#

CC	= cc
CFLAGS	= -I. -O2 -K pentium -DSCOUW7
LDFLAGS	=

#############################################
# Nothing below this line should be changed #
#############################################

O=unixware7

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
