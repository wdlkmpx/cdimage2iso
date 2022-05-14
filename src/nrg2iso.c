/* 
   01/05/2003 Nrg2Iso v 0.4

   Copyright (C) 2003 Grégory Kokanosky <gregory.kokanosky@free.fr>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   http://www.gnu.org/licenses/gpl-2.0.txt
*/


#include "main.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>


//int main (int argc, char *argv[])
int nrg2iso (struct file_info *infile, struct file_info *outfile)
{
    if (infile->format == FORMAT_ISO) {
        printf("It seems that %s is already an ISO 9660 image \n", infile->path);
        printf("[Aborting conversion]\n");
        return -1;
    }
    char footer[12];

    // validate nrg file
    // https://en.wikipedia.org/wiki/NRG_(file_format)#Header
    // https://p2k.unibabwi.ac.id/IT/en/2821-2718/NRG_12241_p2k-unibabwi.html
    fseek (infile->stream, -12, SEEK_END);
    fread (footer, sizeof(char), 12, infile->stream);
    if (memcmp(footer, "NER5", 4) == 0) {
        printf ("NRG - Nero footer (Version 2) detected\n");
    } else if (memcmp(footer+4, "NERO", 4) == 0) {
        printf ("NRG - Nero footer (Version 1) detected\n");
    } else {
        printf ("Can't recognize NRG file, looks invalid..\n");
        return -1;
    }

    // set pointer to the beginning of infile->stream
    fseek (infile->stream, 0, SEEK_SET);

    FILE *nrgFile, *isoFile;
    char buffer[1024 * 1024];
    int NUM_OF_COLUMNS = 70;
    int i = 0, l;
    off_t size = 0;
    off_t nrgSize = 0;
    int percent = 0;
    int old_percent = -1;
    struct stat buf;

    stat (infile->path, &buf);
    nrgSize = buf.st_size;

    nrgFile = infile->stream;
    fseek (nrgFile, 307200, SEEK_SET);

    isoFile = outfile->stream;

    i = fread(buffer, 1, sizeof(buffer), nrgFile);
    while (i > 0)
    {
        fwrite(buffer,i,1,isoFile);

        size+=i;
        percent = (int)(size * 100.0 / nrgSize);
        if (percent != old_percent) {
            old_percent = percent;
            printf("\r|");
            for(l = 0; l < percent * NUM_OF_COLUMNS / 100; l++) {
                printf("=");
            }
            printf(">[%d%%]",percent);
            fflush(stdout);
        }
        i = fread(buffer, 1, sizeof(buffer), nrgFile);
    }

    printf("\r|");
    for (l = 0; l < NUM_OF_COLUMNS; l++) {
        printf("=");
    }
    printf (">[100%%]");
    fflush (stdout);

    printf ("\n%s written : %lld bytes\n", outfile->path, size);
    return 0;
}

