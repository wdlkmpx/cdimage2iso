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

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include <fcntl.h>
#include <string.h>
#include <libgen.h>
#include <errno.h>

#include "iso9660.h"
#include "nrg.h"
#include "globals.h"
#include "w_endian.h"

char* app_name = "nrg4iso";
int app_verhi = 1;
int app_verlo = 1;

int quietmode = false;
int nerosize = false;
int verbose = 0;

enum { BlockSize = 16384 };

void print(const char* a_format, ...)
{
    if (!quietmode) {
        va_list ap;
        va_start(ap, a_format);
        vprintf(a_format, ap);
        va_end(ap);
    }
}


int extend_data(int a_dst, int a_length)
{
    char dataBuff[BlockSize];
    void* voidBuff = (void*) &dataBuff;
    int bytes;
    int i = a_length;

    print("[1KEMExtending image..\n\n");

    // Clear the buffer
    memset(&dataBuff, 0, BlockSize);

    while  (i > 0)
    {
        if (i > BlockSize) {
            bytes = BlockSize;
        } else {
            bytes = i;
        }
        i -= bytes;

        if (RET_ERROR == write(a_dst, voidBuff, bytes)) {
            return RET_ERROR;
        }
    }

    return AOK;
}


int copy_data(int a_src, int a_dst, int a_length)
{
    int i = a_length;

    while (i > 0)
    {
        char dataBuff[BlockSize];
        void* voidBuff = (void*) &dataBuff;
        int bytes;

        print("[1KEMExtracting %i%%", (int ) (((float) (a_length - i) / (float) a_length) * 100));

        if (i > BlockSize) {
            bytes = BlockSize;
        } else {
            bytes = i;
        }

        i -= bytes;

        if (RET_ERROR == read(a_src, voidBuff, bytes)) {
            return RET_ERROR;
        }

        if (RET_ERROR == write(a_dst, voidBuff, bytes)) {
            return RET_ERROR;
        }
    }

    return AOK;
}

int main(int argc, char** argv)
{
    int argoffset = 1;
    char ch;

    NRG_t nrg_file;
    int isofile;

    while (-1 != (ch = getopt(argc, argv, "nqv")))
    {
        switch (ch)
        {
            case 'n':
                nerosize = true;
                break;
            case 'q':
                quietmode = true;
                break;
            case 'v':
                verbose++;
        }
        argoffset++;
    }

    if (verbose) printf("%s v%i.%i (%s)\n", app_name, app_verhi, app_verlo, __DATE__);

    // Check if we got all to input we need or print usage message
    if ((argc - argoffset) < 2)
    {
        printf("usage: %s [-qv] <nrg image> <iso image>\n", basename(argv[0]));
        exit(-1);
    }

    if (nrg_openimage(argv[argoffset], &nrg_file))
    {
        fprintf(stderr, "error: failed while reading source file (%s)\n", nrg_strerror(nrg_errno));
        exit(1);
    }

    argoffset++;
    isofile = open(argv[argoffset], O_WRONLY | O_CREAT | O_TRUNC, 0664);
    if (0 > isofile)
    {
        fprintf(stderr, "error: failed to create target file (%s)\n", strerror(errno));
        exit(1);
    }

    if (verbose) printf("[1mSource is a Nero %s image with %i session(s).[0m\n\n", nrg_imgtypename[nrg_file.img_type], nrg_file.img_sessions);

    // Disable buffering for stdout to enable use of progress bar
    setvbuf(stdout, NULL, _IONBF, 0);

    if (!nrg_seek2header(&nrg_file))
    {
        PVD_t iso_pvd;
        uint32_t iso_length;
        int session = 1;
        int track = 1;
        int size = sizeof(iso_pvd);

        // Read the PVD into a local buffer.
        nrg_readsector(&nrg_file, session, track, LBA_PVD, NRG_DATA, &iso_pvd, &size);

        // Verify that the PVD really is a valid PVD
        if (!iso_verifypvd(&iso_pvd))
        {
            // We have found a track with a valid ISO image inside.
            // Get the big endian size of the iso from the PVD
            iso_length = iso_volumesize(&iso_pvd) * iso_blocksize(&iso_pvd);
            print("Volume ID: %.32s\n", iso_pvd.volumeId);
            print("ISO size: %u MiB (%u B)\n",  iso_length >> 20, iso_length);

            /*
            if (iso_length != nrg_length)
            {
                print("NRG size: %llu MiB (%llu B)\n",  (nrg_length >> 20), nrg_length);
                print("Inconsistent image size in ISO and NRG headers. ");

                if (nerosize)
                {
                    copy_length = nrg_length;
                    print("NRG size will be used.\n");
                }
                else
                {
                    print("ISO size will be used.\n");
                    if (iso_length > nrg_length)
                    {
                        copy_length = nrg_length;
                        fill_length = (iso_length - nrg_length);
                    }
                }
            }
            */
            print("[1KEMFinished!\n\n");
        }
        else
        {
            printf("error... '%s'\n", nrg_strerror(nrg_errno));
        }
    }

    return 0;
}
