MAKE = make
CFLAGS = -g -fpack-struct -Wall -O0
CC = gcc

all: bios_extract bcpvpd

BIOS_EXTRACT_OBJS = lh5_extract.o ami.o award.o phoenix.o bios_extract.o
bios_extract: $(BIOS_EXTRACT_OBJS)
	$(CC) $(CFLAGS) $(BIOS_EXTRACT_OBJS) -o bios_extract

BCPVPD_OBJS = lzss_extract.o bcpvpd.o
bcpvpd: $(BCPVPD_OBJS)
	$(CC) $(CFLAGS) $(BCPVPD_OBJS) -o bcpvpd

# just here to easily verify the functionality of the lh5 routine
LH5_TEST_OBJS = lh5_extract.o lh5_test.o
lh5_test: $(LH5_TEST_OBJS)
	$(CC) $(CFLAGS) $(LH5_TEST_OBJS) -o lh5_test

clean: 
	rm -f *.o
	rm -f bios_extract
	rm -f bcpvpd
	rm -f lh5_test
