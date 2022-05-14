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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <sys/stat.h>

#include "nrg.h"
#include "nrg_chunks.h"
#include "globals.h"
#include "w_endian.h"

// The Nero image header is located at the very end of the file and
// is a variation of the Interchange File Format (IFF).
// Header data is stored in named chunks with integer values in big
// endian byte order.
// The offset to the first chunk in the header is stored in the
// last chunk.  This chunk is referred to as the file header.
// The file header can be read from the last 8 or 12 bytes of the
// file depending on the version of the image.

chunkname_t nrg_chunknames[][NRG_NUMV] =
{
    { "NERO", "NER5" },
    { "CUES", "CUEX" },
    { "DAOI", "DAOX" },
    { "CDTX", "CDTX" },
    { "ETNF", "ETN2" },
    { "SINF", "SINF" },
    { "MTYP", "MTYP" },
    { "END!", "END!" }
};

typedef struct
{
    int sector_size;
    int data_size;
    char* name;
    int components;
} nrg_mode_t;

nrg_mode_t nrg_modes[] =
{
    /* 00 */ { 2048, 2048, "Mode1/2048", NRG_DATA },
    /* 01 */ { 0, 0, 0 },
    /* 02 */ { 0, 0, 0 },
    /* 03 */ { 2336, 2048, "Mode2/2336 (Form1)", NRG_SUBHDR | NRG_DATA | NRG_ECC | NRG_EDC },
    /* 04 */ { 0, 0, 0 },
    /* 05 */ { 0, 0, 0 },
    /* 06 */ { 0, 0, 0 },
    /* 07 */ { 2352, 2352, "CDDA/2352", NRG_DATA },
    /* 08 */ { 0, 0, 0 },
    /* 09 */ { 0, 0, 0 },
    /* 10 */ { 0, 0, 0 },
    /* 11 */ { 0, 0, 0 },
    /* 12 */ { 0, 0, 0 },
    /* 13 */ { 0, 0, 0 },
    /* 14 */ { 0, 0, 0 },
    /* 15 */ { 0, 0, 0 },
    /* 16 */ { 2448, 2352, "CDDA/2448", NRG_DATA | NRG_SUBCHN }
};

char* nrg_imgtypename[] =
{
    "DAO (Disc at once)",
    "TAO (Track at once)"
};


enum NRG_Error { NRG_ERROK, NRG_ERRVER, NRG_ERRIMG, NRG_ERRBRK, NRG_ERRNUM, NRG_ERRSYS = 0xf0000000 };
char* nrg_errortxt[] =
{
    "No error",
    "Unsupported image version",
    "Not a Nero image file",
    "Broken Nero image file"
};

int nrg_errno = 0;

enum { CHUNK_SIZE = 4096 }; // FIXME, will chunks ever be bigger than 4096 bytes?


static int _getchunktype(chunkname_t a_name, int* a_type)
{
    int ver;
    assert(a_type);

    for (*a_type = NRG_NEROC; *a_type < NRG_NUMC; (*a_type)++)
    {
        for (ver = NRG_V1; ver < NRG_NUMV; ver++)
        {
            if (memcmp(a_name, nrg_chunknames[*a_type][ver], 4) == 0)
            {
                return AOK;
            }
        }
    }
    return RET_ERROR;
}

static int _ischunktype(chunkname_t a_name, int a_type)
{
    int type;
    assert(a_type);

    _getchunktype(a_name, &type);
    return (type == a_type);
}

static int _nextchunk(nrg_chunkheader_t** a_chunk)
{
    char** ptr = (char**) a_chunk;

    assert(a_chunk && *a_chunk);

    if (_ischunktype((*a_chunk)->id, NRG_ENDC))
    {
        return RET_ERROR;
    }

    *ptr = *ptr + (*a_chunk)->size;

    return AOK;
}

static int _getnumsessions(NRG_t* a_nrg, int* a_sessions, int* a_tracks)
{
    nrg_chunkheader_t* hdr;

    assert(a_nrg && a_nrg->hdr_data);
    assert(a_sessions || a_tracks);

    // Initialize to zero
    if (a_sessions) *a_sessions = 0;
    if (a_tracks) *a_tracks = 0;

    hdr = (nrg_chunkheader_t*) a_nrg->hdr_data;

    // Counting the SINF chunks will tell us the number of sessions.
    // The chunk data contains the track count for each session.
    do
    {
        if (_ischunktype(hdr->id, NRG_SINFC))
        {
            nrg_sinf_t* sinf = (nrg_sinf_t*) hdr;
            if (a_sessions) *a_sessions++;
            if (a_tracks) *a_tracks += sinf->tracks;
        }
    } while (_nextchunk(&hdr));

    return (*a_sessions > 0);
}

int _getnumtracks(NRG_t* a_nrg, int a_session, int* a_tracks)
{
    off_t curr_offset;
    nrg_sinf_t sinf;
    int type = NRG_SINFC;
    int size = sizeof(nrg_sinf_t);

    assert(a_nrg);
    assert(a_tracks);

    // Save the current file offset to restore it before returning
    curr_offset = lseek(a_nrg->file_handle, 0, SEEK_CUR);
    nrg_seek2header(a_nrg);
    for (;a_session > 0; a_session--)
    {
        nrg_readchunk(a_nrg, &type, (void*) &sinf, &size);
    };
    *a_tracks = be32toh(sinf.tracks);

    // Restore the file pointer
    lseek(a_nrg->file_handle, curr_offset, SEEK_SET);
    return (a_tracks > 0);
}

/*
int _parse_daox(nrg_session_t* a_session, void* a_chunk)
{
    nrg_daox_t* daox = (nrg_daox_t*) a_chunk;
    nrg_track_t* tracks = a_session->tracks;
    int track;

    assert(a_session && tracks && daox);
    assert(_ischunktype(&daox->hdr.id, NRG_DAOIC));

    a_session->toc_type = daox->toc_type;
    if (a_session->num_tracks != (daox->last_track - (daox->first_track - 1)))
    {
        // FIXME, Most likely a damaged image file
    }

    for (track = 0; track < a_session->num_tracks; track++)
    {
        tracks[track].offset = be64toh(daox->trackdata[track].index1);
        tracks[track].length = be64toh(daox->trackdata[track].next_track) - tracks[track].offset;
        tracks[track].mode = daox->trackdata[track].track_mode;
        tracks[track].sector_size = be16toh(daox->trackdata[track].sector_size);
        tracks[track].sectors = tracks[track].length / tracks[track].sector_size;
        // TODO, verify sector count against track length and sector size
    }

    return AOK;
}

int _parse_etn2(nrg_session_t* a_session, void* a_chunk)
{
    nrg_track_t* tracks = a_session->tracks;

    assert(a_session && a_session->tracks);
    a_session->type = NRG_TAO;

    nrg_etn2_t* etn2 = (nrg_etn2_t*) a_chunk;
}
*/

int _initialize(NRG_t* a_nrg)
{
    // Chunk identifier should be 'NERO' for any image
    // made by pre 5.5 Nero, this is referred to as v1.
    // From 5.5 and on 'NER5' is used to identify the
    // new format referred to as v2.
    char data[sizeof(nrg_ner5_t)];
    nrg_nero_t* v1header = (nrg_nero_t*) &data[4];
    nrg_ner5_t* v2header = (nrg_ner5_t*) &data[0];
    struct stat filestat;

    assert(a_nrg);
    // Stat the file (for size)
    if (fstat(a_nrg->file_handle, &filestat))
    {
        nrg_errno = NRG_ERRSYS | errno;
        return RET_ERROR;
    }

    // Seek to the Nero header offset chunk
    if ((filestat.st_size - sizeof(data)) != lseek(a_nrg->file_handle, (filestat.st_size - sizeof(data)), SEEK_SET))
    {
        nrg_errno = NRG_ERRSYS | errno;
        return RET_ERROR;
    }

    // Read the last 12 bytes of the file into header struct
    read(a_nrg->file_handle, &data, sizeof(data));

    // Get chunk version
    if (memcmp(v2header->id, "NER5", 4) == 0) {
        printf ("v2 NER5 detected\n");
        a_nrg->img_version = NRG_V2;
    } else if (memcmp(v1header->id, "NERO", 4) == 0) {
        printf ("v1 NER0 detected\n");
        a_nrg->img_version = NRG_V1;
    } else {
        // Unsupported version or not a Nero image
        nrg_errno = NRG_ERRIMG;
        return RET_ERROR;
    }

    // Get the offset to first chunk in chain
    switch (a_nrg->img_version)
    {
        case NRG_V1:
            a_nrg->hdr_offset = be32toh(v1header->offset);
            break;
        case NRG_V2:
            a_nrg->hdr_offset = be64toh(v2header->offset);
            break;
    }
    a_nrg->hdr_offset = *(uint64_t*)(data+4);
printf ("header offset: %lld\n", *(uint64_t*)(data+4));
    // Make sure we got a sane offset
    if (a_nrg->hdr_offset > filestat.st_size)
    {
        // Broken image
        nrg_errno = NRG_ERRIMG;
        return RET_ERROR;
    }

    // Allocate memory to store the complete chain of chunks
    a_nrg->hdr_data = malloc(filestat.st_size - a_nrg->hdr_offset);
    if (NULL == a_nrg->hdr_data)
    {
        nrg_errno = NRG_ERRSYS | errno;
        return RET_ERROR;
    }
printf ("hdr_data: %ld\n", filestat.st_size - a_nrg->hdr_offset);

    // Read the chain of chunks into memory
    read(a_nrg->file_handle, a_nrg->hdr_data, (filestat.st_size - a_nrg->hdr_offset));

    nrg_errno = NRG_ERROK;
    return AOK;
}

/*
    if (a_nrg->img_sessions && a_nrg->img_tracks)
    {
        a_nrg->sessions = (nrg_session_t*) calloc(a_nrg->img_sessions, sizeof(nrg_session_t));
        if (!a_nrg->img_sessions)
        {
            nrg_errno = errno | NRG_ERRSYS;
            return RET_ERROR;
        }

        a_nrg->tracks = (nrg_track_t*) calloc(a_nrg->img_tracks, sizeof(nrg_track_t));
        if (!a_nrg->img_tracks)
        {
            nrg_errno = errno | NRG_ERRSYS;
            return RET_ERROR;
        }
    }

    tracks = a_nrg->tracks;
    nrg_seek2header(a_nrg);
    for(session = 0; session < a_nrg->img_sessions;)
    {
        int num_tracks;
        int size = CHUNK_SIZE;
        char chunk[size];
        int type;

        // Get the number of tracks for the session and
        // store in the session data struct.
        _getnumtracks(a_nrg, session + 1, &num_tracks);
        a_nrg->sessions[session].num_tracks = num_tracks;

        // Save the current track pointer in the session
        // data struct and advance pointer for next session.
        a_nrg->sessions[session].tracks = tracks;
        tracks += num_tracks;

        // The type of the first chunk tells us if this
        // is a disk- or track at once session.
        type = NRG_ANYC;
        if (!nrg_readchunk(a_nrg, &type, (void*) &chunk, &size))
        {
            switch (type)
            {
                case NRG_DAOIC:
                    // Disk at once
                    _parse_daox(&a_nrg->sessions[session], &chunk);
                    session++;
                    break;
                case NRG_ETNFC:
                    // Track at once
                    _parse_etn2(&a_nrg->sessions[session], &chunk);
                    session++;
                    break;
                default:
                    continue;
            }
        }
        else
        {
            // Failed reading a chunk
            return RET_ERROR;
        }
    }

    return AOK;
    }

    nrg_errno = NRG_ERRIMG;
    return RET_ERROR;
}
*/

void _destroy(NRG_t* a_nrg)
{
    /*
    char* file_path;
    int file_handle;
    nrg_session_t* sessions;
    nrg_track_t* tracks;
    */

    if (a_nrg->file_path) free(a_nrg->file_path);


    if (a_nrg->file_handle != RET_ERROR)
    {
        close(a_nrg->file_handle);
    }

    if (a_nrg->hdr_data) free(a_nrg->hdr_data);
}

char* nrg_strerror(const int a_errno)
{
    if ((a_errno >= NRG_ERROK) && (a_errno < NRG_ERRNUM))
    {
        return nrg_errortxt[a_errno];
    }
    if (a_errno & NRG_ERRSYS)
    {
        return strerror(a_errno & (0xffffffff ^ NRG_ERRSYS));
    }
    return NULL;
}


int nrg_seek2header(NRG_t* a_nrg)
{
    assert(a_nrg);

    nrg_errno = errno | NRG_ERRSYS;
    return (a_nrg->hdr_offset != lseek(a_nrg->file_handle, a_nrg->hdr_offset, SEEK_SET));
}


int nrg_readchunk(NRG_t* a_nrg, int* a_type, void* a_chunk, int* a_size)
{
    nrg_chunkheader_t localheader;
    nrg_chunkheader_t* hdr = &localheader;
    off_t curr_offset;

    assert(a_nrg && a_type && a_size);

    // If the caller supplied a buffer for chunk data use it
    // to store the chunk header instead of the local buffer.
    if (a_chunk && (*a_size >= sizeof(nrg_chunkheader_t)))
    {
	hdr = (nrg_chunkheader_t*) a_chunk;
    }

    // Save the current file offset so that it later can
    // be restored on a failed chunk read.
    curr_offset = lseek(a_nrg->file_handle, 0, SEEK_CUR);

    // Iterate the chunk chain until we find a matching chunk
    while (sizeof(nrg_chunkheader_t) == read(a_nrg->file_handle, (void*) hdr, sizeof(nrg_chunkheader_t)))
    {
        hdr->size = be32toh(hdr->size);
        if ((*a_type != NRG_ANYC) && (!_ischunktype(hdr->id, *a_type)))
        {
            // Not the chunk being searched for.  Skip file pointer
            // forward to point to the nect chunk in chain and repeat.
            curr_offset = lseek(a_nrg->file_handle, hdr->size, SEEK_CUR);
            continue;
        }

        // Set the chunk type for caller.
        _getchunktype(hdr->id, a_type);

        // If the supplied buffer size is smaller than
        // the chunk size we must fail the call.
        // The only exeption is when the supplied size
        // is set to zero in which case we return the
        // size required to satisfy the call.
        if ((*a_size < (sizeof(nrg_chunkheader_t) + hdr->size)) || !a_chunk)
        {
            // Reset the filepointer so that a subsequent
            // call will still address this chunk.
            lseek(a_nrg->file_handle, curr_offset, SEEK_SET);
            if (*a_size)
            {
                // Supplied buffer is too small
                return RET_ERROR;
            }
            // Return the required buffer size
            *a_size = hdr->size;
            return AOK;
        }
        else
        {
            char* chunk = (char*) a_chunk;
            if (hdr->size == read(a_nrg->file_handle, (void*) &chunk[sizeof(nrg_chunkheader_t)], hdr->size))
            {
                return AOK;
            }
        }
    }

    nrg_errno = errno | NRG_ERRSYS;
    return RET_ERROR;
}


int nrg_openimage(const char* a_path, NRG_t* a_nrg)
{
    assert(a_path && a_nrg);

    memset(a_nrg, 0, sizeof(NRG_t));
    a_nrg->file_handle = open(a_path, O_RDONLY, 0);

    if (RET_ERROR != a_nrg->file_handle)
    {
        a_nrg->file_path = strdup(a_path);
        if (AOK ==  _initialize(a_nrg))
        {
            // Get the total number of sessions and tracks in the image
            _getnumsessions(a_nrg, &a_nrg->img_sessions, &a_nrg->img_tracks);
            return AOK;
        }

        _destroy(a_nrg);
    }

    nrg_errno = errno | NRG_ERRSYS;
    return RET_ERROR;
}

int nrg_closeimage(NRG_t* a_nrg)
{
    assert(a_nrg);

    _destroy(a_nrg);

    return AOK;
}

int nrg_readsector(NRG_t* a_nrg, int a_session, int a_track, int a_lsn, int a_flags, void* a_data, int* a_size)
{
    assert(a_nrg);
    assert(a_data);
    assert(a_size);

    if ((a_session < 1) || (a_session > a_nrg->img_sessions))
    {
        // Invalid session number
        return RET_ERROR;
    }

    return RET_ERROR;
}
