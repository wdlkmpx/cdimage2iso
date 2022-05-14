/*
    Copyright 2008-2011 Luigi Auriemma

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA

    http://www.gnu.org/licenses/gpl.txt
*/

#include "main.h"
#include "w_endian.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <ctype.h>

typedef uint8_t     u8;
typedef uint16_t    u16;
typedef int32_t     i32;
typedef uint32_t    u32;
typedef int64_t     i64;
typedef uint64_t    u64;

#define VER         "0.1.1"

#pragma pack(2)
typedef struct {
    u8      id[4];
    u32     size;
} nrg_chunk_t;
#pragma pack()


static char *path2fname (char *path);
static u8 *frames2time(u64 num);
static int _nrg2cue(FILE *fd, u64 nrgoff, char *fileo);
static void nrg_truncate (char *fileo, int secsz);
static char *change_ext (char *fname, char *ext);
static FILE *open_file (char *fname, int write);
static void myfr(FILE *fd, void *data, unsigned size);
static void myfw(FILE *fd, void *data, unsigned size);
static int getxx(u8 *data, u64 *ret, int bits, int intnet);
static int fgetz(u8 *data, int size, FILE *fd);
static void exit_error (int show_perror);

static char *out_iso    = NULL;


//int main(int argc, char *argv[])
int nrg2cue (struct file_info *infile, struct file_info *outfile)
{
    char *filei;

    setbuf(stdout, NULL);

    filei = infile->path;
    out_iso = outfile->path;

    nrg_truncate (filei, 5000);  // 5000 is enough

    printf("\n- finished\n");

    return 0;
}


char *path2fname (char *path)
{
    char *p;
    p = strrchr(path, '\\');
    if (!p) p = strrchr(path, '/');
    if (p) return(p + 1);
    return(path);
}


u8 *frames2time(u64 num)
{
    int     mm,
            ss,
            ff;
    static u8   ret_time[32];

    num /= 2352;
    ff = num % 75;
    ss = (num / 75) % 60;
    mm = (num / 75) / 60;
    sprintf((char*)ret_time, "%02d:%02d:%02d", mm, ss, ff);
    return(ret_time);
}


void nrg_mode(FILE *fdcue, int mode, int track, u32 sectsz)
{
    switch(mode) {
        // case 2: yes, this is data mode 2 BUT the CUE will give an error if you use this mode */
        case 3: fprintf(fdcue, "    TRACK %02d MODE2/%u\r\n", track, sectsz); break;
        case 7: fprintf(fdcue, "    TRACK %02d AUDIO\r\n",    track);         break;
        case 0:
        default:fprintf(fdcue, "    TRACK %02d MODE1/%u\r\n", track, sectsz); break;
    }
}



void quick_percentage(u64 num)
{
    static u64  next = 0;
    static u64  step = 0;
    static int  perc = 0;

    if (next <= 0) {
        next = num;
        step = num / 100;
        perc = 0;
    }
    if (num <= next) {
        printf("  %3d%%\r", perc);
        next -= step;
        perc++;
        if (perc >= 100) fputc('\n', stdout);
    }
}


int dump_iso(FILE *fd, u64 index1)
{
    static int  done = 0;
    static u8   buff[4096];
    FILE    *fdiso  = NULL;
    u64     old_off = 0,
            size;
    int     t;

    if (!out_iso) return(-1);
    if (done) return(-1);

    old_off = ftell(fd);
    fseek(fd, 0, SEEK_END);
    size = ftell(fd) - index1;
    fseek(fd, old_off, SEEK_SET);

    printf("\n"
        "- dump ISO:\n"
        "  offset: %08"PRIx64"\n"
        "  size:   %08"PRIx64"\n"
        "  name:   %s\n",
        index1,
        size,
        out_iso);

    fdiso = fopen(out_iso, "rb");
    if (fdiso) {
        fclose(fdiso);
        printf("\n- the output ISO file already exists, do you want to overwrite it? (y/N):\n  ");
        buff[0] = 0;
        fgets((char*)buff, sizeof(buff), stdin);
        if (tolower(buff[0]) != 'y') return(-1);
    }

    old_off = ftell(fd);
    if (fseek(fd, index1, SEEK_SET)) return(-1);

    fdiso = fopen(out_iso, "wb");
    if (!fdiso) exit_error(1);;
    for (;;) {
        quick_percentage(size);
        if (!size) break;    // so it shows 100%
        t = sizeof(buff);
        if (t > size) t = size;
        myfr(fd, buff, t);
        myfw(fdiso, buff, t);
        size -= t;
    }
    fclose(fdiso);

    fseek(fd, old_off, SEEK_SET);
    done++;
    return(0);
}


int _nrg2cue(FILE *fd, u64 nrgoff, char *fileo)
{
    nrg_chunk_t chunk;
    FILE    *fdcue  = NULL,
            *fdcdt  = NULL;
    u64     index0,
            index1,
            index2;
    u32     sectsz,
            mode;
    int     i,
            numsz,
            track,
            firstindex = 1;
    char    *filec;
    char    *filecdt;
    u8      *buff   = NULL,
            *p,
            *l;

    if (fseek(fd, nrgoff, SEEK_SET)) {
        printf("- Alert: wrong NRG header offset\n");
        return(-1);
    }

    printf("- generate the CUE file\n");
    filec = change_ext(fileo, ".cue");
    fdcue = open_file(filec, 1);
    fprintf(fdcue, "FILE \"%s\" BINARY\r\n", path2fname(fileo));

    track = 1;
    for (;;)
    {
        // get tracks and do more things
        myfr(fd, &chunk, sizeof(chunk));
        chunk.size = be32toh (chunk.size);
        if (!memcmp(chunk.id, "NER5", 4) || !memcmp(chunk.id, "NERO", 4))
        {
            break;
        }
        else if (!memcmp(chunk.id, "DAOX", 4) || !memcmp(chunk.id, "DAOI", 4))
        {
            if (chunk.size >= 22) {
                buff = realloc(buff, chunk.size);
                if (!buff) exit_error(1);;
                myfr(fd, buff, chunk.size);

                numsz = (!memcmp(chunk.id, "DAOX", 4)) ? 8 : 4;

                p = buff + 22;
                l = buff + chunk.size - (4 + 4 + (numsz * 3));
                for (i = 0; p <= l; i++) {
                    p += 10;
                    sectsz = *(u32 *)p; p += 4;
                    sectsz = be32toh (sectsz);
                    mode   = p[0];      p += 4;
                    p += getxx(p, &index0, numsz << 3, 1);
                    p += getxx(p, &index1, numsz << 3, 1);
                    p += getxx(p, &index2, numsz << 3, 1);
                    //#ifdef VERBOSE
                    printf("  %4.4s %08x %02x %08"PRIx64" %08"PRIx64" %08"PRIx64"\n", chunk.id, sectsz, mode, index0, index1, index2);
                    //#endif
                    nrg_mode(fdcue, mode, track, sectsz);
                    if (firstindex) {
                        fprintf(fdcue, "        INDEX 00 00:00:00\r\n");
                        firstindex = 0;
                    } else if (index1 > index0) {
                        fprintf(fdcue, "        INDEX 00 %s\r\n", frames2time(index0));
                    }

                    fprintf(fdcue, "        INDEX 01 %s\r\n", frames2time(index1));
                    dump_iso(fd, index1);
                    track++;
                }
                continue;
            }
        }
        else if (!memcmp(chunk.id, "ETN2", 4) || !memcmp(chunk.id, "ETNF", 4))
        {
            if (chunk.size >= 22) {
                buff = realloc(buff, chunk.size);
                if (!buff) exit_error(1);;
                myfr(fd, buff, chunk.size);

                numsz = (!memcmp(chunk.id, "ETN2", 4)) ? 8 : 4;

                sectsz = 2352;  // right???
                p = buff;
                l = buff + chunk.size - ((numsz * 2) + 4 + 4 + 4);
                for (i = 0; p <= l; i++) {
                    p += getxx(p, &index1, numsz << 3, 1);
                    p += getxx(p, &index2, numsz << 3, 1);
                    mode   = p[0];      p += 4;
                    p += 4 + 4;

                    //#ifdef VERBOSE
                    printf("  %4.4s %02x %08"PRIx64" %08"PRIx64"\n", chunk.id, mode, index1, index2);
                    //#endif

                    nrg_mode(fdcue, mode, track, sectsz);
                    if (!i) fprintf(fdcue, "        INDEX 00 00:00:00\r\n");

                    fprintf(fdcue, "        INDEX 01 %s\r\n", frames2time(index1));
                    dump_iso(fd, index1);
                    track++;
                }
                continue;
            }
        }
        else if (!memcmp(chunk.id, "CDTX", 4))
        {
            buff = realloc(buff, chunk.size);
            if (!buff) exit_error(1);;
            myfr(fd, buff, chunk.size);

            filecdt = change_ext(fileo, ".cdt");
            fdcdt = open_file(filecdt, 1);
            myfw(fdcdt, buff, chunk.size);
            fclose(fdcdt);

            fprintf(fdcue, "CDTEXTFILE \"%s\"\r\n", path2fname(filecdt));
            continue;
        }
        if (fseek(fd, chunk.size, SEEK_CUR)) {
            break;
        }
    }
    fclose(fdcue);
    if (buff) free(buff);
    return(0);
}


void nrg_truncate (char *fileo, int secsz)
{
    FILE    *fd;
    u64     nrgoff;
    u8      *buff,
            *p;
    int isnrg = 0;
    printf("- open %s\n", fileo);
    fd = fopen(fileo, "r+b");
    if (!fd) exit_error(1);;

    if (!fseek(fd, -secsz, SEEK_END))
    {
        buff = malloc(secsz);
        if (!buff) exit_error(1);;
        myfr(fd, buff, secsz);
        for (p = buff + secsz - 12; p >= buff; p--)
        {
            if (!memcmp(p, "NER5", 4)) {
                nrgoff = *(u64 *)(p + 4);
                p += 12;
                isnrg = 1;
                break;
            }
            if (!memcmp(p, "NERO", 4)) {
                nrgoff = *(u32 *)(p + 4);
                p += 8;
                isnrg = 1;
                break;
            }
        }
        if (!isnrg) {
            printf ("NOT NRG");
            exit_error(1);;
        }
        if (p >= buff) {
            nrgoff = be64toh (nrgoff);
            _nrg2cue(fd, nrgoff, fileo);
        }
        free(buff);
    }
    fclose(fd);
}


char *change_ext(char *fname, char *ext)
{
    char *p;

    p = malloc(strlen(fname) + strlen(ext) + 1);
    if (!p) exit_error(1);;
    strcpy(p, fname);
    fname = p;
    p = strrchr(fname, '.');
    if (!p || (p && (strlen(p) != 4))) p = fname + strlen(fname);
    strcpy(p, ext);
    return(fname);
}


FILE *open_file (char *fname, int write)
{
    FILE    *fd;
    u8      ans[16];

    if (write) {
        printf("- create %s\n", fname);
        fd = fopen(fname, "rb");
        if (fd) {
            fclose(fd);
            printf("\n- the output file already exists, do you want to overwrite it (y/N)?\n  ");
            fgetz(ans, sizeof(ans), stdin);
            if ((ans[0] != 'y') && (ans[0] != 'Y')) exit_error(0);
        }
        fd = fopen(fname, "wb");
        if (!fd) exit_error(1);;
    } else {
        printf("- open %s\n", fname);
        fd = fopen (fname, "rb");
        if (!fd) exit_error(1);;
    }
    return(fd);
}


void myfr(FILE *fd, void *data, unsigned size)
{
    if (fread(data, 1, size, fd) == size) return;
    printf("\nError: incomplete input file, can't read %u bytes\n", size);
    exit_error(0);
}


void myfw(FILE *fd, void *data, unsigned size)
{
    if (fwrite(data, 1, size, fd) == size) return;
    printf("\nError: problems during the writing of the output file\n");
    exit_error(0);
}

int getxx(u8 *data, u64 *ret, int bits, int intnet)
{
    u64     num;
    int     i,
            bytes;
    num = 0;
    bytes = bits >> 3;
    for (i = 0; i < bytes; i++) {
        if (!intnet) {   // intel/little endian
            num |= (data[i] << (i << 3));
        } else {        // network/big endian
            num |= (data[i] << ((bytes - 1 - i) << 3));
        }
    }
    *ret = num;
    return(bytes);
}


int fgetz(u8 *data, int size, FILE *fd)
{
    u8      *p;
    fflush(fd);
    if (!fgets((char*)data, size, fd)) {
        data[0] = 0;
        return(0);
    }
    for (p = data; *p && (*p != '\n') && (*p != '\r'); p++);
    *p = 0;
    return(p - data);
}


static void exit_error (int show_perror)
{
    cdimage2iso_cleanup ();
    if (show_perror) {
        perror("\nError");
    }
    exit(1);
}
