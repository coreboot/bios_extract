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

#include	<stdio.h>

typedef unsigned char byte;
typedef unsigned long dword;
typedef unsigned short word;

#define	BLOCK		0x8000
#define	Xtract		0x10
#define	List	   	0x11
#define	XtractM		0x20
#define	ListM  		0x21


#ifndef	__LINUX_NOW__
#define IDSign		""
#else
#define IDSign		"+"
#endif


//#define       __DEBUG__

typedef struct {
    FILE *infile;
    FILE *outfile;
    unsigned long original;
    unsigned long packed;
    int dicbit;
    int method;
} interfacing;

typedef struct {
    byte Month[2];
    byte rsrv1;
    byte Day[2];
    byte rsrv2;
    byte Year[2];
} AMIDATE;

typedef struct {
    dword Prev;
    byte Sig[3];
    byte ID_HI;
    byte ID_LO;
    byte HeadLen;
    byte isPacked;
    word Offset;
    word Segment;
    dword ExpLen1;
    dword Packed1;
    dword Packed2;
    dword ExpLen2;
} PHOENIXHEAD;

typedef struct {
    byte Name[6];
    word Flags;
    word Len;
} PHNXID;

byte StrLen(byte * Str);
byte StrCmp(byte * Dst, byte * Src);

dword FoundAt(FILE * ptx, byte * Buf, byte * Pattern, dword BLOCK_LEN);
byte *GetFullDate(byte * mon, byte * day, byte * year);

byte *GetModuleName(byte ID);
byte *GetCompressionName(byte ID);
void decodeM3(interfacing interface);

byte TotalSec(FILE * ptx, byte * Buf, byte Action, dword BankSize);

/*---------------Modified Module Detection & Manipulating------------
		According to BCPSYS block
--------------------------------------------------------------------*/
byte TotalSecM(FILE * ptx, byte * Buf, byte Action, dword Start, dword ConstOff, dword SYSOff);

dword IsPhoenixBIOS(FILE * ptx, byte * Buf);
