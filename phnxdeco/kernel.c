/**********************************************
***
***
***	These decompression routines based on
***	LZHUFF algo from LHA archiver
***
***
***********************************************/

#include	<stdio.h>
#include	<stdlib.h>
#include	<memory.h>

#define UCHAR_MAX ((1<<(sizeof(unsigned char)*8))-1)
#define CHAR_BIT  8

#define CBIT 9
#define USHRT_BIT 16		/* (CHAR_BIT * sizeof(ushort)) */

#define MAX_DICBIT    13
#define MAX_DICSIZ (1 << MAX_DICBIT)
#define MATCHBIT   8
#define MAXMATCH 256
#define THRESHOLD  3
#define NC (UCHAR_MAX + MAXMATCH + 2 - THRESHOLD)

#define NP (MAX_DICBIT + 1)
#define NT (USHRT_BIT + 3)
#define PBIT 4
#define TBIT 5
#define NPT 0x80

unsigned long origsize, compsize;
unsigned short dicbit;
unsigned short maxmatch;
unsigned long count;
unsigned short loc;
unsigned char *text;

static unsigned short dicsiz;

static unsigned char subbitbuf, bitcount;

unsigned short crc, bitbuf;
int prev_char;
long reading_size;

unsigned short left[2 * NC - 1], right[2 * NC - 1];
unsigned char c_len[NC], pt_len[NPT];
unsigned short c_table[4096], c_code[NC], pt_table[256], pt_code[NPT];
static unsigned char *buf;
static unsigned short bufsiz;
static unsigned short blocksize;

FILE *infile, *outfile;

typedef struct {
    FILE *infile;
    FILE *outfile;
    unsigned long original;
    unsigned long packed;
    int dicbit;
    int method;
} interfacing;


static short c, n, tblsiz, len, depth, maxdepth, avail;
static unsigned short codeword, bit, *tbl;
static unsigned char *blen;

/****************************************/
/*	mktbl()				*/
/****************************************/
static short mktbl(void)
{
    short i;

    if (len == depth) {
	while (++c < n)
	    if (blen[c] == len) {
		i = codeword;
		codeword += bit;
		if (codeword > tblsiz) {
		    printf("\nBad Table!");
		    exit(1);
		}
		while (i < codeword)
		    tbl[i++] = c;
		return c;
	    }
	c = -1;
	len++;
	bit >>= 1;
    }
    depth++;
    if (depth < maxdepth) {
	(void) mktbl();
	(void) mktbl();
    } else if (depth > USHRT_BIT) {
	{
	    printf("\nBad Table [2]");
	    exit(1);
	}
    } else {
	if ((i = avail++) >= 2 * n - 1) {
	    printf("\nBad Table [3]");
	    exit(1);
	}
	left[i] = mktbl();
	right[i] = mktbl();
	if (codeword >= tblsiz) {
	    printf("\nBad Table [4]");
	    exit(1);
	}
	if (depth == maxdepth)
	    tbl[codeword++] = i;
    }
    depth--;
    return i;
}

/****************************************/
/*	make_table()			*/
/****************************************/
void make_table(short nchar, unsigned char bitlen[], short tablebits, unsigned short table[])
{
    n = avail = nchar;
    blen = bitlen;
    tbl = table;
    tblsiz = 1U << tablebits;
    bit = tblsiz / 2;
    maxdepth = tablebits + 1;
    depth = len = 1;
    c = -1;
    codeword = 0;
    (void) mktbl();		/* left subtree */
    (void) mktbl();		/* right subtree */
    if (codeword != tblsiz) {
	printf("\nBad Table [5]");
	exit(1);
    }
}



/****************************************/
/*	fillbif()			*/
/****************************************/
void fillbuf(unsigned char n)
{				/* Shift bitbuf n bits left, read n bits */
    while (n > bitcount) {
	n -= bitcount;
	bitbuf = (bitbuf << bitcount) + (subbitbuf >> (CHAR_BIT - bitcount));
	if (compsize != 0) {
	    compsize--;
	    subbitbuf = (unsigned char) getc(infile);
	} else
	    subbitbuf = 0;
	bitcount = CHAR_BIT;
    }
    bitcount -= n;
    bitbuf = (bitbuf << n) + (subbitbuf >> (CHAR_BIT - n));
    subbitbuf <<= n;
}


/****************************************/
/*	getbits()			*/
/****************************************/
unsigned short getbits(unsigned char n)
{
    unsigned short x;

    x = bitbuf >> (2 * CHAR_BIT - n);
    fillbuf(n);
    return x;
}

/****************************************/
/*	fwrite_crc()			*/
/****************************************/
void fwrite_crc(unsigned char *p, int n, FILE * fp)
{

    if (fp) {
	if (fwrite(p, 1, n, fp) < n) {
	    printf("\nFatal Error");
	    exit(1);
	}
    }
}

/****************************************/
/*	init_getbits()			*/
/****************************************/
void init_getbits(void)
{
    bitbuf = 0;
    subbitbuf = 0;
    bitcount = 0;
    fillbuf(2 * CHAR_BIT);
}


/********************************************/
/************* DECODE ***********************/

static void read_pt_len(short nn, short nbit, short i_special)
{
    short i, c, n;

    n = getbits(nbit);
    if (n == 0) {
	c = getbits(nbit);
	for (i = 0; i < nn; i++)
	    pt_len[i] = 0;
	for (i = 0; i < 256; i++)
	    pt_table[i] = c;
    } else {
	i = 0;
	while (i < n) {
	    c = bitbuf >> (16 - 3);
	    if (c == 7) {
		unsigned short mask = 1 << (16 - 4);
		while (mask & bitbuf) {
		    mask >>= 1;
		    c++;
		}
	    }
	    fillbuf((c < 7) ? 3 : c - 3);
	    pt_len[i++] = c;
	    if (i == i_special) {
		c = getbits(2);
		while (--c >= 0)
		    pt_len[i++] = 0;
	    }
	}
	while (i < nn)
	    pt_len[i++] = 0;
	make_table(nn, pt_len, 8, pt_table);
    }
}

static void read_c_len(void)
{
    short i, c, n;

    n = getbits(CBIT);
    if (n == 0) {
	c = getbits(CBIT);
	for (i = 0; i < NC; i++)
	    c_len[i] = 0;
	for (i = 0; i < 4096; i++)
	    c_table[i] = c;
    } else {
	i = 0;
	while (i < n) {
	    c = pt_table[bitbuf >> (16 - 8)];
	    if (c >= NT) {
		unsigned short mask = 1 << (16 - 9);
		do {
		    if (bitbuf & mask)
			c = right[c];
		    else
			c = left[c];
		    mask >>= 1;
		} while (c >= NT);
	    }
	    fillbuf(pt_len[c]);
	    if (c <= 2) {
		if (c == 0)
		    c = 1;
		else if (c == 1)
		    c = getbits(4) + 3;
		else
		    c = getbits(CBIT) + 20;
		while (--c >= 0)
		    c_len[i++] = 0;
	    } else
		c_len[i++] = c - 2;
	}
	while (i < NC)
	    c_len[i++] = 0;
	make_table(NC, c_len, 12, c_table);
    }
}

unsigned short decode_c_st1(void)
{
    unsigned short j, mask;

    if (blocksize == 0) {
	blocksize = getbits(16);
	read_pt_len(NT, TBIT, 3);
	read_c_len();
	read_pt_len(NP, PBIT, -1);
    }
    blocksize--;
    j = c_table[bitbuf >> 4];
    if (j < NC)
	fillbuf(c_len[j]);
    else {
	fillbuf(12);
	mask = 1 << (16 - 1);
	do {
	    if (bitbuf & mask)
		j = right[j];
	    else
		j = left[j];
	    mask >>= 1;
	} while (j >= NC);
	fillbuf(c_len[j] - 12);
    }
    return j;
}

unsigned short decode_p_st1(void)
{
    unsigned short j, mask;

    j = pt_table[bitbuf >> (16 - 8)];
    if (j < NP)
	fillbuf(pt_len[j]);
    else {
	fillbuf(8);
	mask = 1 << (16 - 1);
	do {
	    if (bitbuf & mask)
		j = right[j];
	    else
		j = left[j];
	    mask >>= 1;
	} while (j >= NP);
	fillbuf(pt_len[j] - 8);
    }
    if (j != 0)
	j = (1 << (j - 1)) + getbits(j - 1);
    return j;
}

void decode_start_st1(void)
{
    init_getbits();
    blocksize = 0;
}

/********end of decode***********************/

void decode(interfacing interface)
{
    int i, j, k, c, dicsiz1, offset;

    infile = interface.infile;
    outfile = interface.outfile;
    dicbit = interface.dicbit;
    origsize = interface.original;
    compsize = interface.packed;
    crc = 0;
    prev_char = -1;
    dicsiz = 1 << dicbit;
    text = (unsigned char *) malloc(dicsiz);
    if (text == NULL)
	exit(1);

    memset(text, ' ', dicsiz);
    decode_start_st1();

    dicsiz1 = dicsiz - 1;
    offset = 0x100 - 3;
    count = 0;
    loc = 0;
    while (count < origsize) {

	c = decode_c_st1();

	if (c <= UCHAR_MAX) {
	    text[loc++] = c;
	    if (loc == dicsiz) {
		fwrite_crc(text, dicsiz, outfile);
		loc = 0;
	    }
	    count++;
	} else {
	    j = c - offset;
	    i = (loc - decode_p_st1() - 1) & dicsiz1;
	    count += j;
	    for (k = 0; k < j; k++) {
		c = text[(i + k) & dicsiz1];
		text[loc++] = c;
		if (loc == dicsiz) {
		    fwrite_crc(text, dicsiz, outfile);
		    loc = 0;
		}
	    }
	}
    }
    if (loc != 0) {
	fwrite_crc(text, loc, outfile);
    }
    free(text);
}
