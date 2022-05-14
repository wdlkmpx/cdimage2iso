/*
    ccd2iso v0.3

    Copyright (C) 2003 by Danny Kurniawan
    danny_kurniawan@users.sourceforge.net

    Contributors:
    - Kerry Harris <tomatoe-source@users.sourceforge.net>

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
 
/*
 * README
 * 
HOW TO USE
----------
Easy... normally you would have 3 files from CloneCD image - .ccd/.img/.sub, just type:

    ccd2iso myimage.img myimage.iso

TECHNICAL EXPLANATION
---------------------
- After some strange hacking through CloneCD's image file,
   I found that CloneCD is only dumping CD's raw data, thus together with 12 byte synchronization, 4 byte header, ECC, and EDC...

- For now, only single session image supported, if multiple session image found
  then the program will dump the first session only with an "Unrecognized sector mode" error...
  but the resulting file, will be mounted quite fine.

TO DO
-----
- Better support for multisession image. (Hopefully this task is possible)
*/

#include "main.h"

#include <stdio.h>
#include <stdlib.h>

//===================================================

#define DATA_SIZE 2048

typedef unsigned char ccd_sectheader_syn[12];
typedef unsigned char ccd_edc[4];
typedef unsigned char ccd_ecc[276];

typedef struct __attribute__((packed))
{
    unsigned char sectaddr_min;
    unsigned char sectaddr_sec;
    unsigned char sectaddr_frac;
    unsigned char mode;
} ccd_sectheader_header;

typedef unsigned char ccd_sectheader_subheader[8];  //??? No idea about the struct

typedef struct __attribute__((packed))
{
    ccd_sectheader_syn syn;
    ccd_sectheader_header header;
} ccd_sectheader;

typedef struct __attribute__((packed))
{
    ccd_sectheader sectheader;
    union {
        struct {
            unsigned char data[DATA_SIZE];
            ccd_edc edc;
            unsigned char unused[8];
            ccd_ecc ecc;
        } mode1;
        struct {
            ccd_sectheader_subheader sectsubheader;
            unsigned char data[DATA_SIZE];
            ccd_edc edc;
            ccd_ecc ecc;
        } mode2;
    } content;
} ccd_sector;


const ccd_sectheader_syn CCD_SYN_DATA =
{
    0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00
};

//===================================================

//int main (int argc, char *argv[])
int ccd2iso (struct file_info *infile, struct file_info *outfile)
{
    ccd_sector src_sect;
    int bytes_read, bytes_written, sect_num = 0;

    FILE *src_file = infile->stream;
    FILE *dst_file = outfile->stream;

    while(!feof(src_file))
    {
        bytes_read = fread(&src_sect, 1, sizeof(ccd_sector), src_file);

        if (bytes_read != 0)
        {
            if (bytes_read < sizeof(ccd_sector))
            {
                printf("Error at sector %d.\n", sect_num);
                printf("The sector does not contain complete data. Sector size must be %d, while actual data read is %d\n",
                sizeof(ccd_sector), bytes_read);
                return 1;
            }

            switch (src_sect.sectheader.header.mode)
            {
                case 1:  // Mode 1 Data Sector
                    bytes_written = fwrite(&(src_sect.content.mode1.data), 1, DATA_SIZE, dst_file);
                    break;
                case 2:  // Mode 2 Data Sector
                    bytes_written = fwrite(&(src_sect.content.mode2.data), 1, DATA_SIZE, dst_file);
                    break;
                case 0xe2:
                    printf("\nFound session marker, the image might contain multisession data.\n only the first session dumped.\n");
                    return -1;
                default:
                    printf("\nUnrecognized sector mode (%x) at sector %d!\n", src_sect.sectheader.header.mode, sect_num);
                    return 1;
            }

            if (bytes_written < DATA_SIZE)
            {
                printf("Error writing to file.\n");
                return 1;
            }

            sect_num++;
            printf("%d sector written\r", sect_num);
        }
    }

    printf("\nDone.\n");

    return 0;
}
