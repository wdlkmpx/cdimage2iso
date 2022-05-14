/*  Copyright (C) 2006,2007 Salvatore Santagati <salvatore.santagati@gmail.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the
    Free Software Foundation, Inc.,
    59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
    */

/*
 * Modified by Dmitry E. Oboukhov <dimka@avanto.org>
 *  [+] Use 'getopt' function;
 *  [+] Use STDOUT as output file (if not defined);
 *  [*] Fix percent output.
 */

/*
 iat  (Iso9660 Analyzer Tool) is a tool for detecting the structure
 of many types of CD-ROM image file formats, such as BIN, MDF, PDI,
 CDI, NRG, and B5I, and  converting them into ISO-9660.

 usage: iat input_image_file [output_iso_file]
*/

#include "main.h"

#include <unistd.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define VERSION "0.1.3"
#define BLOCK_ISO_CD 2048

#define OPTIONS_LIST   "h"

static char *input_file=0, *output_file=0;

/* Signature for Image ISO-9660 */
static const char ISO_9660_START[] = { 
    0x01, 0x43, 0x44, 0x30, 0x30, 0x31, 0x01, 0x00
};

static const char ISO_9660[] = { 
    0x02, 0x43, 0x44, 0x30, 0x30, 0x31, 0x01, 0x00
};

static const char ISO_9660_END[] = { 
    0xFF, 0x43, 0x44, 0x30, 0x30, 0x31, 0x01, 0x00
};

/* Signature for RAW image */
static const char SYNC_RAW[12] = { 
    0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00
};

static const char SYNC_RAW_2[8] = {
    0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x08, 0x00,
};

static long img_size;
static int  img_detect = 2;

static int  img_header = 0;
static int  img_ecc = 0;
static int  img_size_sector = 0;
static int  img_offset;

FILE *fsource, *fdest;

static int previous_percent=-1;

static void main_percent (int percent_bar)
{
    // Prints a progress bar, takes a percentage as argument.
    if (percent_bar==previous_percent) return;  // Nothing changed, don't waste CPU cycles.

    if (isatty(fileno(stderr)))
    {
        fprintf(stderr, 
                "\r%3d%% [:%.*s>%.*s:]",
                percent_bar,
                percent_bar/5,
                "====================",
                20-(percent_bar/5),
                "                    ");
    }
    else
    {
        if (previous_percent==-1) fprintf(stderr, "Working ");
        if ((percent_bar/5)*5==percent_bar) fprintf(stderr, ".");
    }
    previous_percent=percent_bar;
}


static void usage ()
{
    fprintf (stderr, "Web     : http://developer.berlios.de/projects/iat\n");
    fprintf (stderr, "Email   : salvatore.santagati@gmail.com\n\n");
    fprintf (stderr, "Usage   : ");
    fprintf (stderr, "iat  input_file [output_file.iso]\n\n");
    fprintf (stderr, "\tIf output file name is not defined, \n"
                     "\tthen stdout will be used instead.\n");
    fprintf (stderr, "\nOptions :\n");
    fprintf (stderr, "\t-h    		Display this notice\n\n");
}


static int image_convert()
{
    long source_length, i;
    char buf[2448];

    fseek (fsource, 0L, SEEK_END);
    source_length = (ftell (fsource) - img_offset) / img_size_sector;

    fseek (fsource, img_offset, SEEK_SET);
    {
        for (i = 0; i < source_length; i++)

        {
            main_percent(i*100/source_length);
            fseek (fsource, img_header, SEEK_CUR);
            if (fread (buf, sizeof (char),  BLOCK_ISO_CD, fsource));
            else
            {
                fprintf (stderr, "%s\n", strerror (errno));
                exit (EXIT_FAILURE);
            };
            if (fwrite (buf, sizeof (char),  BLOCK_ISO_CD, fdest));
            else
            {
                fprintf (stderr, "%s\n", strerror (errno));
                exit (EXIT_FAILURE);
            };
            fseek (fsource, img_ecc, SEEK_CUR);
        }
    }
    if (isatty(fileno(stderr)))
        fprintf (stderr, "\rDone                           \n");
    else
        fprintf (stderr, " Done\n");
    return 0;
}


static int image_detection() 
{
    char buf[8];
    char raw[12];
    int i;
    int block_image_start = 0;
    int block_image_end = 0;
    int block_image_temp = 0;
    int block_image = 0;
    int block_image_detect = 0;
    int img_header_temp = 0;
    int raw_2_check = 0;
    int raw_check = 0;

    fseek(fsource, 0L, SEEK_END);
    img_size = (((ftell(fsource))) / 8);
    for (i = 0; img_detect == 2; i = i + 1)
    {
        fseek(fsource, 0L, SEEK_SET);
        fseek(fsource, i, SEEK_CUR);
        fread(buf, sizeof(char), 8, fsource);
        fread(raw, sizeof(char), 12, fsource);

        if (!memcmp(ISO_9660_START, buf, 8))
        {
            fprintf(stderr, "Detect Signature ISO9660 START at %d\n", i);
            if (block_image_start == 0) block_image_start = i ;
        }
        if (!memcmp(ISO_9660, buf, 8))
        {
            fprintf(stderr, "Detect Signature ISO9660 at %d\n", i);
            if (block_image_end == 0)
            {
                block_image_end = i;
                block_image_temp = block_image_end - block_image_start;
            }
            img_detect++;
        }

        if (!memcmp(ISO_9660_END, buf, 8))
        {
            fprintf(stderr, "Detect Signature ISO9660 END at %d\n", i);
            if (block_image_end == 0)
            {
                block_image_end = i;
                block_image_temp = block_image_end - block_image_start;
            }
            img_detect++;
        }

        if (!memcmp(SYNC_RAW_2, buf, 8))
        {
            fprintf(stderr, "Detect Signature RAW 2 at %d\n", i);
            if (raw_2_check == 0)
            {
                img_header = img_header + 8;
                raw_2_check = 1;
            }
        }
        if (!memcmp(SYNC_RAW, raw, 12))
        {
            fprintf(stderr, "Detect Signature RAW at %d\n", i);
            if (raw_check == 0)
            {
                img_header = img_header + 16;
                raw_check = 1;
            }
        }

        if ((img_size * 8) <= i)
        {
            img_detect = -1;
            fprintf(stderr, "Image is broken\n");
            return 0;
        }
    }

    /* Detect Block structure of image */

    for (block_image_detect = 1; block_image_detect == 1; block_image_temp =(block_image_temp / 2))
    {
        if (block_image_temp >= 2048)
        {
            switch (block_image_temp)
            {
                case 2048:
                    block_image = block_image_temp;
                    block_image_detect++;
                    break;
                case 2352:
                    block_image = block_image_temp;
                    block_image_detect++;
                    break;
                case 2336:
                    block_image = block_image_temp;
                    block_image_detect++;
                    break;
                case 2448:
                    block_image = block_image_temp;
                    block_image_detect++;
                    break;
                default :
                    break;
            }
        }
        else block_image_detect = -1;
    }

    if (block_image_detect == -1);
    else
        img_size_sector = block_image;

    /* Size header of image */

    img_header_temp = block_image_start - block_image * 16;

    if ((img_header_temp == 8) || (img_header_temp == 16) || (img_header_temp == 24))
    {
        img_header = img_header_temp;
    }

    /* Size ECC of image */
    img_ecc = block_image - img_header - BLOCK_ISO_CD;

    /* Dump of image */
    img_offset = block_image_start - block_image * 16 - img_header;

    fprintf(stderr, "\n Image offset start at %d", img_offset);
    fprintf(stderr, "\n Sector header %d bit", img_header);
    fprintf(stderr, "\n Sector ECC %d bit", img_ecc);
    fprintf(stderr, "\n Block %d\n", block_image);

    return 1;
}


static void parse_options(int argc, char ** argv)
{
    int c;
  
    for (c=getopt(argc, argv, OPTIONS_LIST);
         c!=-1;
         c=getopt(argc, argv, OPTIONS_LIST))
    {
        switch(c)
        {
            case 'h':
                usage();
                exit(0);
            case '?':
                break;
            default:
                fprintf (stderr, "?? getopt returned character code 0%o ??\n", c);
        }
    }
    if (argc-optind<1 || argc-optind>2)
    {
        usage();
        exit(EXIT_FAILURE);
    }

    input_file=argv[optind];
    if (argc-optind==2) output_file=argv[optind+1];
}


int main(int argc, char **argv)
{
    fprintf(stderr, "Iso9660 Analyzer Tool v%s by Salvatore Santagati\n", VERSION);
    fprintf(stderr, "Licensed under GPL v2 or later\n\n");

    parse_options(argc, argv);

    if ((fsource = fopen(input_file, "rb")) != NULL)
    {
        if (image_detection() == 0)
        {
            fprintf(stderr, "This image is not CD IMAGE\n");
            exit(EXIT_FAILURE);
        }
        else
        {
            if (output_file)
            {
                if (!(fdest = fopen(output_file, "wb")))
                {
                    fprintf(stderr, "%s\n", strerror(errno));
                    usage();
                    exit(EXIT_FAILURE);
                }
            }
            else
            {
                fdest=stdout;
            }
            image_convert();
            fclose(fdest);
        }
    }
    else
    {
        fprintf(stderr, "%s\n", strerror(errno));
        usage();
        exit(EXIT_FAILURE);
    }

    return 0;
}

