#
# Main Makefile
# (c) 2002
#

MAKE = make
CFLAGS = -g -fpack-struct -Wall
CC = gcc
OBJS = amideco.o

.o: $*.c
	$(CC) $(CFLAGS) -c $*.c

all: amideco

amideco: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o amideco

clean: 
	rm -f *.o
	rm -f amideco

rebuild:
	$(MAKE) clean
	$(MAKE) amideco
