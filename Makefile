MAKE = make
CFLAGS = -g -fpack-struct -Wall
CC = gcc

all: amideco bcpvpd

AMIDECO_OBJS = lh5_extract.o amideco.o
amideco: $(AMIDECO_OBJS)
	$(CC) $(CFLAGS) $(AMIDECO_OBJS) -o amideco

bcpvpd: bcpvpd.c
	$(CC) $(CFLAGS) bcpvpd.c -o bcpvpd

# just here to easily verify the functionality of the lh5 routine
LH5_TEST_OBJS = lh5_extract.o lh5_test.o
lh5_test: $(LH5_TEST_OBJS)
	$(CC) $(CFLAGS) $(LH5_TEST_OBJS) -o lh5_test

clean: 
	rm -f *.o
	rm -f amideco
	rm -f bcpvpd
	rm -f lh5_test
