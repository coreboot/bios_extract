/**********************************************
**
**
**	Defines for AmiVer
**
**
***********************************************/

#include <stdio.h>

#ifdef __LINUX_NOW__
    #define SftPlatform         "Linux"
#else
    #define SftPlatform         "DOS32"
#endif

#define SftVersion "0.31e ("SftPlatform")"
