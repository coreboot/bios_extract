MAKE = make
CFLAGS = -g -fpack-struct -Wall
CC = gcc

all: amideco

amideco: lh5_extract.o amideco.c
	$(CC) $(CFLAGS) lh5_extract.o amideco.c -o amideco

lh5_extract.o: lh5_extract.c
	$(CC) $(CFLAGS) -c lh5_extract.c -o lh5_extract.o

lh5_test: lh5_extract.o lh5_test.c
	$(CC) $(CFLAGS) lh5_extract.o lh5_test.c -o lh5_test

clean: 
	rm -f *.o
	rm -f amideco
	rm -f lh5_test
