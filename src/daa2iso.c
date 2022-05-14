/*
    Copyright 2007,2008,2009 Luigi Auriemma <aluigi@autistici.org>

    origin: daa2iso v0.1.7e (http://aluigi.org/mytoolz.htm#daa2iso)

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

/***
 - PowerISO may compress any supported CD image and the result is
      a .daa archive that can only be decompressed to ISO.
 - DAA is only a simple ISO compressed image, so it can't handle
      audio or mixed/extra content but only the good old ISO data.
 - Multipart files are supported so it's enough to
      select the first one (i.e file.part01.daa).
***/

#include "main.h"
#include "w_endian.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "LzmaDec.h"
#include "Bra.h"     // x86_Convert()

typedef uint8_t     u8;
typedef uint16_t    u16;
typedef uint32_t    u32;
typedef uint64_t    u64;

//#define DAA_DEBUG 1

#include <zlib.h>

#ifdef HAVE_LOCAL_ZLIB // the local zlib might not include crc32.c/h
// compatible with with zlib crc32
static u32 _crc32 (unsigned unused, u8 *data, u32 size);
#define crc32 _crc32
#endif

#ifdef _WIN32
#define strcasecmp stricmp
#endif

#define PRINTF64(x) (u32)(((x) >> 32) & 0xffffffff), (u32)((x) & 0xffffffff)    // I64x, llx? blah

#pragma pack(4)
typedef struct {
    u8  sign[16];    // DAA
    u32 size_offset; // where starts the list of sizes of the zipped chunks
    u32 version;     // version
    u32 data_offset; // where the zipped chunks start
    u32 b1;          // must be 1
    u32 b0;          // ever 0
    u32 chunksize;   // size of each output chunk
    u64 isosize;     // total size of the output ISO
    u64 daasize;     // total size of the DAA file
    u8  hdata[16];   // it's ever zero
    u32 crc;         // checksum calculated on the first 0x48 bytes
} daa_t;

#pragma pack(1)
typedef struct {
    u8 n1; // strange way to store numbers...
    u8 n2;
    u8 n3;
} daa_data_t;
#pragma pack()

enum {
    TYPE_DAA,
    TYPE_GBI,
    TYPE_NONE
};

static unsigned int daa2iso_read_bits(unsigned int bits, unsigned char *in, unsigned int in_bits, unsigned int lame, int lame_increase);
static void gburner_lame(u8 *data, int size, u8 crc8);
static void poweriso_lame(u8 *data, int size, u64 isosize);
static char *find_ext (char *fname, char *ext);
static FILE *daa_next(void);
static void myalloc(u8 **data, unsigned wantsize, unsigned *currsize);
static void myfr(FILE *fd, void *data, unsigned size);
static void myfw(FILE *fd, void *data, unsigned size);
static int unlzma(CLzmaDec *lzma, u8 *in, u32 insz, u8 *out, u32 outsz, int filter);
static u32 unzip (z_stream *z, u8 *in, u32 insz, u8 *out, u32 outsz);
static void le2host_daa(daa_t *daa);
static int fgetz(u8 *data, int size, FILE *fd);
static void exit_error (int show_perror);

// 7z - LzmaDecode()
static void *SzAlloc(ISzAllocPtr p, size_t size) { return(malloc(size)); }
static void SzFree(ISzAllocPtr p, void *address) { free(address); }
static ISzAlloc g_Alloc = { SzAlloc, SzFree };

static int multi = 0;
static int multinum;
static int daagbi  = TYPE_DAA;
static char *multi_filename;

//================================================================

static CLzmaDec *lzma = NULL;
static z_stream *z = NULL;
static u8 *buffer_in  = NULL;
static u8 *buffer_out = NULL;

static void cleanup (void)
{
    if (buffer_in)  free(buffer_in);
    if (buffer_out) free(buffer_out);
    if (lzma) free (lzma);
    if (z) free (z);
    buffer_in = buffer_out = NULL;
    lzma = NULL;
    z = NULL;
    cdimage2iso_cleanup ();
}

//================================================================
// daa_crypt.h

static u8   daa_crypt_table[128][256];

static void daa_crypt_key(u8 *pass, int num) {    // 00453360
    int     a,
            b,
            c,
            d,
            s,
            i,
            p,
            passlen;
    short   tmp[256];
    u8      *tab;

    passlen = strlen((char*)pass);
    tab = daa_crypt_table[num - 1];
    d = num << 1;

    for(i = 0; i < 256; i++) {
        tmp[i] = i;
    }
    memset(tab, 0, 256);

    if(d <= 64) {
        a = pass[0] >> 5;
        if(a >= d) a = d - 1;
        for(c = 0; c < d; c++) {
            for(s = 0; s != 11;) {
                a++;
                if(a == d) a = 0;
                if(tmp[a] != -1) s++;
            }
            tab[c] = a;
            tmp[a] = -1;
        }
        return;
    }
    a = pass[0];
    b = d - 32;
    a >>= 5;
    tmp[a + 32] = -1;
    tab[0] = a + 32;
    p = 1;
    for(s = 1; s < b; s++) {
        c = 11;
        if(p < passlen) {
            c = pass[p];
            p++;
            if(!c) c = 11;
        }
        for(i = 0; i != c;) {
            a++;
            if(a == d) a = 32;
            if(tmp[a] != -1) i++;
        }
        tmp[a] = -1;
        tab[s] = a;
    }
    i = pass[0] & 7;
    if(!i) i = 7;
    for(; s < d; s++) {
        for(c = 0; c != i;) {
            a++;
            if(a == d) a = 0;
            if(tmp[a] != -1) c++;
        }
        tmp[a] = -1;
        tab[s] = a;
    }
    for(i = 0; i < d; i++) {
        tmp[i] = tab[i];
    }
    i = pass[0] & 24;
    if(i) {
        a = 0;
        for(s = 0; s < d; s++) {
            for(c = 0; c != i;) {
                a++;
                if(a == d) a = 0;
                if(tmp[a] != -1) c++;
            }
            c = tmp[a];
            tmp[a] = -1;
            tab[s] = c;
        }
    }
}

static void daa_crypt_block(u8 *ret, u8 *data, int size) { // 00453540
    int     i;
    u8      c,
            t,
            *tab;

    if(!size) return;
    tab = daa_crypt_table[size - 1];

    memset(ret, 0, size);
    for(i = 0; i < size; i++) {
        c = data[i] & 15;
        t = tab[i << 1];
        if(t & 1) c <<= 4;
        ret[t >> 1] |= c;

        c = data[i] >> 4;
        t = tab[(i << 1) + 1];
        if(t & 1) c <<= 4;
        ret[t >> 1] |= c;
    }
}


static void daa_crypt(u8 *data, int size) {
    int     blocks,
            rem;
    u8      tmp[128],
            *p;
    blocks = size >> 7;
    for(p = data; blocks--; p += 128) {
        daa_crypt_block(tmp, p, 128);
        memcpy(p, tmp, 128);
    }

    rem = size & 127;
    if(rem) {
        daa_crypt_block(tmp, p, rem);
        memcpy(p, tmp, rem);
    }
}


void daa_crypt_init(u8 *pwdkey, u8 *pass, u8 *daakey) {
    int     i;

    for(i = 1; i <= 128; i++) {
        daa_crypt_key(pass, i);
    }
    daa_crypt_block(pwdkey, daakey, 128);
}


//================================================================

//int main (int argc, char *argv[])
int daa2iso (struct file_info *infile, struct file_info *outfile)
{
    daa_data_t  *daa_data;
    daa_t daa;
    FILE  *fdi = NULL,
          *fdo = NULL;
    u64 tot = 0;

    u32 daa_type,
        i     = 0,
        len   = 0,
        insz  = 0,
        outsz = 0,
        pwdtype,
        pwdcrc,
        daacrc,
        daas,       // amount of elements in the index table
        daas_mem,   // size of the memory containing the index table
        last_chunk,
        ver110_x,
        ver110_y,
        daa_dataz = 0;

    int ztype    = 1, // for 110  // INFLATE default =1 
        bitpos   = 0, // for 110
        bitsize  = 0, // for 110
        bittype  = 0, // for 110
        dolame   = 0,
        ver110_btype = 0, // swapped - tinflate
        dolamebits   = 0,
        lzma_filter  = 0;

    u8  ans[66],    // password goes from 1 to 64 chars
        pwdkey[128],
        daakey[128];

    char *filei;
    //char *fileo; // not used
    char *p, *signature;

    setbuf(stdout, NULL);

    fdo    = NULL;

    filei = infile->path;
    //fileo = outfile->path; // not used

    fdi = infile->stream;
    fdo = outfile->stream;

    myfr(fdi, &daa, sizeof(daa));
    daacrc = crc32(0, (u8 *)&daa, sizeof(daa) - 4);
    le2host_daa(&daa);
    signature = (char*) daa.sign;
    if (!strncmp(signature, "DAA", 16) || !strncmp(signature, "\xb8\xbd\xb6", 16)) {
        daagbi = TYPE_DAA;
    } else if (!strncmp(signature, "GBI", 16)) {
        daagbi = TYPE_GBI;
    } else {
        if (!strncmp(signature, "DAA VOL", 16) || !strncmp(signature, "GBI VOL", 16)) {
            printf("\n"
                "Error: you must choose the first DAA file (*.part01.daa, *.part001.daa or\n"
                "       *.daa) because this is a splitted archive\n");
            exit_error(0);
        } else {
            printf("\n"
                "Error: wrong DAA signature (%.16s)\n"
                "       do you want to continue handling it as a normal DAA file (y/N)?\n"
                "       ", signature);
            fgetz(ans, sizeof(ans), stdin);
            if ((ans[0] == 'y') || (ans[0] == 'Y')) {
                daagbi = TYPE_DAA;
            } else if (!strcasecmp((char*)ans, "daa")) {
                daagbi = TYPE_DAA;
            } else if (!strcasecmp((char*)ans, "gbi")) {
                daagbi = TYPE_GBI;
            } else {
                exit_error(0);
            }
        }
    }
    if (daacrc != daa.crc) {
        printf("- Alert: the CRC of the DAA header differs\n");
    }
    if (((daa.version != 0x100) && (daa.version != 0x110)) || (daa.b1 != 1)) {
        printf("- Alert: unknown DAA version (%08x %08x %08x)\n", daa.version, daa.b1, daa.b0);
    }

    if (daa.version == 0x100) {
        daas_mem = daa.data_offset - daa.size_offset;
        daa_data = malloc(daas_mem);
        if (!daa_data) exit_error(1);
        daas = daas_mem / 3;
    } else {
        ver110_x = daa.data_offset;
        ver110_y = daa.chunksize;

        daa.data_offset &= 0xffffff;
        daa.chunksize = (daa.chunksize & 0xfff) << 14;
        bittype = daa.hdata[5] & 7;
        bitsize = daa.hdata[5] >> 3;
        if (bitsize) bitsize += 10;
        if (!bitsize) {
            for (bitsize = 0, len = daa.chunksize; len > bittype; bitsize++, len >>= 1);
        }
        printf("- bits reader  %d %d\n", bittype, bitsize);

        daas_mem = daa.data_offset - daa.size_offset;
        daas = (daas_mem << 3) / (bittype + bitsize);
        daas_mem = (((bitsize + bittype) * daas) + 7) >> 3;
        if (ver110_y & 0x4000) {
            daas_mem += 0x10000;
            daa_dataz = *(u32 *)(daa.hdata + 1);
#ifdef WORDS_BIGENDIAN
            daa_dataz = le32toh (daa_dataz);
#endif
        }
        daa_data = malloc(daas_mem);
        if (!daa_data) exit_error(1);

            // seems that most of these fields are not supported by PowerISO too
        if (ver110_x & 0x80000000) {
            printf("- Alert: ver110_x 0x80000000 not supported yet, contact me if doesn't work!\n");
            if (ver110_y & 0x4000) {
                // daa.daasize -= daa_dataz;
            } else {
                // daa.daasize -= daas_mem;
            }
        }
        if (ver110_x & 0x10000000) {
            printf("- Alert: ver110_x 0x10000000 not supported yet, contact me if doesn't work!\n");
            // esi[8948] = 1;   // seems that if it's 1 the readbit function is not used and at its place is returned an error... senseless
        }
        if (ver110_y & 0x20000) {
            printf("- activate the lame decryption of the read bits\n");
            dolamebits = 1;
        }
        if (ver110_y & 0x8000000) {
            printf("- activate the lame decryption of the index table\n");
            dolame = 1;
        }
        if (ver110_y & 0x80000) {
            printf("- Alert: ver110_y & 0x80000 not supported yet, contact me if doesn't work!\n");
        }
        if (ver110_y & 0x100000) {
            printf("- activate LZMA\n");
            lzma = (CLzmaDec *) malloc (sizeof(CLzmaDec));
            lzma_filter = daa.hdata[6]; // this one comes from Pismo of Joe Lowe
            LzmaDec_Construct(lzma);
            LzmaDec_Allocate(lzma, daa.hdata + 7, LZMA_PROPS_SIZE, &g_Alloc);
        }
        ver110_btype = (ver110_y >> 0x17) & 3;
        if (daagbi == TYPE_GBI) ver110_btype ^= 1;
    }

    z = malloc(sizeof(z_stream));
    z->zalloc = Z_NULL;
    z->zfree  = Z_NULL;
    z->opaque = Z_NULL;
    z->avail_in = 0;
    z->next_in = Z_NULL;
    // 1..15 process zlib-wrapped deflate data
    // -1..-15 process raw deflate data with no header or trailer
    if (inflateInit2(z, -15) != Z_OK) {
        printf("\nError: zlib initialization error\n");
        exit_error(0);
    }

    printf("\n"
        "  version      %08x %08x %08x\n"
        "  size offset  %08x\n"
        "  data offset  %08x\n"
        "  chunk size   %08x\n"
        "  ISO size     %08x%08x\n"
        "  DAA size     %08x%08x\n"
        "  header data  %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n"
        "  header CRC   %08x\n"
        "\n",
        daa.version, daa.b1, daa.b0,
        daa.size_offset,
        daa.data_offset,
        daa.chunksize,
        PRINTF64(daa.isosize),
        PRINTF64(daa.daasize),
        daa.hdata[0],  daa.hdata[1],  daa.hdata[2],  daa.hdata[3],
        daa.hdata[4],  daa.hdata[5],  daa.hdata[6],  daa.hdata[7],
        daa.hdata[8],  daa.hdata[9],  daa.hdata[10], daa.hdata[11],
        daa.hdata[12], daa.hdata[13], daa.hdata[14], daa.hdata[15],
        daa.crc);

    while(ftell(fdi) < daa.size_offset) {
        myfr(fdi, &daa_type, 4);
        myfr(fdi, &len,  4);
#ifdef WORDS_BIGENDIAN
        daa_type = le32toh (daa_type);
        len      = le32toh (len);
#endif
        if (daa_type == 1) {
            multi = 1;  // multi volume, not needed but I want to be sure to catch it in any case
        } else if (daa_type == 3) {
            myfr(fdi, &pwdtype, 4);
            myfr(fdi, &pwdcrc, 4);
#ifdef WORDS_BIGENDIAN
            pwdtype = le32toh (pwdtype);
            pwdcrc  = le32toh (pwdcrc);
#endif
            myfr(fdi, daakey, 128);
            if (pwdtype != 0) {  // ???
                printf("- Alert: this type of encryption/password (%d) seems not supported\n", pwdtype);
            }
            printf("- the input file is protected by password, insert it: ");
            fgetz(ans, sizeof(ans), stdin);
            daa_crypt_init(pwdkey, ans, daakey);
            if (pwdcrc != crc32(0, pwdkey, 128)) {
                printf("\nError: wrong password\n");
                exit_error(0);
            }
        }
        if (fseek(fdi, len - 8, SEEK_CUR)) exit_error(1);
    }

    fseek(fdi, 0, SEEK_END);
    tot = ftell(fdi);
    if (multi || (tot != daa.daasize)) {
        printf("- multi volume file\n");
        if ((p = find_ext(filei, "001.daa"))) {
            multi    = 1;
            multinum = 2;   // the number of the next archive
        } else if ((p = find_ext(filei, "01.daa"))) {
            multi    = 2;
            multinum = 2;   // if the current is 1 the next is 2
        } else {
            multi    = 3;
            multinum = 0;
            p = strrchr(filei, '.');
            if (!p) p = filei + strlen(filei);
        }

        len = p - filei;
        multi_filename = malloc(len + 16);
        memcpy(multi_filename, filei, len);
        multi_filename[len] = 0;
    }

    tot  = 0;

    if (fseek(fdi, daa.size_offset, SEEK_SET)) exit_error(1);
    if (daa_dataz) { // solution never tested, just a blind conversion from 0045ed7c
        myalloc(&buffer_in, daa_dataz, &insz);
        myfr(fdi, buffer_in, daa_dataz);
        daas_mem = unzip (z, buffer_in, daa_dataz, (u8 *)daa_data, daas_mem);
        daas = (daas_mem << 3) / (bittype + bitsize);
        daas_mem = (((bitsize + bittype) * daas) + 7) >> 3;
    } else {
        myfr(fdi, daa_data, daas_mem);
    }
    if (daagbi == TYPE_GBI) gburner_lame((u8 *)daa_data, daas_mem, daa.crc & 0xff);
    if (dolame) poweriso_lame((u8 *)daa_data, daas_mem, daa.isosize);
    if (fseek(fdi, daa.data_offset, SEEK_SET)) exit_error(1);

    printf("- start unpacking:\n");
    outsz = daa.chunksize;
    buffer_out = malloc (outsz);

    last_chunk = daas - 1;

    for (i = 0; i < daas; i++)
    {
        printf("  %03d%%\r", (i * 100) / daas);

        if (daa.version == 0x100) {
            len = (daa_data[i].n1 << 16) | (daa_data[i].n2) | (daa_data[i].n3 << 8);
        } else {
            len   = daa2iso_read_bits(bitsize, (u8 *)daa_data, bitpos, dolamebits, 0);  bitpos += bitsize;
            len  += LZMA_PROPS_SIZE;
            ztype = daa2iso_read_bits(bittype, (u8 *)daa_data, bitpos, dolamebits, 1);  bitpos += bittype;
            if (len >= daa.chunksize) ztype = -1;
        }

        myalloc(&buffer_in, len, &insz);
        myfr(fdi, buffer_in, len);
        if (daa_type == 3) daa_crypt(buffer_in, len);

        switch(ztype)
        {
            case -1: // no compression (this ztype is used only in this tool)
                len = daa.chunksize;
                memcpy(buffer_out, buffer_in, len);
                break;
            case 0:  // LZMA
                len = unlzma(lzma, buffer_in, len, buffer_out, outsz, lzma_filter);
                break;
            case 1:  // INFLATE
                len = unzip (z,    buffer_in, len, buffer_out, outsz);
                break;
            default:
                printf("\nError: unknown compression type (%d)\n", ztype);
                exit_error(0);
                break;
        }
        if (i == last_chunk) {   // last chunk
            if ((tot + len) > daa.isosize) len = daa.isosize - tot;
        } else {                // other chunks
            if (len != daa.chunksize) {
                printf("\nError: the uncompressed size doesn't match the chunksize (%08x %08x)\n", len, daa.chunksize);
                exit_error(0);
            }
        }
        myfw(fdo, buffer_out, len);
        tot += len;
    }

    printf("  100%%\n"
        "- 0x%08x%08x bytes written\n", PRINTF64(tot));

    if (tot != daa.isosize) {
        printf("- Alert: the size of the ISO should be 0x%08x%08x bytes!\n", PRINTF64(daa.isosize));
    }

    if (lzma) {
        LzmaDec_Free(lzma, &g_Alloc);
    }
    if (z) inflateEnd (z);
    cleanup(); // free buffers

    printf("- finished\n");
    return 0;
}


unsigned int daa2iso_read_bits(unsigned int bits, unsigned char *in, unsigned int in_bits, unsigned int lame, int lame_increase)
{
    static const u8 powerisux[] = "\x0A\x35\x2D\x3F\x08\x33\x09\x15";
    static int      powerisuxn  = 0;
    unsigned int    seek_bits,
                    rem,
                    seek = 0,
                    ret  = 0,
                    mask = 0xffffffff;

    if (bits > 32) return(0);
    if (bits < 32) mask = (1 << bits) - 1;
    for (;;) {
        seek_bits = in_bits & 7;
        ret |= ((in[in_bits >> 3] >> seek_bits)) << seek;
        rem = 8 - seek_bits;
        if (rem >= bits) break;
        bits    -= rem;
        in_bits += rem;
        seek    += rem;
    }
    if (lame) {
        ret ^= ((powerisuxn ^ powerisux[powerisuxn & 7]) & 0xff) * 0x01010101;
        if (lame_increase) powerisuxn++;
    }
    return(ret & mask);
}


static void gburner_lame(u8 *data, int size, u8 crc8)
{
    int     i;
    u8      d;

    d = size >> 2;
    for (i = 0; i < size; i++) {
        data[i] -= crc8;
        data[i] ^= d;
    }
}


static void poweriso_lame(u8 *data, int size, u64 isosize)
{
    int     i;
    u8      a,
            c;

    isosize /= 0x800;
    a = (isosize >> 8) & 0xff;
    c = isosize & 0xff;
    for (i = 0; i < size; i++) {
        data[i] -= c;
        c += a;
    }
}


static char *find_ext (char *fname, char *ext)
{
    size_t len, extlen;
    char *ret;
    len    = strlen(fname);
    extlen = strlen(ext);
    ret    = fname + len - extlen;
    if ((len >= extlen) && !strcasecmp(ret, ext)) {
        return(ret);
    }
    return(NULL);
}


static FILE *daa_next(void)
{
    daa_t   daa;
    FILE    *fd;
    static char *toadd = NULL, *fmt;
    char * signature;

    if (!toadd) {
        toadd = multi_filename + strlen(multi_filename);
        switch(multi) {
            case 1:  fmt = "%03d.daa";  break;
            case 2:  fmt = "%02d.daa";  break;
            default: fmt = ".d%02d";    break;
        }
    }

    sprintf(toadd, fmt, multinum);
    printf("  open %s\n", multi_filename);
    fd = fopen(multi_filename, "rb");
    if (!fd) exit_error(1);

    myfr(fd, &daa, sizeof(daa));
    le2host_daa(&daa);
    signature = (char*) daa.sign;
    if (strncmp(signature, "DAA VOL", 16) && strncmp(signature, "GBI VOL", 16)) {
        printf("\nError: wrong DAA VOL signature (%.16s)\n", signature);
        exit_error(0);
    }
    if (fseek(fd, daa.size_offset, SEEK_SET)) exit_error(1);

    multinum++;
    return(fd);
}


static void myalloc(u8 **data, unsigned wantsize, unsigned *currsize)
{
    if (wantsize <= *currsize) return;
    *data = realloc(*data, wantsize);
    if (!*data) exit_error(1);
    *currsize = wantsize;
}


static void myfr(FILE *fd, void *data, unsigned size)
{
    int     len;

    len = fread(data, 1, size, fd);
    if (len == size) return;
    if (!multi) {
        printf("\nError: incomplete input file, can't read %u bytes\n", size);
        exit_error(0);
    }
    fclose(fd);
    fd = daa_next();
    myfr(fd, data + len, size - len);
}


static void myfw(FILE *fd, void *data, unsigned size)
{
    if (fwrite(data, 1, size, fd) == size) return;
    printf("\nError: problems during the writing of the output file, check your disk space\n");
    exit_error(0);
}


static int unlzma(CLzmaDec *lzma, u8 *in, u32 insz, u8 *out, u32 outsz, int filter)
{
#ifdef DAA_DEBUG
    printf ("LZMA (filter: %d)\n", filter);
#endif
    u32 x86State;
    ELzmaStatus status;
    SizeT   inlen,
            outlen;

    LzmaDec_Init(lzma);

    inlen  = insz;
    outlen = outsz;
    if (LzmaDec_DecodeToBuf(lzma, out, &outlen, in, &inlen, LZMA_FINISH_END, &status) != SZ_OK) {
        printf("\nError: the compressed LZMA input is wrong or incomplete (%d)\n", status);
        exit_error(0);
    }
    if (filter) {
        x86_Convert_Init (x86State);
        x86_Convert (buffer_out, outsz, 0, &x86State, 0);
    }

    return(outlen);
}


static u32 unzip (z_stream *z, u8 *in, u32 insz, u8 *out, u32 outsz)
{
    if (!z) return(0);
    inflateReset (z);
    z->next_in   = in;
    z->avail_in  = insz;
    z->next_out  = out;
    z->avail_out = outsz;
    if (inflate(z,Z_SYNC_FLUSH) != Z_STREAM_END) {
        printf("\nError: the compressed INFLATE input is wrong or incomplete\n");
        exit_error(0);
    }
    return(z->total_out);
}


static void le2host_daa(daa_t *daa)
{
#ifdef WORDS_BIGENDIAN
    daa->size_offset = le32toh (daa->size_offset);
    daa->version     = le32toh (daa->version);
    daa->data_offset = le32toh (daa->data_offset);
    daa->b1          = le32toh (daa->b1);
    daa->b0          = le32toh (daa->b0);
    daa->chunksize   = le32toh (daa->chunksize);
    daa->isosize     = le64toh (daa->isosize);
    daa->daasize     = le64toh (daa->daasize);
    daa->crc         = le32toh (daa->crc);
#endif
}


#ifdef HAVE_LOCAL_ZLIB
static u32 _crc32 (unsigned unused, u8 *data, u32 size)
{
    static const u32    crctable[] = {
        0x00000000, 0x77073096, 0xee0e612c, 0x990951ba,
        0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3,
        0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
        0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,
        0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de,
        0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
        0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec,
        0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5,
        0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
        0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
        0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940,
        0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
        0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116,
        0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f,
        0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
        0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,
        0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a,
        0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
        0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818,
        0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
        0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
        0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457,
        0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c,
        0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
        0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2,
        0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb,
        0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
        0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,
        0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086,
        0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
        0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4,
        0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad,
        0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
        0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683,
        0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8,
        0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
        0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe,
        0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7,
        0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
        0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
        0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252,
        0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
        0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60,
        0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79,
        0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
        0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f,
        0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04,
        0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
        0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a,
        0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,
        0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
        0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21,
        0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e,
        0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
        0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c,
        0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,
        0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
        0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db,
        0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0,
        0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
        0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6,
        0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf,
        0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
        0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d };
    u32     crc = 0xffffffff;
    u8      *limit;

    for (limit = data + size; data < limit; data++) {
        crc = crctable[*data ^ (crc & 0xff)] ^ (crc >> 8);
    }
    return(~crc);
}
#endif


static int fgetz(u8 *data, int size, FILE *fd)
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
    cleanup ();
    if (show_perror) {
        perror("\nError");
    }
    exit(1);
}
