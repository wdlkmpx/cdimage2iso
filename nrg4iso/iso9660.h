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

#ifndef __ISO_H
#define __ISO_H

#include <stdint.h>

enum { LBA_PVD = 16 };
enum VolumeDescriptors { VDBoot= 0, VDPrimary  = 1, VDSupplementary  = 2, VDPartition  = 3, VDTermination  = 255 };

extern const char* ISO_STD_ID;

typedef struct
{
    uint8_t  VDType;
    char     VSStdId[5];
    uint8_t  Version;
    uint8_t  volumeFlags;
    char     systemId[32];
    char     volumeId[32];
    char     reserved2[8];
    uint32_t leVolumeSpaceSize;
    uint32_t beVolumeSpaceSize;
    char     escapeSequences[32];
    uint16_t leVolumeSetSize;
    uint16_t beVolumeSetSize;
    uint16_t leVolumeSetSequenceNumber;
    uint16_t beVolumeSetSequenceNumber;
    uint16_t leLogicalBlockSize;
    uint16_t beLogicalBlockSize;
    uint32_t lePathTableSize;
    uint32_t bePathTableSize;
    uint32_t lePathTable1;
    uint32_t lePathTable2;
    uint32_t bePathTable1;
    uint32_t bePathTable2;
    char     rootDirectoryRecord[34];
    char     volumeSetId[128];
    char     publisherId[128];
    char     dataPreparerId[128];
    char     applicationId[128];
    char     copyrightFileId[37];
    char     abstractFileId[37];
    char     bibliographicFileId[37];
    char     volumeCreation[17];
    char     volumeModification[17];
    char     volumeExpiration[17];
    char     volumeEffective[17];
    char     FileStructureStandardVersion;
    char     Reserved4;
    char     ApplicationUse[512];
    char     FutureStandardization[653];
} PVD_t;

int iso_verifypvd(PVD_t* a_pvd);
int iso_blocksize(PVD_t* a_pvd);
int iso_volumesize(PVD_t* a_pvd);

#endif
