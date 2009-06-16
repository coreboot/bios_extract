PhnxDeco (PhoenixDeco) 
(C) 2000, 2002, 2003

Anthony Borisow
email: anton.borisov@gmail.com

http://kaos.ru/biosgfx/
http://biosgfx.euro.ru/phoenixdeco/
http://biosgfx.narod.ru/phoenixdeco/

PhoenixDeco - Ultimate Phoenix Decompressor

/*--------------------------------*/
/*------------ INFO --------------*/
/*--------------------------------*/


This software is intended to decompress (expand) the content of the PhoenixBIOS.
Currently supported cores are: PhoenixBIOS 4.0 Version 6.0 (any subversion),
Phoenix FirstBIOS (any version). Compression schemes to decompress are:
LZINT, LZSS, NONE.

The best way to decompress the BIOS is to use the next example:

1.	phnxdeco PhoenixBIOS.ROM -xs
	or
2.	phxndeco PhoenixBIOS.ROM -x


N.B.	While using example #1 also generates file named rom.scr - this is
	script file is used for combining the uncompressed parts into solid ROM.
	
/*--------------------------------*/
/*--------- DISCLAIMER -----------*/
/*--------------------------------*/

THIS SOFTWARE AND THE ACCOMPANYING FILES ARE DISTRIBUTED "AS IS"
AND WITHOUT ANY WARRANTIES WHETHER EXPRESSED OR IMPLIED. NO
RESPONSIBILITIES FOR POSSIBLE DAMAGES OR EVEN FUNCTIONALITY CAN BE
TAKEN. THE USER MUST ASSUME THE ENTIRE RISK OF USING THIS PROGRAM.
ALL TRADEMARKS ARE PROPERTY OF THEIR RESPECTIVE OWNERS.
