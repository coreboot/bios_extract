#
# Main Makefile
# (c) 2002
#

MAKE = make

amideco:
	gcc amideco.c amifunc.o kernel.o -s -fpack-struct -o amideco

amifunc:
	gcc amifunc.c -c -o amifunc.o -fpack-struct

kernel:
	gcc kernel.c -c -o kernel.o -fpack-struct

clean: 
	rm -f *.o
	rm -f amideco

rebuild:
	make clean
	make amifunc
	make kernel
	make amideco
