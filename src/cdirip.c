/*
    CDIrip 0.6.3 

    Copyright 2004 DeXT/Lawrence Williams | Linux add-ons by Pedro Melo

    http://www.gnu.org/licenses/gpl-2.0.txt
*/

#include "main.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <inttypes.h>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#endif

#ifndef _WIN32
#define MAX_PATH PATH_MAX
#endif

// Global variables
static char *global_read_buffer_ptr;
static char *global_write_buffer_ptr;
static char *out_basename;

// Strings
static char STR_TDISC_CUE_FILENAME[]  = "%s-tdisc.cue";
static char STR_TDISCN_CUE_FILENAME[] = "%s-tdisc%d.cue";
static char STR_TAUDIO_RAW_FILENAME[] = "%s-taudio%02d.raw";
static char STR_TAUDIO_WAV_FILENAME[] = "%s-taudio%02d.wav";
static char STR_TAUDIO_CDA_FILENAME[] = "%s-taudio%02d.cda";
static char STR_TDATA_ISO_FILENAME[]  = "%s-tdata%02d.iso";
static char STR_TDATA_BIN_FILENAME[]  = "%s-tdata%02d.bin";

static void cleanup (void) {
    if (out_basename) free (out_basename);
    if (global_read_buffer_ptr)  free (global_read_buffer_ptr);
    if (global_write_buffer_ptr) free (global_write_buffer_ptr);
    out_basename = NULL;
    global_read_buffer_ptr  = NULL;
    global_write_buffer_ptr = NULL;
    cdimage2iso_cleanup ();
}

#define true  1
#define false 0

#define DEFAULT_FORMAT   0
#define ISO_FORMAT       1
#define BIN_FORMAT       2

#define WAV_FORMAT       0
#define RAW_FORMAT       1
#define CDA_FORMAT       2

#define SHOW_INTERVAL 2000

#define READ_BUF_SIZE  1024*1024
#define WRITE_BUF_SIZE 1024*1024

#define ERR_GENERIC   0L
#define ERR_OPENIMAGE 0x01
#define ERR_SAVETRACK 0x02
#define ERR_READIMAGE 0x03
#define ERR_SAVEIMAGE 0x04
#define ERR_PATH      0x05

//#define DEBUG_CDI /* For Debug only! */

typedef struct opts_s {
    char showinfo;
    char cutfirst; // TODO: delete?, no way to set this
    char cutall;
    char convert;
    char fulldata;
    char audio;
    char swap;
    char pregap;
} opts_s;

typedef struct flags_s {
    char do_cut;
    char do_convert;
    char create_cuesheet;
    char save_as_iso;
} flags_s;


// ============================================================================
// cdi.h / CDI file

static unsigned long CDI_temp_value;
#define CDI_V2  0x80000004
#define CDI_V3  0x80000005
#define CDI_V35 0x80000006

/* Basic structures */
typedef struct image_s
{
    long               header_offset;
    long               header_position;
    long               length;
    unsigned long      version;
    unsigned short int sessions;
    unsigned short int tracks;
    unsigned short int remaining_sessions;
    unsigned short int remaining_tracks;
    unsigned short int global_current_session;
} image_s;


typedef struct track_s
{
    unsigned short int global_current_track;
    unsigned short int number;
    long               position;
    unsigned long      mode;
    unsigned long      sector_size;
    unsigned long      sector_size_value;
    long               length;
    long               pregap_length;
    long               total_length;
    unsigned long      start_lba;
    unsigned char      filename_length;
} track_s;

static unsigned long CDI_ask_type (FILE *fsource, long header_position);
static void CDI_read_track (FILE *fsource, image_s *image, track_s *track);
static void CDI_skip_next_session (FILE *fsource, image_s *image);
static void CDI_get_tracks (FILE *fsource, image_s *image);
static void CDI_init (FILE *fsource, image_s *image, char *fsourcename);
static void CDI_get_sessions (FILE *fsource, image_s *image);


// ============================================================================
// misc utility functions
static void savetrack(FILE *fsource, image_s *, track_s *, opts_s *, flags_s *);
static void savecuesheet(FILE *fcuesheet, image_s *, track_s *, opts_s *, flags_s *);
static void show_counter(unsigned long i, long track_length, unsigned long image_length, long pos);
static void writewavheader (FILE *fdest, long track_length);
static void error_exit(long errcode, char *string);


// ============================================================================
// buffer.h / buffer_s

struct buffer_s
{
    FILE *file;
    char *ptr;
    long index;
    long size;
};
static int BufWrite (char *data, long data_size, struct buffer_s *buffer);
static int BufWriteFlush (struct buffer_s *buffer);
static int BufRead (char *data, long data_size, struct buffer_s *buffer, long filesize);


// ============================================================================
//int main (int argc, char *argv[])

int cdirip (struct file_info *infile, struct file_info *outfile)
{
    char *p = NULL;
    char cuesheetname[13], filename[MAX_PATH], destpath[MAX_PATH];
    FILE *fsource = NULL, *fcuesheet = NULL;

    image_s image = { 0, };
    track_s track = { 0, };
    opts_s  opts  = { 0, };
    flags_s flags = { 0, };

    image.global_current_session = 0;
    track.global_current_track   = 0;
    track.position               = 0;

    opts.audio = WAV_FORMAT;
    // cdrecord friendly defaults
    opts.cutall = true;
    opts.convert = ISO_FORMAT;

    for (p = program_options.flags; *p; p++)
    {
        switch (*p) {
            case 'b': opts.convert = BIN_FORMAT; break; /* ISO = default */
            case 'c': opts.audio = CDA_FORMAT; break;
            case 'r': opts.audio = RAW_FORMAT; break;
            case 'w': opts.audio = WAV_FORMAT; break;
            case 's': opts.swap     = true; break;
            case 'p': opts.pregap   = true; break;
            case 'f': opts.fulldata = true; break;
            case 'i': opts.showinfo = true; break;
            default:
                printf ("*** CDIrip warning: '%c' unknown flag\n", *p);
        }
    }

    strcpy(filename, infile->path);
    fsource = infile->stream;

    // currently this func only needs outfile->path
    out_basename = strdup (outfile->path);
    p = strrchr (out_basename, '.');
    if (p) *p = 0; // remove extension
    clear_fileinfo (outfile, 1);

    CDI_init (fsource, &image, filename);

    if (image.version == CDI_V2)
        printf("This is a v2.0 image\n");
    else if (image.version == CDI_V3)
        printf("This is a v3.0 image\n");
    else if (image.version == CDI_V35)
        printf("This is a v3.5 image\n");
    else
        error_exit(ERR_GENERIC, "Unsupported image version");

    // Sets proper audio format
    if (opts.audio == CDA_FORMAT)
    {
        opts.swap = opts.swap ? false : true; // invert swap switch for CDA (MSB by default)
    }

    // Start parsing image

    printf("\nAnalyzing image...\n");

    CDI_get_sessions (fsource, &image);

    if (image.sessions == 0) {
        error_exit(ERR_GENERIC, "Bad format: Could not find header");
    }

    printf("Found %d session(s)\n",image.sessions);

    // Allocating buffers

    global_read_buffer_ptr = (char *) malloc (READ_BUF_SIZE);
    global_write_buffer_ptr = (char *) malloc (WRITE_BUF_SIZE);

    if (global_read_buffer_ptr == NULL || global_write_buffer_ptr == NULL) {
        error_exit(ERR_GENERIC, "Not enough free memory for buffers!");
    }
    // Start ripping

    if (!opts.showinfo) printf("\nRipping image... (Press Ctrl-C at any time to exit)\n");

    if (opts.pregap) printf("Pregap data will be saved\n");

    image.remaining_sessions = image.sessions;

    /////////////////////////////////////////////////////////////// Bucle sessions

    while(image.remaining_sessions > 0)
    {
        image.global_current_session++;

        CDI_get_tracks (fsource, &image);

        image.header_position = ftell(fsource);

        printf("\nSession %d has %d track(s)\n",image.global_current_session,image.tracks);

        if (image.tracks == 0)
            printf("Open session\n");
        else
        {
            // Decidir si crear cuesheet
            if (!opts.showinfo)
            {
                if (image.global_current_session == 1)
                {
                    if (CDI_ask_type(fsource, image.header_position) == 2)
                    {
                        if (opts.convert == ISO_FORMAT) {
                            flags.create_cuesheet = false;  // "/iso" & Mode2 -> no cuesheet
                        } else {
                            flags.create_cuesheet = true;
                            opts.convert = BIN_FORMAT;     // Mode2 -> cuesheet + BIN
                        }
                    } else {
                        flags.create_cuesheet = true;      // Si Audio o Modo1
                    }
                }
                else
                {
                    if (opts.convert == BIN_FORMAT)      // ojo! tb depende de lo anterior
                        flags.create_cuesheet = true;
                    else
                        flags.create_cuesheet = false;
                }
            }
            else
            {
                flags.create_cuesheet = false;
            }

            // Create cuesheet
            if (flags.create_cuesheet)
            {
                printf("Creating cuesheet...\n");
                if (image.global_current_session == 1) {
                    sprintf(cuesheetname,STR_TDISC_CUE_FILENAME, out_basename);
                } else {
                    sprintf(cuesheetname,STR_TDISCN_CUE_FILENAME, out_basename, image.global_current_session);
                }
                fcuesheet = fopen(cuesheetname,"wb");
            }

            image.remaining_tracks = image.tracks;

            /////////////////////////////////////////////////////////////////
            // loop tracks

            while(image.remaining_tracks > 0)
            {
                track.global_current_track++;
                track.number = image.tracks - image.remaining_tracks + 1;

                CDI_read_track (fsource, &image, &track);

                image.header_position = ftell(fsource);

                // Show info
                if (!opts.showinfo) printf("Saving  ");
                printf("Track: %2d  ",track.global_current_track);
                printf("Type: ");
                switch(track.mode)
                {
                    case 0 : printf("Audio/"); break;
                    case 1 : printf("Mode1/"); break;
                    case 2 :
                    default: printf("Mode2/"); break;
                }
                printf("%d  ",track.sector_size);
                if (opts.pregap) {
                    printf("Pregap: %-3ld  ",track.pregap_length);
                }
                printf("Size: %-6ld  ",track.length);
                printf("LBA: %-6ld  ",track.start_lba);

                if (opts.showinfo) printf("\n");

                if (track.length < 0 && opts.pregap == false) {
                    error_exit(ERR_GENERIC, "Negative track size found\n"
                                        "You must extract image with /pregap option");
                }

                // Decidir si cortar
                if (!opts.fulldata && track.mode != 0 && image.global_current_session == 1 && image.sessions > 1) {
                    flags.do_cut = 2;
                } else if (!(track.mode != 0 && opts.fulldata)) {
                    flags.do_cut = ((opts.cutall) ? 2 : 0) +
                                ((opts.cutfirst && track.global_current_track == 1) ? 2 : 0);
                } else {
                    flags.do_cut = 0;
                }

                // Decidir si convertir
                if (track.mode != 0 && track.sector_size != 2048)
                {
                    switch (opts.convert)
                    {
                        case BIN_FORMAT: flags.do_convert = false; break;
                        case ISO_FORMAT: flags.do_convert = true; break;
                        case DEFAULT_FORMAT:
                        default:
                            if (track.mode == 1) {
                                flags.do_convert = true;          // Modo1/2352 -> ISO
                            } else {
                                if (image.global_current_session > 1) {
                                    flags.do_convert = true;       // Modo2 2ª sesion -> ISO
                                } else {
                                    flags.do_convert = false;      // Modo2 1ª sesion -> BIN (obsoleto)
                                }
                            }
                            break;
                    }
                }
                else
                {
                    flags.do_convert = false;
                }

                if (track.sector_size == 2048 || (track.mode != 0 && flags.do_convert)) {
                    flags.save_as_iso = true;
                } else {
                    flags.save_as_iso = false;
                }

                // Save track
                if (!opts.showinfo)
                {
                    if (track.total_length < track.length + track.pregap_length)
                    {
                        printf("\nThis track seems truncated. Skipping...\n");
                        fseek (fsource, track.position, SEEK_SET);
                        fseek (fsource, track.total_length, SEEK_CUR);
                        track.position = ftell(fsource);
                    }
                    else
                    {
                        savetrack(fsource, &image, &track, &opts, &flags);
                        track.position = ftell(fsource);

                        // Generate cuesheet entries
                        if (flags.create_cuesheet && !(track.mode == 2 && flags.do_convert))  // No generar entrada si convertimos (obsoleto)
                            savecuesheet(fcuesheet, &image, &track, &opts, &flags);
                    }
                }

                fseek (fsource, image.header_position, SEEK_SET);

                // Close loops
                image.remaining_tracks--;
            }

            if (flags.create_cuesheet) fclose(fcuesheet);
        }

        CDI_skip_next_session (fsource, &image);

        image.remaining_sessions--;
    }

    // Finalize
    printf("\nAll done!\n");

    if (!opts.showinfo) printf("Good burnin'...\n");

    cleanup();

    return 0;
}



// ============================================================================
// misc utility functions

static void savetrack(FILE *fsource, image_s *image, track_s *track, opts_s *opts, flags_s *flags)
{
    unsigned long i, ii;
    long track_length;
    unsigned long header_length;
    char tmp_val;
    int all_fine;
    char buffer[2352], filename [13];
    FILE *fdest;
    const char *cdi_name;

    struct buffer_s read_buffer;
    struct buffer_s write_buffer;

    fseek (fsource, track->position, SEEK_SET);

    if (track->mode == 0)
    {
        switch (opts->audio)
        {
            case RAW_FORMAT: cdi_name = STR_TAUDIO_RAW_FILENAME; break;
            case CDA_FORMAT: cdi_name = STR_TAUDIO_CDA_FILENAME; break;
            case WAV_FORMAT:
            default:         cdi_name = STR_TAUDIO_WAV_FILENAME; break;
        }
    }
    else
    {
        if (flags->save_as_iso) {
            cdi_name = STR_TDATA_ISO_FILENAME;
        } else {
            cdi_name = STR_TDATA_BIN_FILENAME;
        }
    }

    sprintf (filename, cdi_name, out_basename, track->global_current_track);
    fdest = fopen(filename,"wb");

    if (fdest == NULL) error_exit(ERR_SAVETRACK, filename);

    read_buffer.file = fsource;
    read_buffer.size = READ_BUF_SIZE;
    read_buffer.index = 0;
    read_buffer.ptr = global_read_buffer_ptr;
    write_buffer.file = fdest;
    write_buffer.size = WRITE_BUF_SIZE;
    write_buffer.index = 0;
    write_buffer.ptr = global_write_buffer_ptr;

    fseek (fsource, track->pregap_length*track->sector_size, SEEK_CUR);  // always skip pregap

    if (flags->do_cut != 0) printf("[cut: %d] ", flags->do_cut);

    track_length = track->length - flags->do_cut;     // para no modificar valor original

    if (opts->pregap && track->mode == 0 && image->remaining_tracks > 1)  // quick hack to save next track pregap (audio tracks only)
        track_length += track->pregap_length;                              // if this isn't last track in current session

    if (flags->do_convert)
        printf("[ISO]\n");
    else
        printf("\n");
/*
    if (track->mode == 2) {
        switch (track->sector_size) {
            case 2352: sector_type = MODE2_2352; break;
            case 2336: sector_type = MODE2_2336; break;
            case 2056: sector_type = MODE2_2056; break;
            case 2048: sector_type = MODE2_2048; break;
        }
     } else if (track->mode == 1) {
        switch (track->sector_size) {
            case 2352: sector_type = MODE1_2352; break;
            case 2048: sector_type = MODE1_2048; break;
        }
    } else if (track->mode == 0) {
        switch (track->sector_size) {
            case 2352: sector_type = MODE0_2352; break;
        }
    }
*/
    if (flags->do_convert)
    {
        if (track->mode == 2) {
            switch (track->sector_size)
            {
                case 2352: header_length = 24; break;
                case 2336: header_length =  8; break;
                default:   header_length =  0; break;
            }
        } else {
            switch (track->sector_size)
            {
                case 2352: header_length = 16; break;
                case 2048:
                default:   header_length =  0; break;
            }
        }
    }

    if (track->mode == 0) {
        switch (opts->audio)
        {
            case WAV_FORMAT: writewavheader(fdest, track_length); break;
            default: break;
        }
    }

    for(i = 0; i < track_length; i++)
    {
#ifndef DEBUG_CDI
        if (!(i%128)) show_counter (i, track_length, image->length, ftell(fsource));
#endif
        BufRead (buffer, track->sector_size, &read_buffer, image->length);

        if (track->mode == 0 && opts->swap)
        {
            for (ii = 0; ii < track->sector_size; ++ii, ++ii)
            {
                tmp_val = buffer[ii];
                buffer[ii] = buffer[ii+1];
                buffer[ii+1] = tmp_val;
            }
        }

        if (flags->do_convert) {
            all_fine = BufWrite (buffer + header_length, 2048, &write_buffer);
        } else {
            all_fine = BufWrite (buffer, track->sector_size, &write_buffer);
        }

        if (!all_fine) error_exit(ERR_SAVETRACK, filename);
    }

    printf("\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b"
            "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b"
            "                                                          "
            "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b"
            "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b");

    // Calcular posicion (BufRead no seguro, tb cuttrack & pregap)
    fseek (fsource, track->position, SEEK_SET);
    //fseek (fsource, track->pregap_length * track->sector_size, SEEK_CUR);
    //fseek (fsource, track->length * track->sector_size, SEEK_CUR);
    fseek (fsource, track->total_length * track->sector_size, SEEK_CUR);

    // Vaciar buffers
    BufWriteFlush(&write_buffer);
    fflush(fdest);
    fclose(fdest);
}


static void show_counter (unsigned long i, long track_length, unsigned long image_length, long pos)
{
    unsigned long progress, total_progress;

    progress = (i*100/track_length);
    total_progress = ((pos>>10)*100)/(image_length>>10);
    printf("\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b"
        "[Current: %02d%%  Total: %02d%%]  ",progress, total_progress);
}


static void savecuesheet(FILE *fcuesheet, image_s *image, track_s *track, opts_s *opts, flags_s *flags)
{
    char track_format_string[10];
    char audio_file_ext[5];

    if (opts->swap) {
        strcpy(track_format_string,"MOTOROLA");
    } else {
        strcpy(track_format_string,"BINARY");
    }
    switch (opts->audio)
    {
        case CDA_FORMAT: strcpy(audio_file_ext,"cda"); break;
        case RAW_FORMAT: strcpy(audio_file_ext,"raw"); break;
        case WAV_FORMAT:
        default:         strcpy(audio_file_ext,"wav"); break;
    }

    if (track->mode == 0)
    {
        if (opts->audio == WAV_FORMAT) {
              fprintf(fcuesheet, "FILE taudio%02d.wav WAVE\r\n"
                                 "  TRACK %02d AUDIO\r\n",
                                 track->global_current_track,
                                 track->number);
        } else {
              fprintf(fcuesheet, "FILE taudio%02d.%s %s\r\n"
                                 "  TRACK %02d AUDIO\r\n",
                                 track->global_current_track,
                                 audio_file_ext,
                                 track_format_string,
                                 track->number);
        }
        if (track->global_current_track > 1 && !opts->pregap && track->pregap_length > 0) {
              fprintf(fcuesheet, "    PREGAP 00:02:00\r\n");
        }
    }
    else
    {
        if (flags->save_as_iso) {
              fprintf(fcuesheet, "FILE tdata%02d.iso BINARY\r\n"
                                 "  TRACK %02d MODE%d/2048\r\n",
                                 track->global_current_track,
                                 track->number,
                                 track->mode);
        } else {
              fprintf(fcuesheet, "FILE tdata%02d.bin BINARY\r\n"
                                 "  TRACK %02d MODE%d/%d\r\n",
                                 track->global_current_track,
                                 track->number,
                                 track->mode,
                                 track->sector_size);
        }
    }

    fprintf(fcuesheet, "    INDEX 01 00:00:00\r\n");

    if (opts->pregap && track->mode != 0 && image->remaining_tracks > 1) {
        fprintf(fcuesheet, "  POSTGAP 00:02:00\r\n"); // instead of saving pregap
    }
}


static void writewavheader (FILE *fdest, long track_length)
{
    uint32_t wData_length   = track_length * 2352;
    uint32_t wTotal_length  = wData_length + 8 + 16 + 12;
    uint32_t wHeaderLength  = 16;
    uint16_t wFormat        = 1;
    uint16_t wChannels      = 2;
    uint32_t wSampleRate    = 44100;
    uint32_t wBitRate       = 176400;
    uint16_t wBlockAlign    = 4;
    uint16_t wBitsPerSample = 16;
    // write header
    fwrite ("RIFF", 4, 1, fdest);
    fwrite (&wTotal_length, 4, 1, fdest);
    fwrite ("WAVE", 4, 1, fdest);
    fwrite ("fmt ", 4, 1, fdest);
    fwrite (&wHeaderLength, 4, 1, fdest);
    fwrite (&wFormat, 2, 1, fdest);
    fwrite (&wChannels, 2, 1, fdest);
    fwrite (&wSampleRate, 4, 1, fdest);
    fwrite (&wBitRate, 4, 1, fdest);
    fwrite (&wBlockAlign, 2, 1, fdest);
    fwrite (&wBitsPerSample, 2, 1, fdest);
    fwrite ("data", 4, 1, fdest);
    fwrite (&wData_length, 4, 1, fdest);
}


static void error_exit(long errcode, char *string)
{
    char errmessage[256];
    if (errcode != ERR_GENERIC) {
        printf("\n%s: %s\n", string, strerror(errno));  // string is used as Filename
    }
    switch(errcode)
    {
        case ERR_OPENIMAGE: strcpy(errmessage, "File not found... typo?\n"); break;
        case ERR_SAVETRACK: strcpy(errmessage, "Could not save track\n"); break;
        case ERR_READIMAGE: strcpy(errmessage, "Error reading image\n"); break;
        case ERR_PATH: strcpy(errmessage, "Could not find destination path\n"); break;
        case ERR_GENERIC: // string is used as Error message
        default: strcpy(errmessage, string);
    }
    puts (errmessage);
}


// ============================================================================
// bufffer

static int BufWrite (char *data, long data_size, struct buffer_s *buffer)
{
    long write_length;
    if (data_size > (buffer->size + (buffer->size - buffer->index - 1))) {
        return 0;  // unimplemented
    }
    if (buffer->index + data_size < buffer->size)  { // 1 menos
        memcpy ((buffer->ptr + buffer->index), data, data_size);
        buffer->index += data_size;
    } else {
        write_length = buffer->size - buffer->index;
        memcpy ((buffer->ptr + buffer->index), data, write_length);
        fwrite (buffer->ptr, buffer->size, 1, buffer->file);
        memcpy (buffer->ptr, data + write_length, data_size - write_length);
        buffer->index = data_size - write_length;
    }
    return 1;
}


static int BufWriteFlush (struct buffer_s *buffer)
{
    fwrite (buffer->ptr, buffer->index, 1, buffer->file);
    buffer->index = 0;
    return 1;
}


static int BufRead (char *data, long data_size, struct buffer_s *buffer, long filesize)
{
    long read_length, max_length, pos;
    if (data_size > (buffer->size + (buffer->size - buffer->index - 1))) {
        return 0;  // unimplemented
    }
    if (filesize == 0)  { // no cuenta
        max_length = buffer->size;
    } else {
        pos = ftell(buffer->file);
        if (pos > filesize) max_length = 0;
        else max_length = ((pos + buffer->size) > filesize) ? (filesize - pos) : buffer->size;
    }
    if (buffer->index == 0) {
        fread (buffer->ptr, max_length, 1, buffer->file);
    }
    if (buffer->index + data_size <= buffer->size) {
        memcpy (data, buffer->ptr + buffer->index, data_size);
        buffer->index += data_size;
        if (buffer->index >= buffer->size) buffer->index = 0;
    } else {
        read_length = buffer->size - buffer->index;
        memcpy (data, buffer->ptr + buffer->index, read_length);
        fread (buffer->ptr, max_length, 1, buffer->file);
        memcpy (data + read_length, buffer->ptr, data_size - read_length);
        buffer->index = data_size - read_length;
    }
    return 1;
}



// ============================================================================
// CDI file

static unsigned long CDI_ask_type (FILE *fsource, long header_position)
{
    unsigned char filename_length;
    unsigned long track_mode;
    fseek (fsource, header_position, SEEK_SET);
    fread (&CDI_temp_value, 4, 1, fsource);
    if (CDI_temp_value != 0) {
       fseek (fsource, 8, SEEK_CUR); // extra data (DJ 3.00.780 and up)
    }
    fseek (fsource, 24, SEEK_CUR);
    fread (&filename_length, 1, 1, fsource);
    fseek (fsource, filename_length, SEEK_CUR);
    fseek (fsource, 19, SEEK_CUR);
    fread (&CDI_temp_value, 4, 1, fsource);
    if (CDI_temp_value == 0x80000000) {
        fseek (fsource, 8, SEEK_CUR); // DJ4
    }
    fseek (fsource, 16, SEEK_CUR);
    fread (&track_mode, 4, 1, fsource);
    fseek (fsource, header_position, SEEK_SET);
    return (track_mode);
}


static void CDI_read_track (FILE *fsource, image_s *image, track_s *track)
{
    char TRACK_START_MARK[10] = { 0, 0, 0x01, 0, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF };
    char current_start_mark[10];

    fread (&CDI_temp_value, 4, 1, fsource);
    if (CDI_temp_value != 0) {
        fseek (fsource, 8, SEEK_CUR); // extra data (DJ 3.00.780 and up)
    }
    fread (&current_start_mark, 10, 1, fsource);
    if (memcmp(TRACK_START_MARK, current_start_mark, 10)) {
        error_exit(ERR_GENERIC, "Unsupported format: Could not find the track start mark");
    }
    fread (&current_start_mark, 10, 1, fsource);
    if (memcmp(TRACK_START_MARK, current_start_mark, 10)) {
        error_exit(ERR_GENERIC, "Unsupported format: Could not find the track start mark");
    }
    fseek (fsource, 4, SEEK_CUR);
    fread (&track->filename_length, 1, 1, fsource);
    fseek (fsource, track->filename_length, SEEK_CUR);
    fseek (fsource, 11, SEEK_CUR);
    fseek (fsource, 4, SEEK_CUR);
    fseek (fsource, 4, SEEK_CUR);
    fread (&CDI_temp_value, 4, 1, fsource);
    if (CDI_temp_value == 0x80000000) {
        fseek (fsource, 8, SEEK_CUR); // DJ4
    }
    fseek (fsource, 2, SEEK_CUR);
    fread (&track->pregap_length, 4, 1, fsource);
    fread (&track->length, 4, 1, fsource);
    fseek (fsource, 6, SEEK_CUR);
    fread (&track->mode, 4, 1, fsource);
    fseek (fsource, 12, SEEK_CUR);
    fread (&track->start_lba, 4, 1, fsource);
    fread (&track->total_length, 4, 1, fsource);
    fseek (fsource, 16, SEEK_CUR);
    fread (&track->sector_size_value, 4, 1, fsource);

    switch(track->sector_size_value)
    {
        case 0 : track->sector_size = 2048; break;
        case 1 : track->sector_size = 2336; break;
        case 2 : track->sector_size = 2352; break;
        default: error_exit(ERR_GENERIC, "Unsupported sector size");
    }

    if (track->mode > 2) {
        error_exit(ERR_GENERIC, "Unsupported format: Track mode not supported");
    }

    fseek (fsource, 29, SEEK_CUR);
    if (image->version != CDI_V2)
    {
        fseek (fsource, 5, SEEK_CUR);
        fread (&CDI_temp_value, 4, 1, fsource);
        if (CDI_temp_value == 0xffffffff) {
            fseek (fsource, 78, SEEK_CUR); // extra data (DJ 3.00.780 and up)
        }
    }
}


static void CDI_skip_next_session (FILE *fsource, image_s *image)
{
    fseek (fsource, 4, SEEK_CUR);
    fseek (fsource, 8, SEEK_CUR);
    if (image->version != CDI_V2) fseek (fsource, 1, SEEK_CUR);
}


static void CDI_get_tracks (FILE *fsource, image_s *image)
{
    fread (&image->tracks, 2, 1, fsource);
}


static void CDI_init (FILE *fsource, image_s *image, char *fsourcename)
{
    fseek (fsource, 0L, SEEK_END);
    image->length = ftell(fsource);

    if (image->length < 8) error_exit(ERR_GENERIC, "Image file is too short");

    fseek (fsource, image->length-8, SEEK_SET);
    fread (&image->version, 4, 1, fsource);
    fread (&image->header_offset, 4, 1, fsource);

    if (errno != 0) error_exit(ERR_READIMAGE, fsourcename);
    if (image->header_offset == 0) error_exit(ERR_GENERIC, "Bad image format");
}


static void CDI_get_sessions (FILE *fsource, image_s *image)
{
#ifndef DEBUG_CDI
    if (image->version == CDI_V35) {
        fseek (fsource, (image->length - image->header_offset), SEEK_SET);
    } else {
        fseek (fsource, image->header_offset, SEEK_SET);
    }
#else
    fseek (fsource, 0L, SEEK_SET);
#endif
    fread (&image->sessions, 2, 1, fsource);
}

