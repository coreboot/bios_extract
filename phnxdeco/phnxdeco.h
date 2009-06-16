/**********************************************
**
**
**	Defines & Functions for PhnxDeco
**
**
***********************************************/

/* ------ 26 Jan 2003 -------
**	New Name Conventions
* ------------------------- */

//typedef unsigned long dword;
typedef unsigned short word;

#define	BLOCK		0x8000
#define	Xtract		0x10
#define	List	   	0x11
#define	XtractM		0x20
#define	ListM  		0x21


#define IDSign		"+"

typedef struct {
    FILE *infile;
    FILE *outfile;
    unsigned long original;
    unsigned long packed;
    int dicbit;
    int method;
} interfacing;

typedef struct {
    unsigned int Prev;
    unsigned char Sig[3];
    unsigned char ID_HI;
    unsigned char ID_LO;
    unsigned char HeadLen;
    unsigned char isPacked;
    word Offset;
    word Segment;
    unsigned int ExpLen1;
    unsigned int Packed1;
    unsigned int Packed2;
    unsigned int ExpLen2;
} PHOENIXHEAD;

typedef struct {
    unsigned char Name[6];
    word Flags;
    word Len;
} PHNXID;

unsigned int FoundAt(FILE * ptx, unsigned char * Buf, char *Pattern, unsigned int BLOCK_LEN);

unsigned char TotalSec(FILE * ptx, unsigned char * Buf, unsigned char Action, unsigned int BankSize);

/* Modified Module Detection & Manipulating According to BCPSYS block */
unsigned char TotalSecM(FILE * ptx, unsigned char * Buf, unsigned char Action, unsigned int Start, unsigned int ConstOff, unsigned int SYSOff);

unsigned int IsPhoenixBIOS(FILE * ptx, unsigned char * Buf);

/* kernel.c */
void decode(interfacing interface);
