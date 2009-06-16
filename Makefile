MAKE = make
CFLAGS = -g -fpack-struct -Wall
CC = gcc

all: bios_extract bcpvpd

BIOS_EXTRACT_OBJS = lh5_extract.o ami.o bios_extract.o
bios_extract: $(BIOS_EXTRACT_OBJS)
	$(CC) $(CFLAGS) $(BIOS_EXTRACT_OBJS) -o bios_extract

bcpvpd: bcpvpd.c
	$(CC) $(CFLAGS) bcpvpd.c -o bcpvpd

# just here to easily verify the functionality of the lh5 routine
LH5_TEST_OBJS = lh5_extract.o lh5_test.o
lh5_test: $(LH5_TEST_OBJS)
	$(CC) $(CFLAGS) $(LH5_TEST_OBJS) -o lh5_test

clean: 
	rm -f *.o
	rm -f bios_extract
	rm -f bcpvpd
	rm -f lh5_test
