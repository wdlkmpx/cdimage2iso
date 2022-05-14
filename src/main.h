//
// This file is put in the public domain
//

#ifndef __CDIMAGE2ISO_H
#define __CDIMAGE2ISO_H 1

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef _LARGEFILE_SOURCE
#define _LARGEFILE_SOURCE 1
#endif
#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE 1
#endif
#undef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>

#if defined(__APPLE__)
#   define fseek fseeko
#   define ftell ftello
#elif defined(__linux__) || defined(__MINGW32__)
#   define off_t off64_t
#   define fopen fopen64
#   define fseek fseeko64
#   define ftell ftello64
//#elif defined(__MINGW32__)
//#   define fseek _fseeki64
//#   define ftell _ftelli64
//#include <io.h>
//#   define lseek _lseeki64
//#   define off_t off64_t
//#include <sys/types.h>
//#include <sys/stat.h>
//#   define stat  _stati64
//#   define fstat _fstati64
//#   define wstat _wstati64
#endif


enum
{
    FORMAT_ISO = 1,
    FORMAT_CSO,
    FORMAT_DAX,
    FORMAT_ISZ,
    FORMAT_BWI,
    FORMAT_B5I,
    FORMAT_MDS,
    FORMAT_CDI,
    FORMAT_DAA,
    FORMAT_DMG,
    FORMAT_GBI,
    FORMAT_IMG,
    FORMAT_MDF,
    FORMAT_NRG,
    FORMAT_PDI,
    FORMAT_UIF,
    FORMAT_BIN,
    FORMAT_CUE,
};

struct sig_info
{
    unsigned int offset;
    const char *signature;
    unsigned int siglen;
    const char *desc;
};

// ===========================================================

struct file_info
{
    FILE *stream;
    char *path;
    struct stat *st;
    int format;
};

struct app_options
{
    int overwrite;
    char flags[100]; // options to be parsed by plugins
    struct file_info *infile;
    struct file_info *outfile;
};

extern struct app_options program_options;

// ===========================================================
void clear_fileinfo (struct file_info *finfo, int remove_empty_file);
void cdimage2iso_cleanup (void);
// ===========================================================

/*
 - to reduce code lines, the xxx2iso functions only handle open file streams
 - main() opens and closes the input and output files
*/

/* b5i2iso.c */
// TODO: app used to support --cue opt
int b5i2iso (struct file_info *infile, struct file_info *outfile);

/* bchunk.c */
int bchunk (struct file_info *infile, struct file_info *outfile);

/* bin2iso.c */
int bin2iso (struct file_info *infile, struct file_info *outfile);

/* ccd2iso.c */
int ccd2iso (struct file_info *infile, struct file_info *outfile);

/* cdirip.c (replaces cdi2iso.c) */
// TODO: possible --cue opt (cdirip -bin)
int cdirip (struct file_info *infile, struct file_info *outfile);

/* cso2iso.c */
int cso2iso (struct file_info *infile, struct file_info *outfile);

/* daa2iso.c */
int daa2iso (struct file_info *infile, struct file_info *outfile);

/* daxcreator.c */
int dax2iso (struct file_info *infile, struct file_info *outfile);

/* dmg2iso.c */
int dmg2iso (struct file_info *infile, struct file_info *outfile);

/* iat.c */
int iat (struct file_info *infile, struct file_info *outfile);

/* isz2iso.c */
int isz2iso (struct file_info *infile, struct file_info *outfile);

/* mdf2iso.c */
// TODO: app used to support --cue / --toc opts
int mdf2iso (struct file_info *infile, struct file_info *outfile);

/* nrg2iso.c */
int check_iso9660 (struct file_info *file);
int nrg2iso (struct file_info *infile, struct file_info *outfile);

/* nrg2cue.c */
int nrg2cue (struct file_info *infile, struct file_info *outfile);

/* pdi2iso.c */
// TODO: app used to support --cue opt
int pdi2iso (struct file_info *infile, struct file_info *outfile);

/* uif2iso.c */
int uif2iso (struct file_info *infile, struct file_info *outfile);

#endif
