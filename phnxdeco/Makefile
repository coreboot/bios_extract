CFLAGS = -g -fpack-struct -Wall
CC = gcc

OBJS = kernel.o phnxdeco.o

all: phnxdeco

clean:
	rm -f *.o
	rm -f phnxdeco

phnxdeco: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o phnxdeco
