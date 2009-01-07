/*
 *	AMIBIOS	Decompress
 *
 *      Copyright (C) 2000, 2002 Anthony Borisow
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/********************************************
**
**	Last update for 0.31 - Sep 29, 2000
**
********************************************/

/****************************************/
/*	0.31 Added Buffered Search	*/
/****************************************/
/*	WARNING!	Under DJGPP	*/
/****************************************/

/*		Version 0.31		*/


#include	<stdio.h>
#include	<stdlib.h>


#include	"./amideco.h"
#include	"./amisoft.h"
#include	"./amiver.h"
#include	"./amihelp.h"

int main(byte argc, byte *argv[])
	{

		FILE *ptx, *pto;
		dword fLen, i, RealRead = 0;
		byte Temp[] = "AMIBIOSC", *BufBlk;
		byte Buf[12];
		byte BufAdd[0x30];
		ABCTag abc;

		dword ConstOff, Offset, BODYOff = 0;
		LZHHead FHead;

		byte AMIVer = 0;
		AMIDATE amidate;

		byte	PartTotal = 0;
		byte	Action = 0;
		byte	HelpID = 0;

		HelpID = HelpSystem(argc,argv);
		switch( HelpID & 0x7F ){
			case 0x20:	return 0;
			case 0x10:	Action = Xtract; break;
			case 0x01:	Action = List; break;
			default:
				PrintUsage();
				printf("\n");
				return 0;
			}

		if( (ptx = fopen(argv[1],"rb")) == NULL )
			{
			printf("\nFile %s opening error...\n", argv[1]);
			return 0;
			};

		PrintHeader("\n\n");

		fseek( ptx, 0, 2 );
		fLen = ftell(ptx);
		rewind( ptx );
		printf("FileLength\t: %lX (%lu bytes)\n", fLen, fLen);
		printf("FileName\t: %s\n", argv[1]);

		/*------- Memory Alloc --------*/


		if( (BufBlk = (byte*)calloc(BLOCK, sizeof(byte))) == NULL ) exit(1);
		i = 0;

		while(!feof(ptx))
		{
			fseek( ptx, i, 0 );
			RealRead = fread(BufBlk, 1, BLOCK, ptx);
			if(( i = FoundAt(ptx, BufBlk, Temp, RealRead) ) != 0)
			{
			fseek(ptx, i + 8, 0);
			fread(&abc, 1, sizeof(abc), ptx);
			AMIVer = 95;
			break;
			}
			i = ftell(ptx) - 0x100;

		}
		if(AMIVer != 95)
			{
			printf("AMI'95 hook not found..Turning to AMI'94\n");
			rewind(ptx);
			fread(&Buf, 1, 8, ptx);
			if(memcmp(Buf, Temp, 8) != 0)
				{
				printf("Obviously not even AMIBIOS standard..Exit\n");

				return 0;
				} else { AMIVer = 94; };
			};



		printf("\n\tAMIBIOS information:");

		switch(AMIVer)
			{
			case 95:
		fseek(ptx, -11L, 2);
		fread(&amidate, 1, sizeof(amidate), ptx);

		printf("\nVersion\t\t: %.4s",abc.Version);
		printf("\nPacked Data\t: %lX (%lu bytes)",(dword)abc.CRCLen*8,(dword)abc.CRCLen*8);
		printf("\nStart\t\t: %lX",Offset=((dword)abc.BeginHi<<4)+(dword)abc.BeginLo);

		BODYOff = fLen - ( 0x100000 - ( Offset + 8 + sizeof(abc)) ) - 8 - sizeof(abc);
		printf("\nPacked Offset\t: %lX", BODYOff);

		ConstOff = Offset - BODYOff;
		printf("\nOffset\t\t: %lX", ConstOff);

			break;
			case 94:
		fread(&amidate, 1, sizeof(amidate), ptx);
		if(atoi(amidate.Day) == 10 && atoi(amidate.Month) == 10) { Offset = 0x30; AMIVer = 10; }
			else Offset = 0x10;
		fseek(ptx, -11L, 2);
		fread(&amidate, 1, sizeof(amidate), ptx);
			break;
			};
		printf("\nReleased\t: %s", GetFullDate(amidate.Month, amidate.Day, amidate.Year));

	switch(AMIVer)
	{
	case 95:
		PartTotal = Xtract95(ptx, HelpID, ConstOff, Offset, argv[1]);
			break;
	case 94:
		PartTotal = Xtract0725(ptx, Action, Offset);
			break;
	case 10:
		PartTotal = Xtract1010(ptx, Action, Offset);
	default: break;
	}
		printf("\nTotal Sections\t: %i", PartTotal);

		free(BufBlk);

		printf("\n");
		return 0;
	}
