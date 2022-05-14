/*
   DAX Creator 0.3 (Dark_AleX)
*/

#include "main.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#define byte unsigned char
#define u16 unsigned short
#define u32 unsigned int

// ==============================================================
// daxfile.h

#define DAXFILE_SIGNATURE 0x00584144 /* "DAX\0" */
#define DAX_FRAME_SIZE	8192
#define DAXFORMAT_VERSION_0	0
#define DAXFORMAT_VERSION_1	1
#define MAX_NCAREAS 192

typedef struct
{
	u32 signature; 
	u32 decompsize; /* Size of original non-compressed file */
	u32 version;   /* DAX format version */
	u32 nNCareas; /* Number of non-compressed areas. */
	u32 reserved[4]; /* Reserved for future use. It must be zero */
} DAXHeader;

typedef struct
{
	u32 frame; /* First frame of the NC Area */
	u32 size; /*  Size of the NC Area in frames */
} NCArea;

// ==============================================================

/* Global vars */
static FILE *isofile = NULL;
static FILE *daxfile = NULL;
static FILE *infofile = NULL;
static char *infofile_path = NULL;
static char *inputfile;
static char *outputfile;

static u32 *offsets = NULL; 
static u16 *lengths = NULL;
static byte combuf[DAX_FRAME_SIZE+1024], decbuf[DAX_FRAME_SIZE];
static NCArea ncareas[MAX_NCAREAS];
static int waitinput;


/* Functions */
static int getfilesize(FILE *f)
{
	int s, res;
	
	s = ftell(f);
	fseek (f, 0, SEEK_END);
	res = ftell(f);
	fseek(f, s, SEEK_SET);

	return res;
}


static int IsNCArea(u32 frame, int count)
{
	int i;

	for (i = 0; i < count; i++)
	{
		if (frame >= ncareas[i].frame &&
			((frame - ncareas[i].frame) < ncareas[i].size))
		{
			return 1;
		}
	}

	return 0;
}


static int compressFile (FILE *inputf, FILE *outputf, int complevel, int nNCAreas)
{
	DAXHeader header;
	u32 nframes, read, written, csize;
	int i, j, fpointer;	
	char *report;

	isofile = inputf;
	daxfile = outputf;

	i = strlen (inputfile) + 6;
	infofile_path = malloc ((size_t)i);
	snprintf (infofile_path, (size_t)i-1, "%s%s", inputfile, ".log");
	infofile = fopen (infofile_path, "w");
	if (!infofile) {
		printf("Warning: cannot open/create info file %s\n", infofile_path);
	}

	memset(&header, 0, sizeof(header));
	header.signature = DAXFILE_SIGNATURE;
	header.decompsize = getfilesize(isofile);

	if (header.decompsize < 0)
	{
		printf("Error getting file size.\n");
		return -1;
	}

	nframes = header.decompsize / DAX_FRAME_SIZE;

	if ((header.decompsize % DAX_FRAME_SIZE) != 0)
		nframes++;

	printf("N of frames: %d\n", nframes);

	if (nNCAreas == 0)
	{
		header.version = DAXFORMAT_VERSION_0;
	}

	else
	{
		header.version = DAXFORMAT_VERSION_1;
		header.nNCareas = nNCAreas;
	}

	offsets = (u32 *)malloc(nframes * 4);
	lengths = (u16 *)malloc(nframes * 2);

	fwrite(&header, 1, sizeof(header), daxfile);
	fwrite(offsets, 1, nframes * 4, daxfile);
	fwrite(lengths, 1, nframes * 2, daxfile);

	if (nNCAreas > 0)
		fwrite(ncareas, 1, sizeof(ncareas), daxfile); 	

	printf("Compressing...\n");

	i = 0;
	fpointer = ftell(daxfile);

	do
	{
		int res;

		read = fread(decbuf, 1, DAX_FRAME_SIZE, isofile);

		if (IsNCArea(i, nNCAreas))
		{
			fwrite(decbuf, 1, read, daxfile);
			csize = DAX_FRAME_SIZE;
			goto next_iteration;			
		}

		if (read > 0)
		{
			csize = DAX_FRAME_SIZE + 1024;
			res = compress2(combuf, (uLongf*)&csize, decbuf, read, complevel);
			
			if (res == Z_BUF_ERROR)
			{
				printf("Not enough memory in compressed buffer!\n");
				return -1;
			}

			else if (res != Z_OK)
			{
				printf("Memory error while compressing.\n");
				return -1;
			}

			written = fwrite(combuf, 1, csize, daxfile);
			
			if (written != csize)
			{	
				printf("I/O write error.\n");
                return -1;
				break;
			}

			if (infofile)
			{
				fprintf(infofile, "Frame #%d: %d -> %d",
						i, DAX_FRAME_SIZE, csize);

				fprintf(infofile, "(%d%%)\n", 
							(csize * 100) / DAX_FRAME_SIZE);
			}	

next_iteration:
			offsets[i] = fpointer;
			lengths[i] = (u16)csize;
			i++;
			fpointer += csize;
		}
	} while (read == DAX_FRAME_SIZE);

	printf("Total frames written: %d\n", i);
	
	printf("Writing tables...\n");
	fseek(daxfile, sizeof(header), SEEK_SET);

	if (ftell(daxfile) != sizeof(header))
	{
		return -1;
	}

	fwrite(offsets, 1, nframes * 4, daxfile);
	fwrite(lengths, 1, nframes * 2, daxfile);
	printf("Table size: %d+%d=%d\n", nframes*4, nframes*2, nframes*6);

	printf("Finished.\n\n");
	
	i = header.decompsize / 1024;
	j = getfilesize(daxfile) / 1024;

	report = "Report: %d KB -> %d KB (%d%%)\n\n"; 
	printf(report, i, j, (100 * j) / i);
	if (infofile)
		fprintf(infofile, report);
	
	return 0;
}


static int decompressFile (FILE *inputf, FILE *outputf)
{
	DAXHeader header;
	u32 nframes, read, written, dsize;
	int i;

	daxfile = inputf;
	isofile = outputf;

	read = fread(&header, 1, sizeof(header), daxfile);

	if (read != sizeof(header) || header.signature != DAXFILE_SIGNATURE)
	{
		printf("Input is not a valid DAX file.\n");
		return -1;
	}

	if (header.version > DAXFORMAT_VERSION_1)
	{
		printf("The version of the file is greater than supported.\n");
		return -1;
	}

	nframes = header.decompsize / DAX_FRAME_SIZE;

	if ((header.decompsize % DAX_FRAME_SIZE) != 0)
		nframes++;

	printf("N of frames: %d\n", nframes);

	offsets = (u32 *)malloc(nframes * 4);
	lengths = (u16 *)malloc(nframes * 2);

	read = fread(offsets, 1, nframes * 4, daxfile);
	if (read != nframes * 4) {
		printf("Corrupted input file.\n");
		return -1;
	}

	read = fread(lengths, 1, nframes * 2, daxfile);
	if (read != nframes * 2) {
		printf("Corrupted input file.\n");
		return -1;
	}
	
	if (header.nNCareas > 0)
	{
		read = fread(ncareas, 1, sizeof(ncareas), daxfile);
		if (read != sizeof(ncareas)) {
			printf("Corrupted input file.\n");
			return -1;
		}
	}

	printf("Decompressing...\n");

	for (i = 0; i < (int)nframes; i++)
	{
		int res;

		fseek(daxfile, offsets[i], SEEK_SET);
		read = fread(combuf, 1, lengths[i], daxfile);

		if (IsNCArea(i, header.nNCareas))
		{
			fwrite(combuf, 1, DAX_FRAME_SIZE, isofile);
			continue;
		}

		if (read != lengths[i]) {
			printf("Input seems to be corrupted.\n");
			return -1;
		}

		dsize = DAX_FRAME_SIZE;

		res = uncompress(decbuf, (uLongf*)&dsize, combuf, lengths[i]);

		if (res != Z_OK) {
			printf("Error while decompressing (corrupt input?)\n");
			return -1;
		}

		written = fwrite(decbuf, 1, dsize, isofile);

		if (written != dsize)
		{
			printf("I/O error writing to output file.\n");
			return -1;
		}
	}

	printf("Finished.\n\n");	
	return 0;
}


/*
    daxcr img.iso img.dax = compress
    daxcr img.dax img.iso = decompress
*/

//int main(int argc, char *argv[])
int dax2iso (struct file_info *infile, struct file_info *outfile)
{
    int decompress, complevel;
    int ret;
    const char * ext;

    inputfile  = infile->path;
    outputfile = outfile->path;

    ext = strrchr (inputfile, '.');
    if (ext && strcasecmp(ext,".dax") == 0) {
        printf ("Decompress DAX file to ISO\n");
        decompress = 1;
    } else {
        printf ("Compress ISO file to DAX\n");
        decompress = 0;
    }

    complevel = 9; //0-9
    waitinput = 0;

    if (complevel == 0)
        complevel = Z_DEFAULT_COMPRESSION;

    if (!decompress)
    {
        int n;
        n = 0;
        //n = DAX_FRAME_SIZE;
        ret = compressFile(infile->stream, outfile->stream, complevel, n);
    }
    else
    {
        ret = decompressFile(infile->stream, outfile->stream);
    }

    // free memory
    if (infofile) fclose(infofile);
    if (offsets) free(offsets);
    if (lengths) free(lengths);
    if (infofile_path) free (infofile_path);

    return ret;
}
