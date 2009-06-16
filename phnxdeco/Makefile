CFLAGS = -g -fpack-struct -Wall
CC = gcc

all: phnxdeco

clean:
	rm -f *.o
	rm phnxdeco

phnxdeco: phnxdeco.c phnxfunc.o kernel.o
	$(CC) $(CFLAGS) phnxfunc.o kernel.o phnxdeco.c -o phnxdeco
