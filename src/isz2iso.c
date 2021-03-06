/*
 To be written...
 
 Resources:
 
 https://en.wikipedia.org/wiki/UltraISO#ISZ_format
 http://www.ezbsystems.com/isz/iszspec.txt
 */

#include "main.h"
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
 
/*
ISZ File Format Specification
Version: 1.00 
Revised: July 3, 2006
Copyright (c) 2002-2006 EZB Systems, Inc., All Rights Reserved.

1. Purpose
----------
This specification is intended to define an interoperable 
ISO CD/DVD image storage and transfer format. 

2. Disclaimer
---------------

Although EZB Systems will attempt to supply current and accurate
information relating to its file formats, algorithms, and the
subject programs, the possibility of error or omission cannot 
be eliminated. EZB Systems therefore expressly disclaims any warranty 
that the information contained in the associated materials relating 
to the subject programs and/or the format of the files created or
accessed by the subject programs and/or the algorithms used by
the subject programs, or any other matter, is current, correct or
accurate as delivered.  Any risk of damage due to any possible
inaccurate information is assumed by the user of the information.
Furthermore, the information relating to the subject programs
and/or the file formats created or accessed by the subject
programs and/or the algorithms used by the subject programs is
subject to change without notice.

3. General Format of a .ISZ file
--------------------------------

Large .ISZ files can span multiple disk media or be split into 
user-defined segment sizes.  

Overall .ISZ file format:

[ISZ file header]
[Segment defination table] 
[Chunk defination table] 
[Chunk #1 data]
.
.
.
[Chunk #n data]

3.1 ISZ file header: */

typedef struct isz_file_header
{
  char signature[4];             // 'IsZ!'
  unsigned char header_size;     // header size in bytes
  char ver;                      // version number
  unsigned int vsn;              // volume serial number

  unsigned short sect_size;      // sector size in bytes
  unsigned int total_sectors;    // total sectors of ISO image

  char has_password;             // is Password protected?
  
  int64_t segment_size;          // size of segments in bytes
  
  unsigned int nblocks;          // number of chunks in image
  unsigned int block_size;       // chunk size in bytes (must be multiple of sector_size)
  unsigned char ptr_len;         // chunk pointer length

  char seg_no;                   // segment number of this segment file, max 99

  unsigned int ptr_offs;         // offset of chunk pointers, zero = none

  unsigned int seg_offs;         // offset of segment pointers, zero = none

  unsigned int data_offs;        // data offset
  
  char reserved;

} isz_header;

//The 'has_password' field should be one of the following values:

#define ADI_PLAIN       0        // no encryption
#define ADI_PASSWORD    1        // password protected (not used)
#define ADI_AES128      2        // AES128 encryption
#define ADI_AES192      3        // AES192 encryption
#define ADI_AES256      4        // AES256 encryption

/*3.2. Segment defination table (SDT)

This descriptor exists only if 'seg_offs' field of ISZ header is not zero. 

Immediately following the ISZ file header to define segment information.*/

typedef struct isz_seg_st
{
  int64_t size;                  // segment size in bytes
  int num_chks;                  // number of chunks in current file
  int first_chkno;               // first chunk number in current file
  int chk_off;                   // offset to first chunk in current file
  int left_size;                 // uncompltete chunk bytes in next file
} isz_seg;

/*If an ISZ file is not segmented (has only one segment), no SDT should be
stored. The 'seg_offs' field in ISZ file header should be zero.

For ISZ files with N segments, N+1 SDT entries should be stored. 'size' field of 
the last SDT entry should be zero. 

3.3. Chunk defination table (CDT)

This descriptor exists only if 'ptr_offs' field of ISZ haeder is not zero. 

Immediately following the SDT to define chunk information.*/

///typedef struct isz_chunk_st
///{
///  chunk_flag;
///  blk_len;
///} isz_chunk;

/*'chunk_flag' and 'blk_len' defination is variable according to 'ptr_len' field of 
ISZ header.  

The 'chunk_flag' should be one of the following values:*/

#define ADI_ZERO        0x00    // all zeros chunk
#define ADI_DATA        0x40    // non-compressed data
#define ADI_ZLIB        0x80    // ZLIB compressed
#define ADI_BZ2         0xC0    // BZIP2 compressed

/*Number of CDT entries should equal to 'nblocks' field in ISZ header, 'blk_len'
field in CDT entry MUST be less or equal to 'block_size' in ISZ header.

3.4 Chunk data

According to 'chunk_flag' defination, a chunk may have no data (ADI_ZERO) or 
'blk_len' bytes of compressed (ADI_ZLIB or ADI_BZ2) or non-compressed data (ADI_DATA)

3.5. Splitting and Spanning ISZ files

Spanning is the process of segmenting a ISZ file across multiple removable media. 
This support has typically been provided for floppy diskettes, CD-R discs and DVD-R discs. 

File splitting is a newer derivative of spanning. Splitting follows the same 
segmentation process as spanning, however, it does not require writing each
segment to a unique removable medium and instead supports placing all pieces onto 
local or non-removable locations such as file systems, local drives, folders, etc...

Split ISZ files are typically written to the same location and are subject to name 
collisions if the spanned name format is used since each segment will reside on the same 
drive. To avoid name collisions, split archives are named as follows.

    Segment 1 = filename.isz
    Segment 2 = filename.i01
    Segment n = filename.i(n-1)

The .ISZ extension is used on the first segment to support quickly reading the ISO image 
information directory. The segment number n should be a decimal value.

    Capacities for split archives are as follows.

    Maximum number of segments = 99
    Minimum segment size = 100KB  
    Maximum segment size = 4TB - 1 (64 bits)
          
Segment sizes may be different however by convention, all segment sizes should be the same 
with the exception of the last, which may be smaller. 

4. Encryption Method

Only chunk data is encrypted if 'has_password' filed of ISZ header is defined.

There are three encryption method may be used: AES128, AES192 and AES256.

Reference implementations for these algorithms are available from either commercial or 
open source distributors.  Readily available cryptographic toolkits make implementation of 
the encryption features straight-forward.  

Encryption is always applied to a chunk after compression. The block oriented algorithms 
all operate in Cypher Block Chaining (CBC) mode.  The block size used for AES encryption is 16.  

5. Compression Method

Chunk data may be compressed by ZLIB or BZIP2 method.

ZLIB is a compression library written by Jean-loup Gailly (compression) and Mark Adler (decompression).
BZIP2 is an open-source data compression algorithm developed by Julian Seward.  
Information and source code for these algorithm can be found on the internet.

The length of compressed data is defined by CDT entries, and the length of uncompressed data
is always less than 'block_size' field in ISZ header.

6. Usful Tips

1) Capacity of an ISZ file can be caculated by 'total_sectors' field ('sect_size' is always 2048 for 
   ISO CD/DVD images)
2) SDT and CDT are the central information of an ISZ file. Sector data can be located this way:
   - Calculate chunk number by: chk_no = (sector_no * sect_size) / block_size
   - Search in CDT, get chunk length and offset
   - Search in SDT, get which segment file to read. For data of last chunk, you may need to read left 
     byte from next segment file
   - Decrypt chunk data as needed
   - Uncompress chunk data as needed
   - Get sector data from chunk buffer 
3) Sengment files may be located in differnet folder. A dialogue box for asking file location is needed 
   for this situation. Segment file can be verified by 'seg_no' and 'vsn' field in ISZ header.  

7. Change Process
------------------

In order for the .ISZ file format to remain a viable definition, this specification should be 
considered as open for periodic review and revision.  Although this format was originally 
designed with a certain level of extensibility, not all changes in technology (present or future) 
were or will be necessarily considered in its design.  If your application requires new definitions 
to this format, or if you would like to submit new data structures, please forward your request to
isz@ezbsystems.com.  All submissions will be reviewed for possible inclusion into future versions 
of this specification.  Periodic revisions to this specification will be published to ensure 
interoperability. We encourage comments and feedback that may help improve clarity or content.

8. Incorporating ISZ format into Your Product
------------------------------------------------------------------

EZB Systems offers a free license for certain technological aspects described above under certain restrictions 
and conditions. A free SDK package is also available. Please contact EZB Systems at isz@ezbsystems.com with 
regard to acquiring a license.
*/

static char *infile_path;
static char *outfile_path;
static FILE *inputfile;
static FILE *outputfile;


static int isz_decompress()
{
    printf ("Sorry, support for this format is yet to be added...\n");
    return 0;
}


// int main (int argc, char **argv)
int isz2iso (struct file_info *infile, struct file_info *outfile)
{
    int ret;

    inputfile  = infile->stream;
    outputfile = outfile->stream;

    infile_path  = infile->path;
    outfile_path = outfile->path;

    ret = isz_decompress ();

    return ret;
}
