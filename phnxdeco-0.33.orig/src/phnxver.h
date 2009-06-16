/**********************************************
**
**
**	Defines for PhnxVer
**
**
***********************************************/

#include	<stdio.h>

#ifdef __LINUX_NOW__
    #define SftPlatform		"Linux"
#else
    #define SftPlatform		"DOS32"
#endif

#define SftVersion      "0.33 ("SftPlatform")"
