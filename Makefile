MAKE = make
CFLAGS = -g -fpack-struct -Wall
CC = gcc

all: amideco lh5_extract

amideco: amideco.c
	$(CC) $(CFLAGS) amideco.c -o amideco

lh5_extract: lh5_extract.c
	$(CC) $(CFLAGS) lh5_extract.c -o lh5_extract

clean: 
	rm -f amideco
	rm -f lh5_extract
