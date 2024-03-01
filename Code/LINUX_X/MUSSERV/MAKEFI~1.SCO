#
# Makefile for musserver 1.4
#

CC	= gcc
# use this for an older GNU C 2.7.2
#CFLAGS	= -I. -Wall -O2 -m486 -b elf -DSCOOS5
# use this for EGCS 1.0.1
CFLAGS	= -I. -Wall -O2 -mpentium -DSCOOS5
LDFLAGS	=

#############################################
# Nothing below this line should be changed #
#############################################

O=openserver5

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
