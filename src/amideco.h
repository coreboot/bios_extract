/**********************************************
**
**
**      Defines & Functions for AmiDeco
**
**
***********************************************/

#ifndef AMIDECO_H
#define AMIDECO_H 1

#include        <stdio.h>
#include        <stdlib.h>

#if defined(LINUX) || defined(__LINUX__) || defined(__linux__)
    #include    <memory.h>
    #define     __LINUX_NOW__
#else
    #include    <mem.h>
    #include    <conio.h>
#endif

typedef unsigned char   byte;
typedef unsigned short  word;
typedef unsigned long   dword;

#define Xtract  0x10
#define List    0x01
#define Delete  0x12
#define XtractD 0x13

#define BLOCK   0x8000

#ifndef __LINUX_NOW__
    #define IDSign              ""
#else
    #define     IDSign          "+"
#endif

typedef struct {
        FILE *infile;
        FILE *outfile;
        unsigned long original;
        unsigned long packed;
        int dicbit;
        int method;
        } interfacing;

typedef struct
        {
                byte    Version[4];
                word    CRCLen;
                dword   CRC32;
                word    BeginLo;
                word    BeginHi;
        } ABCTag;

typedef struct
        {
                word    PrePartLo;      /*      Previous part LO word   */
                word    PrePartHi;      /*      Previous part HI word   */
                                        /*      value 0xFFFFFFFF = last */
                word    CSize;          /*      This module header Len  */
                byte    PartID;         /*      ID for this header      */
                byte    IsComprs;       /*      Value 0x80 means that
                                                this module not
                                                compressed, 0x00 - is   */
                dword   RealCS;         /*      Address in RAM where
                                                expand to               */
                dword   ROMSize;        /*      Compressed Len          */
                dword   ExpSize;        /*      Expanded Len            */
        } PARTTag;

typedef struct
        {
                byte    HeadLen;
                byte    HeadCrc;
                byte    Method[5];
                dword   PackLen;
                dword   RealLen;
                dword   TStamp;
                byte    Attr;
                byte    Level;
                byte    FilenameLen;
                byte    FileName[12];
                word    CRC16;
                byte    DOS;
                word    Empty;
        } LZHHead;

typedef struct
        {
                word    PackLenLo;
                word    PackLenHi;
                word    RealLenLo;
                word    RealLenHi;
        } BIOS94;

typedef struct
        {
                byte    Month[2];
                byte    rsrv1;
                byte    Day[2];
                byte    rsrv2;
                byte    Year[2];
        } AMIDATE;

typedef struct
        {
        dword Offset;
        byte  ModID;
        byte  IsPacked;
        } AMIHEAD94;

byte* GetModuleName(byte ID);

//dword   CalcCRC32(FILE *ptx, dword Begin, word Len);

byte StrLen(byte *Str);
byte StrCmp(byte *Dst, byte *Src);

byte *GetFullDate(byte *mon, byte *day, byte *year);

/*---------------------------------
        FindHook
----------------------------------*/
dword FoundAt(FILE *ptx, byte *Buf, byte *Pattern, dword BLOCK_LEN);

/*---------------------------------
        Xtract95
----------------------------------*/
byte Xtract95(FILE *ptx, byte Action, dword ConstOff, dword Offset, byte* fname);

/*---------------------------------
        Xtract0725
----------------------------------*/
byte Xtract0725(FILE *ptx, byte Action, dword Offset);

/*---------------------------------
        Xtract1010
----------------------------------*/
byte Xtract1010(FILE *ptx, byte Action, dword Offset);

#endif
