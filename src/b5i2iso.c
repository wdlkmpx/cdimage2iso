/*  $Id: b5i2iso.c, v0.2 - 07/04/05 10.47

    Copyright (C) 2004,2005 Salvatore Santagati <salvatore.santagati@gmail.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    http://www.gnu.org/licenses/gpl-2.0.txt
*/

#include "main.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
BlindWrite CD Image related extensions:

    .b00 BlindWrite CD Image Segment
    .b5l BlindWrite 5 License Data
    .b6l BlindWrite License Data
    .bwa BlindWrite Physical Media Descriptor
    .bwi BlindWrite <= 4 CD Image
    .b5i BlindWrite 5    CD Image
    .b6i BlindWrite 6+   CD Image
    .bws BlindWrite Sub Channel Data
*/

static void cuesheets (char *destfilename)
{
    char destfilecue[1024];
    char destfilebin[1024];
    FILE *fcue;

    strcpy (destfilecue, destfilename);
    strcpy (destfilebin, destfilename);

    strcpy (destfilecue+strlen(destfilename)-4, ".cue");
    strcpy (destfilebin+strlen(destfilename)-4, ".bin");

    fcue = fopen (destfilecue,"w");
    fprintf (fcue,"FILE \"%s\" BINARY\n",destfilebin);
    fprintf (fcue,"TRACK 1 MODE1/2352\n");
    fprintf (fcue,"INDEX 1 00:00:00\n");

    if (!rename (destfilename,destfilebin)== 0)  {
        printf ("\nSorry, I don't create %s,\nbecause is in use", destfilebin);
        printf (" or this file exists\n\n");
    } else {
        printf ("Create Bin       : %s\n", destfilebin);
    }

    printf ("Create Cuesheets : %s\n", destfilecue);

    fclose (fcue);
}


static void main_percent (int percent_bar)
{
    int progress_bar,progress_space;
    printf ("%d%% [:",percent_bar);
    for (progress_bar=1; progress_bar<=(int)(percent_bar/5); progress_bar++) {
        printf ("=");
    }
    printf (">");
    for (progress_space=0; progress_space<(20-progress_bar); progress_space++) {
        printf (" ");
    }
    printf (":]\r");
}


//int main (int argc, char *argv[])
int b5i2iso (struct file_info *infile, struct file_info *outfile)
{
    int seek_ecc;
    int sector_size;
    int seek_head;
    int sector_data;
    int cue = 0;
    //int sub = 1;
    int bw  = 1;
    int dump;
    double size_iso,write_iso;
    long  percent=0;
    long  i, source_length;
    char  buf[2448];
    FILE  *fdest, *fsource;

    const char SYNC_HEADER_B5I_96[16] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x41, 0x01, 0x00, 0x00 };
    const char SYNC_HEADER_B5I[16]    = { 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x01, 0x01 };
    const char SYNC_HEADER_BWI[16]    = { 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x02, 0x01, 0x01 };

    const char BWI_IMG[16] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x90, 0x00, 0xC1, 0x00, 0x12 };
    const char B5I_IMG[16] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x90, 0x00, 0x48, 0x00, 0xD8 };

    fsource = infile->stream;
    fdest   = outfile->stream;

    if (infile->format == FORMAT_ISO) {
        printf ("This is file iso9660 ;)\n");
        return 0;
    }

    /* DETECT BLINDWRITE IMAGE */
    fseek(fsource, 2336, SEEK_SET);
    fread(buf, sizeof(char), 16, fsource);

    if (memcmp(B5I_IMG, buf,16) == 0) {
        dump = 150;
    } else if (memcmp(BWI_IMG, buf,16) == 0) {
        dump = 0;
    } else {
        printf ("Sorry This file is not BlindWrite B5I / BWI image\n");
        return -1;
    }

    fseek (fsource, 0L, SEEK_END);
    fseek (fsource, 2352, SEEK_SET);
    fread (buf, sizeof(char), 16, fsource);

    if (memcmp(SYNC_HEADER_B5I_96, buf, 16) == 0)
    {
        /* BAD IMAGE BLINDWRITE */
        printf ("BAD IMAGE BLINDWRITE\n");
        if (cue == 1) {
            /* RAW TO BIN*/
            seek_ecc = 96;
            sector_size = 2448;
            sector_data = 2352;
            seek_head = 0;
        } else {
            /* RAW */
            seek_ecc = 384;
            sector_size = 2448;
            sector_data = 2048;
            seek_head = 16 ;
        }
    }
    else if ((memcmp(SYNC_HEADER_BWI, buf, 16) == 0)
          || (memcmp(SYNC_HEADER_B5I, buf, 16) == 0))
    {
        /* NORMAL IMAGE BLINDWRITE */
        if (cue == 1) {
            //sub = 0;
            seek_ecc = 0;
            sector_size = 2352;
            sector_data = 2352;
            seek_head = 0;
        } else {
            seek_ecc = 288;
            sector_size = 2352;
            sector_data = 2048;
            seek_head = 16;
        }
    }
    else
    {
        printf ("Sorry cannot identify this BlindWrite CD image format ...\n");
        return -1;
    }

    // -- Proceed --
    fseek (fsource, 0L, SEEK_END);
    source_length = (ftell(fsource))/sector_size ;
    size_iso = (int)(source_length  * sector_data);
    fseek (fsource, 0, SEEK_SET);

    for (i = 0; i < source_length; i++)
    {
        fseek (fsource, seek_head, SEEK_CUR);
        fread (buf, sizeof(char), sector_data, fsource);
        if (bw >dump) {
            fwrite(buf, sizeof(char),sector_data, fdest);
        }
        fseek(fsource, seek_ecc, SEEK_CUR);
        write_iso = (int)( sector_data * i );
        if (i!=0) {
            percent =(int)(write_iso * 100 / size_iso);
        }
        if (bw >dump) {
            main_percent(percent);
        }
        bw++;
    }

    printf ("100%%[:====================:]\n");

    if (cue == 1) {
        cuesheets (outfile->path);
    }
    else  printf ("Create iso9660 : %s\n", outfile->path);

    return 0;
}
