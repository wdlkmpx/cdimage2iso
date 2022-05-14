/*  $Id: mdf2iso.c, v0.3.1 - 22/05/05 

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
#include <errno.h>


static const char SYNC_HEADER[12] = {
    0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00
};

static const char SYNC_HEADER_MDF_AUDIO[12] = {
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0xC0, 0x80, 0x80, 0x80, 0x80
};

static const char SYNC_HEADER_MDF[12] = {
    0x80, 0xC0, 0x80, 0x80, 0x80, 0x80, 0x80, 0xC0, 0x80, 0x80, 0x80, 0x80
};


#define UNKNOWN   -1
#define ISO9660    0
#define SYNC       1
#define SYNC_MDF   2
#define MDF_AUDIO  3

static int toc_file (char *destfilename, int sub)
{
    int ret=0;
    char *destfiletoc;
    char *destfiledat;
    FILE *ftoc;
  
    destfiletoc=strdup(destfilename);
    destfiledat=strdup(destfilename);
    strcpy (destfiletoc + strlen (destfilename) - 4, ".toc");
    strcpy (destfiledat + strlen (destfilename) - 4, ".dat");

    if ((ftoc = fopen (destfiletoc, "w")) != NULL)
    {
        fprintf (ftoc, "CD_ROM\n");
        fprintf (ftoc, "// Track 1\n");
        fprintf (ftoc, "TRACK MODE1_RAW");

        if (sub == 1) fprintf (ftoc, " RW_RAW\n");
        else fprintf (ftoc, "\n");

        fprintf (ftoc, "NO COPY\n");
        fprintf (ftoc, "DATAFILE \"%s\"\n", destfiledat);
        rename (destfilename, destfiledat);
        printf ("Create TOC File : %s\n", destfiletoc);
        fclose (ftoc);
    }
    else
    {
        printf ("Error opening %s for output: %s\n",destfiletoc,strerror(errno));
        ret=-1;
    };
    free(destfiletoc);
    free(destfiledat);
    return ret;
}

/*
static int number_file (char *destfilename)
{
    int i = 1, test_mdf = 0;
    int n_mdf;
    char mdf[2], destfilemdf[2354];
    FILE *fsource;

    strcpy (destfilemdf, destfilename);
    strcpy (destfilemdf + strlen (destfilename) - 1, ".0");
    for (i = 0; test_mdf == 0; i++)
    {
        if ((fsource = fopen (destfilemdf, "rb")) != NULL)
        {
            printf ("\nCheck : ");
            sprintf (mdf, "md%d", i);
            strcpy (destfilemdf + strlen (destfilename) - 3, mdf);
            printf ("%s, ", destfilemdf);
            fclose (fsource);
        }
        else
        {
            test_mdf = 1;
        }
    }
    printf ("\r                                   \n");
    n_mdf = i - 1;
    return (n_mdf);
}
*/

static int cuesheets (char *destfilename)
{
    int ret=0;
    char *destfilecue;
    char *destfilebin;
    FILE *fcue;

    destfilecue = strdup(destfilename);
    destfilebin = strdup(destfilename);
    strcpy (destfilecue + strlen (destfilename) - 4, ".cue");
    strcpy (destfilebin + strlen (destfilename) - 4, ".bin");
    if ((fcue = fopen (destfilecue, "w"))!=NULL)
    {
        fprintf (fcue, "FILE \"%s\" BINARY\n", destfilebin);
        fprintf (fcue, "TRACK 1 MODE1/2352\n");
        fprintf (fcue, "INDEX 1 00:00:00\n");
        rename (destfilename, destfilebin);
        printf ("Create Cuesheets : %s\n", destfilecue);
        fclose (fcue);
    }
    else
    {
        printf ("Error opening %s for output: %s\n",destfilecue,strerror(errno));
        ret=-1;
    }
    return ret;
}


static int previous_percent=-1;

static void main_percent (int percent_bar)
{
    // Prints a progress bar, takes a percentage as argument.
    if (percent_bar == previous_percent) {
        return;  // Nothing changed, don't waste CPU cycles.
    }
    printf ("%3d%% [:%.*s>%.*s:]\r",percent_bar,20-(percent_bar/5),"                    ",
                                   percent_bar/5,"====================");
}


static int mdftype (FILE *f)
{
    char buf[12];
  
    fseek (f, 0L, SEEK_SET);
    fread (buf, sizeof (char), 12, f);

    fseek (f, 2352, SEEK_SET);
  
    if (!memcmp (SYNC_HEADER, buf, 12))  // Has SYNC_HEADER
    {
        fread (buf, sizeof (char), 12, f);
        if (!memcmp (SYNC_HEADER_MDF, buf, 12)) {
            return SYNC_MDF; // File is SYNC MDF
        }
        if (!memcmp (SYNC_HEADER, buf, 12)) {
            return SYNC; // File is SYNC
        }
    }
    else  // Does not have SYNC_HEADER
    {
        fread (buf, sizeof (char), 12, f);
        if (!memcmp (SYNC_HEADER_MDF_AUDIO, buf, 12)) {
            return MDF_AUDIO; // File is MDF Audio
        }
    }

    // Reached a point where nothing else matters.  
    return UNKNOWN; // Unknown format
}


//int main (int argc, char *argv[])
int mdf2iso (struct file_info *infile, struct file_info *outfile)
{
    int seek_ecc, sector_size, seek_head, sector_data;
    //int n_mdf;
    int cue = 0, toc = 0, sub_toc = 0;
    long i, source_length;
    char buf[2448];
    FILE *fdest, *fsource;
 
    fsource = infile->stream;
    fdest   = outfile->stream;

    // *** Preprocess basefile ***

    if (infile->format == FORMAT_ISO) {
        printf ("%s is already ISO9660.\n", outfile->path);
        return EXIT_SUCCESS;
    }

    // Determine filetype & set some stuff accordingly (or exit)
    switch (mdftype(fsource))
    {
        case SYNC:
            if (cue == 1) {
                seek_ecc = 0;
                sector_size = 2352;
                sector_data = 2352;
                seek_head = 0;
            } else if (toc == 0) {
                /*NORMAL IMAGE */
                seek_ecc = 288;
                sector_size = 2352;
                sector_data = 2048;
                seek_head = 16;
            } else {
                seek_ecc = 0;
                sector_size = 2352;
                sector_data = 2352;
                seek_head = 0;
            }
            break;

        case SYNC_MDF:
            if (cue == 1) {
                /* BAD SECTOR TO NORMAL IMAGE */
                seek_ecc = 96;
                sector_size = 2448;
                sector_data = 2352;
                seek_head = 0;
            } else if (toc == 0) {
                /*BAD SECTOR */
                seek_ecc = 384;
                sector_size = 2448;
                sector_data = 2048;
                seek_head = 16;
            } else {
                /*BAD SECTOR */
                seek_ecc = 0;
                sector_size = 2448;
                sector_data = 2448;
                seek_head = 0;
                sub_toc = 1;
            }
            break;

        case MDF_AUDIO:
            /*BAD SECTOR AUDIO */
            seek_head = 0;
            sector_size = 2448;
            seek_ecc = 96;
            sector_data = 2352;
            cue = 0;
            break;

        default:
            printf ("Unknown format for %s.\n",outfile->path);
            return EXIT_FAILURE;
    }

    //  *** Create destination file ***

    fseek (fsource, 0L, SEEK_END);
    source_length = ftell (fsource) / sector_size;
    fseek (fsource, 0L, SEEK_SET);

    {
        for (i = 0; i < source_length; i++)
        {
            fseek (fsource, seek_head, SEEK_CUR);
            if (fread(buf, sizeof (char), sector_data, fsource)!=sector_data)
            {
                printf ("Error reading from %s: %s\n", infile->path, strerror (errno));
                remove (outfile->path);
                return EXIT_FAILURE;
            }
            if (fwrite (buf, sizeof (char), sector_data, fdest)!=sector_data)
            {
                printf ("Error writing to %s: %s\n", outfile->path, strerror (errno));
                remove (outfile->path);
                return EXIT_FAILURE;
            }
            fseek (fsource, seek_ecc, SEEK_CUR);
            main_percent (i*100/source_length);
        }
    } printf ("100%% [:=====================:]\n");

    // *** create Toc or Cue file is requested ***
    if (cue == 1) {
        if (cuesheets(outfile->path))
            return EXIT_FAILURE;
    }
    else if (toc == 1) {
        if (toc_file(outfile->path, sub_toc))
            return EXIT_FAILURE;
    }
    if ((toc == 0) && (cue == 0)) {
        printf ("Created iso9660: %s\n", outfile->path);
    }
/*
    n_mdf = number_file (outfile->path) - 1;
    if (n_mdf > 1)  {
        printf ("\rDetect %d md* file and now emerge this\n", n_mdf);
    }
*/
    return EXIT_SUCCESS;
}
