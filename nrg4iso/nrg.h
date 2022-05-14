/*-
 * Copyright (c) 2007 Martin Akesson <makesson@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef __NRG_H
#define __NRG_H

#include "iso9660.h"
#include "globals.h"

typedef struct
{
    off_t offset;
    off_t length;
    int sectors;
    int mode;
    int sector_size;
} nrg_track_t;


typedef struct
{
    char* file_path;
    int file_handle;
    int img_version;
    int img_type;
    int img_sessions;
    int img_tracks;
    off_t hdr_offset;
    void* hdr_data;
} NRG_t;


enum NRG_Version { NRG_V1 = 0, NRG_V2, NRG_NUMV };
enum NRG_ImgType { NRG_DAO = 0, NRG_TAO };
enum NRG_ChunkType {
    NRG_ANYC = 0, NRG_NEROC = 0, NRG_CUESC,
    NRG_DAOIC, NRG_CDTX, NRG_ETNFC, NRG_SINFC,
    NRG_MTYPC, NRG_ENDC, NRG_NUMC
};

enum NRG_SectorComponents {
    NRG_SYNC = 1,
    NRG_HEAD = 2,
    NRG_SUBHDR = 4,
    NRG_DATA = 8,
    NRG_ECC = 16,
    NRG_EDC = 32,
    NRG_SPARE = 64,
    NRG_SUBCHN = 128,
    NRG_SUBQCHN = 256
};

// The number of sessions a disc can have is only limited to the
// ammount of space available.  nrg4iso enforces a limit of 64
// sessions for simplicity, this might change in the future.
// The track limit of 99 is an standards defined limit.
enum { NRG_MAXSESSIONS = 64, NRG_MAXTRACKS = 99 };

extern int nrg_errno;
extern char* nrg_imgversion[];
extern char* nrg_imgtypename[];

// Prototypes
char* nrg_strerror(const int a_errno);
int nrg_openimage(const char* a_path, NRG_t* a_nrg);
int nrg_seek2header(NRG_t* a_nrg);
int nrg_readchunk(NRG_t* a_nrg, int* a_type, void* a_chunk, int* a_size);

int nrg_readsector(NRG_t* a_nrg, int a_session, int a_track, int a_lsn, int a_flags, void* a_data, int* a_size);

#endif
