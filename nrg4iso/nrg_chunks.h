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

#ifndef __NRG_CHUNKS_H
#define __NRG_CHUNKS_H

typedef char chunkname_t[4];

typedef struct 
{
    chunkname_t id;
    uint32_t size;
} nrg_chunkheader_t;


/* NERO chunk */
typedef struct 
{
    chunkname_t id;
    uint32_t offset;
} nrg_nero_t;


/* NER5 chunk */
typedef struct 
{
    chunkname_t id;
    uint64_t offset;
} nrg_ner5_t;

/* CUEX track data */
typedef struct
{
    uint8_t  ctrladr;
    uint8_t  track; // BCD encoded; 00 = Lead in, AA = Lead out
    uint8_t  index;
    uint8_t  recdep;
    int32_t  start; // Negative for lead in
} __attribute__((__packed__)) nrg_cuex_trk_t;


/* CUEX chunk "CUE sheet v2" */
typedef struct
{
    nrg_chunkheader_t hdr;
    nrg_cuex_trk_t trackdata[];
} __attribute__((__packed__)) nrg_cuex_t;


/* DAOI track data */
typedef struct
{
    char     isrc[12]; // International Standard Recording Code
    uint16_t sector_size;
    uint8_t  track_mode;
    uint8_t  track_config;
    uint8_t  first_index;
    uint8_t  last_index;
    uint32_t index0;
    uint32_t index1;
    uint32_t next_track;
} __attribute__((__packed__)) nrg_daoi_trk_t;


/* DAOI chunk "Disk at once information v1" */
typedef struct
{
    nrg_chunkheader_t hdr;
    uint32_t be_size;
    char     mcn[12]; // Media Catalog Number
    uint8_t  toc_type;
    uint8_t  unknown1;
    uint8_t  unknown2;
    uint8_t  unknown3;
    uint8_t  first_track;
    uint8_t  last_track;
    nrg_daoi_trk_t trackdata[];
} __attribute__((__packed__)) nrg_daoi_t;


/* DAOX track data */
typedef struct
{
    char     isrc[12]; // International Standard Recording Code
    uint16_t sector_size;
    uint8_t  track_mode;
    uint8_t  track_config;
    uint8_t  first_index;
    uint8_t  last_index;
    uint64_t index0;
    uint64_t index1;
    uint64_t next_track;
} __attribute__((__packed__)) nrg_daox_trk_t;

// 2048 = MODE1 Data 
// 2352 = MODE1 Data Raw
// 2336 = MODE2 Data 2336


/* DAOX chunk "Disk at once information v2" */
typedef struct
{
    nrg_chunkheader_t hdr;
    uint32_t be_size;
    char     mcn[12]; // Media Catalog Number
    uint8_t  toc_type; // 0x00 = CD ROM?, 0x20 = SVCD?, 0x40 = DVD-ROM ?
    uint8_t  unknown1; // Session closed?
    uint8_t  unknown2; // Disc fixated?
    uint8_t  unknown3; //
    uint8_t  first_track;
    uint8_t  last_track;
    nrg_daox_trk_t trackdata[];
} __attribute__((__packed__)) nrg_daox_t;


/* ETNF chunk "Extended track information v1" */
typedef struct
{
    nrg_chunkheader_t hdr;
    uint32_t offset;
    uint32_t length;
    uint32_t mode;
    uint32_t start_lsn;
    uint32_t unknown1;
} nrg_etnf_t;


/* ETN2 chunk "Extended track information v2" */
typedef struct
{
    nrg_chunkheader_t hdr;
    uint64_t offset;
    uint64_t length;
    uint32_t mode;
    uint32_t start_lsn;
    uint32_t unknown1;
} nrg_etn2_t;


/* MTYP chunk "Medium type" */
typedef struct
{
    nrg_chunkheader_t hdr;
    uint32_t type; // 0x1 = CD (old format), 0x6 = DVD-R/RW, 0x1c = DVD (Old format)
} nrg_mtyp_t;


/* SINF chunk "Session information" */
typedef struct
{
    nrg_chunkheader_t hdr;
    uint32_t tracks; 
} nrg_sinf_t;


/* ========= CDTEXT ==========
 
 ##  ID1 - CD-Text pack type

    7   6   5   4   3   2   1   0
  +---+---+---+---+---+---+---+---+
  | x | x | x | x | x | x | x | x |
  +---+---+---+---+---+---+---+---+
 
  (x)
  80h - Album title (ID2=00h) / Song name (ID2=01h...63h)
  81h - Performer name
  82h - Song writer name
  83h - Composer name
  84h - Arranger name
  85h - Message from artist / content provider
  86h - Disc ID
  87h - Search keyword / Genre
  88h - TOC
  89h - 2nd TOC
  8ah - Reserved
  8bh - Reserved
  8ch - Reserved
  8dh - Reserved for content provider
  8eh - UPC/EAN code of the album & ISRC code of each track
  8fh - Size information of the block
  Range: 0 - 255

  ### ID2 - Track number

    7   6   5   4   3   2   1   0
  +---+---+---+---+---+---+---+---+
  | 0 | x | x | x | x | x | x | x |
  +---+---+---+---+---+---+---+---+
 
  (x)
  Specifies the track this pack belongs to.  Track 0 specifies
  the album itself.
  Range: 0 - 99
 
  Bit7 is reserved.


  ### ID3 - Block sequential number

    7   6   5   4   3   2   1   0
  +---+---+---+---+---+---+---+---+
  | x | x | x | x | x | x | x | x |
  +---+---+---+---+---+---+---+---+
 
  (x)
  The sequential number of rhe pack is increased incrementally
  for each pack.
  Range: 0 - 255

  
  ### ID4 - Block number and character position

    7   6   5   4   3   2   1   0
  +---+---+---+---+---+---+---+---+
  | x | y | y | y | z | z | z | z |
  +---+---+---+---+---+---+---+---+
 
  (x)
  Double byte character code (DBCC) indicator.  If this bit is
  set the text data is in double byte character code.
 
  (y)
  Block number that this pack belongs to
 
  (z)
  Character position indicator specifies the number of characters
  in the text data that belongs to the previous pack.  The character
  position starts from 0 to 15.  A index of 15 indicates that the
  first character belongs to the one before the previous pack.
  If DBCC is set a set of 2 bytes in text data is counted as one.
*/

enum
{
    CDTX_ALBUM_TITLE = 0x80,
    CDTX_SONG_TITLE = 0x80,
    CDTX_PERFORMER_NAME,
    CDTX_SONGWRITER_NAME,
    CDTX_COMPOSER_NAME,
    CDTX_ARRANGER_NAME,
    CDTX_MESSAGE,
    CDTX_DISC_ID,
    CDTX_GENRE,
    CDTX_TOC,
    CDTX_TOC2,
    CDTX_UPC_EAN_ISRC = 0x8e,
    CDTX_BLOCK_INFO
};


typedef struct
{
    uint8_t  id1;     // Pack type
    uint8_t  id2;     // Track number
    uint8_t  id3;     // Block seq. number
    uint8_t  id4;     // Block number & character position
    char     text[12];
    uint16_t crc;     // 16bit CRC-CCITT
} __attribute__((__packed__)) cdtext_pack_t;


/* CDTX chunk "CD Text" */
typedef struct
{
    nrg_chunkheader_t hdr;
    cdtext_pack_t cdtext[];
} __attribute__((__packed__)) nrg_cdtx_t;


// ========= CDTEXT END ==========


#endif
