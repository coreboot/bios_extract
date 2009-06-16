#
# Main Makefile
# (c) 2002
#

MAKE = make

phnxdeco:
	DEMO=""
	gcc -DDEBUG $(DEMO) phnxdeco.c phnxfunc.o kernel.o -s -fpack-struct -o phnxdeco

phnxdeco-demo:
	DEMO=1
	gcc -DDEBUG=$(DEMO) phnxdeco.c phnxfunc.o kernel.o -s -fpack-struct -o phnxdeco-demo

phnxfunc:
	gcc phnxfunc.c -c -o phnxfunc.o -fpack-struct

kernel:
	gcc kernel.c -c -o kernel.o -fpack-struct

clean: 
	rm -f *.o
	rm -f phnxdeco

rebuild:
	make clean
	make phnxfunc
	make kernel
	make phnxdeco

rebuild-demo:
	make clean
	make phnxdeco-demo
