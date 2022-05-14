/*
    DMG2ISO is an Apple's compressed dmg to standard iso convert tool.
    This tool handles z-lib compressed dmg images.
 
    dmg2iso.c v0.2c

    Copyright (c) 2004 vu1tur <v@vu1tur.eu.org>
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License 
    as published by the Free Software Foundation.

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
#include <stdbool.h>

// ==============================================
// dmg2iso.h

#include "zlib.h"
#include <inttypes.h>

#define PLIST_LOOKUP_SIZE 0x200000
#define BT_ZLIB 0x80000005
#define BT_COPY 0x00000001
#define BT_ZERO 0x00000002
#define BT_END 0xffffffff

z_stream z;

const char plist_begin[]="<plist version=\"1.0\">";
const char plist_end[]="</plist>";
const char partlist_begin[]="<array>";
const char partlist_end[]="</array>";
const char part_begin[]="<data>";
const char part_end[]="</data>";

uint32_t convert_endian (uint32_t n) {
    return (n >> 24) | ((n & 0x00ff0000) >> 8) |
            ((n & 0x0000ff00) << 8) | (n << 24);
}


// ==============================================
// base64.c

static bool is_base64(const char c)
{
    if ((c >= 'A' && c <= 'Z') || 
        (c >= 'a' && c <= 'z') || 
        (c >= '0' && c <= '9') ||
        (c == '+') ||  (c == '/') ||  (c == '='))
    {
        return true;
    }
    return false;
}

static void cleanup_base64(char *inp, const unsigned int size)
{
    char *tinp1,*tinp2;
    tinp1 = inp;
    tinp2 = inp;
    for (unsigned int i = 0; i < size; i++)
    {
        if (is_base64(*tinp2)) {
            *tinp1++ = *tinp2++;
        } else {
            *tinp1 = *tinp2++;
        }
    }
    *(tinp1) = 0;
}

static unsigned char decode_base64_char(const char c)
{
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '=') return 0;
    return 63;   
}

static void decode_base64 (const char *inp, unsigned int isize, 
                           char *out, unsigned int *osize)
{
    char *tinp = (char*)inp; 
    char *tout;
    *osize = isize / 4 * 3;
    if (inp != out) {
        tout = (char*) malloc(*osize);
        out = tout;
    } else {
        tout = tinp;
    }
    for(unsigned int i = 0; i < isize >> 2; i++)
    {
        *tout++ = (decode_base64_char(*tinp++) << 2) | (decode_base64_char(*tinp) >> 4);
        *tout++ = (decode_base64_char(*tinp++) << 4) | (decode_base64_char(*tinp) >> 2);
        *tout++ = (decode_base64_char(*tinp++) << 6) | decode_base64_char(*tinp++);
    }
    if (*(tinp-1) == '=') (*osize)--;
    if (*(tinp-2) == '=') (*osize)--;
}


// ==============================================

int findstr(const char* inp,
            const char* str,
            const unsigned long ilen,
            const unsigned long slen,
            const unsigned int num)
{
    if (num > 0)
    {
        unsigned int tnum = num-1;
        int found = 0;
        char *tinp, *tstr;
        for (tinp = (char*)inp; tinp < inp+ilen-slen+1; tinp++)
        {
            for (tstr = (char*)str; tstr < str+slen; tstr++) {
                if (*tstr != tinp[tstr-str]) {
                    break;
                }
                if ((*tstr == tinp[tstr-str])&&(tstr-str == slen-1)) {
                    found = 1;
                }
            }
            if ((found == 1)&&(!tnum--)) {
                return tinp-inp;
            }
            found = 0;
        }
        return -1;
    }
    else
    {
        unsigned int tnum = 0;
        //int found = 0;
        char *tinp, *tstr;
        for (tinp = (char*)inp; tinp < inp+ilen-slen+1; tinp++) {
            for (tstr = (char*)str; tstr < str+slen; tstr++) {
                if (*tstr != tinp[tstr-str]) {
                    break;
                }
                if ((*tstr == tinp[tstr-str])&&(tstr-str == slen-1)) {
                    tnum++;
                }
            }
        }
        return tnum;
    }
}


//int main (int argc, char *argv[])
int dmg2iso (struct file_info *infile, struct file_info *outfile)
{
    FILE *FIN = infile->stream;
    FILE *FOUT = outfile->stream;

    fseek(FIN,-PLIST_LOOKUP_SIZE,2);
    //unsigned long ss = ftell(FIN);
    printf("reading property list...");
    char *ar;
    ar = (char*)malloc(PLIST_LOOKUP_SIZE);
    if (ar == NULL) { printf("ERROR: not enough memory\n");return 0; }
    fread((char*)ar,PLIST_LOOKUP_SIZE,1,FIN);

    if (findstr(ar,plist_begin,PLIST_LOOKUP_SIZE,strlen(plist_begin),0) == 0) {
        printf("ERROR: Property list is corrupted.\n");
        return 0;
    }
    printf("ok\n");
    unsigned int ploffs_begin = findstr(ar,partlist_begin,PLIST_LOOKUP_SIZE,strlen(partlist_begin),1);
    unsigned int pl_size = findstr(ar,partlist_end,PLIST_LOOKUP_SIZE,strlen(partlist_end),1)+8-ploffs_begin;
    char *plist = (char*)malloc(pl_size);
    //	fseek(FIN,ploffs_begin,0);
    memcpy(plist,ar+ploffs_begin,pl_size);
    //   fread(plist,1,ploffs_end-ploffs_begin,FIN);
    unsigned char partnum = findstr(plist,part_begin,pl_size,strlen(part_begin),0);
    printf("found %d partitions\n",partnum);
    char *parts[partnum];
    unsigned int partlen[partnum];
    for (int i = 0; i < partnum; i++)
    {
        unsigned int data_begin = findstr(plist,part_begin,pl_size,strlen(part_begin),i+1)+strlen(part_begin);
        unsigned int data_size = findstr(plist,part_end,pl_size,strlen(part_end),i+1)-data_begin;
        parts[i] = (char*)malloc(data_size);
        memcpy(parts[i],plist+data_begin,data_size);
        cleanup_base64(parts[i],data_size);
        //printf("partition %d: begin=%d, size=%d\n",i,data_begin,strlen(parts[i]));
        decode_base64(parts[i],strlen(parts[i]),parts[i],&partlen[i]);
        //printf("decoded, got %d bytes\n",partlen[i]);
        //fwrite(parts[i],partlen[i],1,FOUT);
    }
    z.zalloc = (alloc_func)0;
    z.zfree = (free_func)0;
    z.opaque = (voidpf)0;

    printf("decompressing:\n");
    Bytef* tmp = (Bytef*)malloc(0x40000);
    Bytef* otmp = (Bytef*)malloc(0x40000);
    uint32_t last_offs = 0;

    for (int i = 0; i < partnum; i++)
    {
        printf("partition %d...",i);
        unsigned int offset = 0xcc;
        //memcpy(&asdf,parts[i]+0xcc,4);
        uint32_t out_offs,out_size,in_offs,in_size,last_in_offs = 0;
        uint32_t block_type = 0;
        while (block_type != 0xffffffff)
        {
            memcpy(&block_type,parts[i]+offset,4);
            memcpy(&out_offs,parts[i]+offset+12,4);
            memcpy(&out_size,parts[i]+offset+20,4);
            memcpy(&in_offs,parts[i]+offset+28,4);
            memcpy(&in_size,parts[i]+offset+36,4);
            block_type = convert_endian(block_type);
            out_offs = convert_endian(out_offs)*0x200;
            out_size = convert_endian(out_size)*0x200;
            in_offs = convert_endian(in_offs);
            in_size = convert_endian(in_size);
            if (block_type == BT_ZLIB) {
                if (inflateInit(&z) != Z_OK) {
                    printf("ERROR: Can't initialize inflate stream\n");
                    return -1;
                }
                fseek(FIN,last_offs+in_offs,0);
                fread(tmp,in_size,1,FIN);
                z.next_in = (Bytef*)tmp;
                z.next_out = (Bytef*)otmp;
                while (1) {
                    z.avail_in = z.avail_out = 32768;
                    int err = inflate(&z, Z_NO_FLUSH);
                    if (err != Z_OK && err != Z_STREAM_END) {
                        printf("ERROR: Inflation failed\n");
                        return -1;
                    }
                    if (err == Z_STREAM_END) break;
                }
                if (inflateEnd(&z) != Z_OK) { 
                    printf("ERROR:\n");
                    return -1;
                }
                fwrite(otmp,out_size,1,FOUT);
                last_in_offs = in_offs+in_size;
            }
            else if (block_type == BT_COPY)
            {
                fseek(FIN,last_offs+in_offs,0);
                fread(tmp,in_size,1,FIN);
                fwrite(tmp,out_size,1,FOUT);//copy
            }
            else if (block_type == BT_ZERO)
            {
                char* zeroblock = (char*)malloc(out_size);
                memset(zeroblock,0,out_size);
                fwrite(zeroblock,out_size,1,FOUT);
                free(zeroblock);
            }
            else if (block_type == BT_END)
            {
                last_offs+=last_in_offs;
            }
            offset+=0x28;
            //if ((offset-0xcc)/0x28 % 64 == 0)
            //printf(".");
        }
        printf("ok\n");
    }

    printf("\nconversion successful\n");
    free(tmp);
    free(otmp);
    for (int i = 0; i < partnum; i++) free(parts[i]);

    free(plist);
    return 0;
}
