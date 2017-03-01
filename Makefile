MAKE = make
CFLAGS ?= -g -fpack-struct -Wall -O0
CC ?= gcc

all: bios_extract bcpvpd ami_slab xfv

BIOS_EXTRACT_OBJS = lh5_extract.o ami.o award.o phoenix.o bios_extract.o compat.o
bios_extract: $(BIOS_EXTRACT_OBJS)
	$(CC) $(CFLAGS) $(BIOS_EXTRACT_OBJS) -o bios_extract

BCPVPD_OBJS = lzss_extract.o bcpvpd.o
bcpvpd: $(BCPVPD_OBJS)
	$(CC) $(CFLAGS) $(BCPVPD_OBJS) -o bcpvpd

AMISLAB_OBJS = ami_slab.o
ami_slab: $(AMISLAB_OBJS)
	$(CC) $(CFLAGS) $(AMISLAB_OBJS) -o ami_slab

XFV_OBJS = xfv/Decompress.o xfv/efidecomp.o
xfv: $(XFV_OBJS)
	$(CC) -I xfv/ $(CFLAGS) -o xfv/efidecomp $(XFV_OBJS)

# just here to easily verify the functionality of the lh5 routine
LH5_TEST_OBJS = lh5_extract.o lh5_test.o
lh5_test: $(LH5_TEST_OBJS)
	$(CC) $(CFLAGS) $(LH5_TEST_OBJS) -o lh5_test

gitconfig:
	[ -d .git ]
	mkdir -p .git/hooks
	for hook in commit-msg pre-commit ; do							\
		if [ util/gitconfig/$$hook -nt .git/hooks/$$hook -o				\
		! -x .git/hooks/$$hook ]; then				       			\
			sed -e "s,%MAKE%,$(MAKE),g" util/gitconfig/$$hook > .git/hooks/$$hook;	\
			chmod +x .git/hooks/$$hook;		  				\
		fi;			  							\
	done
	git config remote.origin.push HEAD:refs/for/master
	(git config --global --includes user.name >/dev/null && git config --global --includes user.email >/dev/null) || (printf 'Please configure your name and email in git:\n\n git config --global user.name "Your Name Comes Here"\n git config --global user.email your.email@example.com\n'; exit 1)

clean: 
	rm -f *.o
	rm -f bios_extract
	rm -f bcpvpd
	rm -f lh5_test
	rm -f ami_slab
	rm -f xfv/efidecomp xfv/*.o

.PHONY: all bios_extract bcpvpd ami_slab efidecomp lh5_test clean gitconfig
