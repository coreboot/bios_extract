#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "efihack.h"


EFI_STATUS
EFIAPI
EfiGetInfo (
            IN      VOID                    *Source,
            IN      UINT32                  SrcSize,
            OUT     UINT32                  *DstSize,
            OUT     UINT32                  *ScratchSize
            );

EFI_STATUS
EFIAPI
EfiDecompress (
               IN      VOID                    *Source,
               IN      UINT32                  SrcSize,
               IN OUT  VOID                    *Destination,
               IN      UINT32                  DstSize,
               IN OUT  VOID                    *Scratch,
               IN      UINT32                  ScratchSize
               );

int main(int argc, char **argv)
{
    char *buffer;
    long buflen, fill;
    ssize_t got;
    char *dstbuf, *scratchbuf;
    UINT32 DstSize;
    UINT32 ScratchSize;
    EFI_STATUS Status;

    // read all data from stdin
    buflen = 32768;
    fill = 0;
    buffer = malloc(buflen);
    if (buffer == NULL) {
        fprintf(stderr, "Out of memory!\n");
        return 1;
    }
    for (;;) {
        if (fill == buflen) {
            long newbuflen;

            newbuflen = buflen << 1;
            buffer = realloc(buffer, newbuflen);
            if (buffer == NULL) {
                fprintf(stderr, "Out of memory!\n");
                return 1;
            }
            buflen = newbuflen;
        }

        got = read(0, buffer + fill, buflen - fill);
        if (got < 0) {
            fprintf(stderr, "Error during read: %d\n", errno);
            return 1;
        } else if (got == 0) {
            break;  // EOF
        } else {
            fill += got;
        }
    }

    //fprintf(stderr, "got %d bytes\n", fill);

    // inspect data
    Status = EfiGetInfo(buffer, fill, &DstSize, &ScratchSize);
    if (Status != EFI_SUCCESS) {
        fprintf(stderr, "EFI ERROR (get info)\n");
        return 1;
    }
    dstbuf = malloc(DstSize);
    scratchbuf = malloc(ScratchSize);
    if (dstbuf == NULL || scratchbuf == NULL) {
        fprintf(stderr, "Out of memory!\n");
        return 1;
    }

    // decompress data
    Status = EfiDecompress(buffer, fill, dstbuf, DstSize, scratchbuf, ScratchSize);
    if (Status != EFI_SUCCESS) {
        fprintf(stderr, "EFI ERROR (decompress)\n");
        return 1;
    }

    // write to stdout
    while (DstSize > 0) {
        got = write(1, dstbuf, DstSize);
        if (got < 0) {
            fprintf(stderr, "Error during write: %d\n", errno);
            return 1;
        } else {
            dstbuf += got;
            DstSize -= got;
        }
    }

    return 0;
}
