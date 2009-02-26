MAKE = make
CFLAGS = -g -fpack-struct -Wall
CC = gcc

all: amideco

amideco: amideco.c
	$(CC) $(CFLAGS) amideco.c -o amideco

clean: 
	rm -f amideco

rebuild:
	$(MAKE) clean
	$(MAKE) amideco
