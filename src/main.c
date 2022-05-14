//
// This file is put in the public domain
//

#include "main.h"

#include <stdlib.h>
#include <string.h>

struct app_options program_options;

struct toiso_data
{
    int input_format;
    char * input_ext;
    char * output_ext; // basically for special conversions
    int (*validate_input_func) (struct file_info*);
    int (*isofunc) (struct file_info*,struct file_info*);
    struct sig_info *sig;
};

// ===========================================================  

const char MAGIC_ISO9660[8] = { 0x01, 0x43, 0x44, 0x30, 0x30, 0x31, 0x01 , 0x00 };
const char MAGIC_CSO[4] = { 'C', 'I', 'S', 'O' };
const char MAGIC_DAX[4] = { 'D', 'A', 'X', 0 };
const char MAGIC_ISZ[4] = { 'I', 's', 'Z', '!' };

const char MAGIC_BWI[16] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x90, 0x00, 0xC1, 0x00, 0x12 };
const char MAGIC_B5I[16] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x90, 0x00, 0x48, 0x00, 0xD8 };
const char MAGIC_MDS[16] = { 'M', 'E', 'D', 'I', 'A', ' ', 'D', 'E', 'S', 'C', 'R', 'I', 'P', 'T', 'O', 'R' };

struct sig_info SIGNATURE_ISO = { // ISO9660 & UDF
    .offset    = 32768, // the beginning of the 16th sector (16*2048)
    .signature = MAGIC_ISO9660,
    .siglen    = sizeof(MAGIC_ISO9660),
    .desc      = "ISO - ISO9660/UDF",
};

struct sig_info SIGNATURE_CSO = {
    .offset    = 0,
    .signature = MAGIC_CSO,
    .siglen    = sizeof(MAGIC_CSO),
    .desc      = "CSO - PSP ZLIB Compressed ISO",
};

struct sig_info SIGNATURE_DAX = {
    .offset    = 0,
    .signature = MAGIC_DAX,
    .siglen    = sizeof(MAGIC_DAX),
    .desc      = "DAX - PSP ZLIB Compressed ISO",
};

struct sig_info SIGNATURE_ISZ = {
    .offset    = 0,
    .signature = MAGIC_ISZ,
    .siglen    = sizeof(MAGIC_ISZ),
    .desc      = "ISZ - Compressed ISO",
};

struct sig_info SIGNATURE_MDS = {
    .offset    = 0,
    .signature = MAGIC_MDS,
    .siglen    = sizeof(MAGIC_MDS),
    .desc      = "MDS - Alcohol 120% CD Image Descriptor",
};

// ===========================================================

struct toiso_data supported_formats[] =
{
    // in          in     out |check in  conv
    { FORMAT_BIN, ".bin", NULL,   NULL, bchunk , NULL },
    { FORMAT_B5I, ".b5i", NULL,   NULL, b5i2iso, NULL },
    { FORMAT_BWI, ".bwi", NULL,   NULL, b5i2iso, NULL },
    { FORMAT_CDI, ".cdi", NULL,   NULL, cdirip,  NULL },
    { FORMAT_CSO, ".cso", NULL,   NULL, cso2iso, &SIGNATURE_CSO },
    { FORMAT_ISO, ".iso", ".cso", NULL, cso2iso, &SIGNATURE_ISO },
    { FORMAT_DAA, ".daa", NULL,   NULL, daa2iso, NULL },
    { FORMAT_DAX, ".dax", NULL,   NULL, dax2iso, &SIGNATURE_DAX },
    { FORMAT_ISO, ".iso", ".dax", NULL, dax2iso, NULL },
    { FORMAT_DMG, ".dmg", NULL,   NULL, dmg2iso, NULL },
    { FORMAT_GBI, ".gbi", NULL,   NULL, daa2iso, NULL },
    { FORMAT_IMG, ".img", NULL,   NULL, ccd2iso, NULL },
    { FORMAT_ISZ, ".isz", NULL,   NULL, isz2iso, &SIGNATURE_ISZ },
    { FORMAT_MDF, ".mdf", NULL,   NULL, mdf2iso, NULL },
    { FORMAT_MDS, ".mds", NULL,   NULL, mdf2iso, &SIGNATURE_MDS },
    { FORMAT_NRG, ".nrg", NULL,   NULL, nrg2iso, NULL },
    { FORMAT_PDI, ".pdi", NULL,   NULL, pdi2iso, NULL },
    { FORMAT_UIF, ".uif", NULL,   NULL, uif2iso, NULL },
    { -1,         NULL,   NULL,   NULL, NULL   , NULL },
};

// ===========================================================

static int detect_by_signature (FILE *stream)
{
    int bufsize = 64 * 1024; // 64k
    char *buf;
    int i;
    int ret = 0; // unknonw format
    char *pos;
    struct sig_info *sig;

    buf = (char*) malloc (bufsize);
    fread (buf, sizeof(char), bufsize, stream);

    for (i = 0; supported_formats[i].isofunc; i++)
    {
        sig = supported_formats[i].sig;
        if (!sig) {
            // signature info not available
            continue;
        }
        if (!sig->signature || !sig->siglen) {
            // missing info, skip this entry
            continue;
        }

        pos = buf + sig->offset;
        //fseek (stream, sig->offset, SEEK_SET);
        //fread (buf, sizeof(char), sig->siglen, stream);

        if (memcmp(pos, sig->signature, sig->siglen) == 0) {
            printf ("Detected format: %s\n", sig->desc);
            ret = supported_formats[i].input_format;
            break;
        }
    }

    // reset position
    fseek (stream, 0, SEEK_SET);

    if (ret < 1) {
        //printf ("Could not detect file type\n");
        return -1;
    }

    free (buf);
    return ret;
}


// ===========================================================

static void show_version (void)
{
    puts (VERSION);
    exit (0);
}

static void show_help (char *program)
{
    char *p;
    p = strrchr (program, '\\');
    if (!p) p = strrchr (program, '/');
    if (p) {
        p++;
    } else {
        p = program;
    }
    printf ("%s v%s\n", p, VERSION);
    printf ("Convert disc image files to .ISO file\n\n");
    printf ("show_help: \n");
    printf ("    %s [options] <image> <out.iso>\n", p);
    printf ("\n");
    printf ("Options:\n");
    printf ("   -y               overwrite output files\n");
    printf ("   -flags <flags>   flags to pass to selected file handler\n");
    printf ("   --help           display this help message\n");
    printf ("   --version        display version number\n");
    printf ("\n");
    printf ("The following <image> extensions are supported:\n");
    printf (" .NRG (Nero disc image)\n");
    printf (" .IMG (CloneCD IMG file. CCD)\n");
    printf (" .CDI (DiscJuggler Disc Image)\n");
    printf ("    flags: r=raw|w=wav|c=cda (default_audio=wav)\n");
    printf ("    flags: s=swap b=bin+cue p=keep_pregap\n");
    printf (" .CSO (PlayStation Portable Compressed ISO)\n");
    printf (" .DAX (PlayStation Portable Compressed ISO)\n");
    printf (" .B5I/.BWI (BlindWrite Disc Image)\n");
    printf (" .PDI (Pinnacle InstantCopy Disc Image)\n");
    printf (" .MDF/.MDS (Alcohol 120%% Disc Image/Descriptor File)\n");
    printf (" .DAA (PowerISO Direct Access Archive)\n");
    printf (" .GBI (gBurner Image Format)\n");
    printf (" .DMG (Apple Disk Image File)\n");
    printf (" .UIF (MagicISO Disc Image)\n");
    printf ("   uif2iso selects the output ISO,CUE/BIN,MDS/MDS,CCD,NRG extension\n");
    exit(0);
}


static char * ensure_extension (char *path, char *ext)
{
    // returns a new string that must be freed (or NULL)
    size_t len;
    char *ret, *p;
    if (!path) {
        return NULL;
    }
    if (!ext || !*ext) {
        return strdup(path);
    }
    p = strrchr (path, '.');
    if (p && strcasecmp(p,ext) == 0) {
        return strdup(path);
    }
    len = strlen(path) + strlen(ext) + 5;
    ret = (char*) malloc (len);
    snprintf (ret, len-1, "%s%s", path, ext);
    return ret;
}

void clear_fileinfo (struct file_info *finfo, int remove_empty_file)
{
    //puts ("clear_fileinfo()");
    if (finfo->path && finfo->stream && remove_empty_file) {
        // check if output file is empty and remove empty file
        int i;
        fseek (finfo->stream, 0, SEEK_SET);
        i = fgetc (finfo->stream);
        if (i == EOF) {
            remove (finfo->path);
        }
    }
    if (finfo->stream) {
        fclose (finfo->stream);
    }
    if (finfo->path) {
        free (finfo->path);
    }
    memset (finfo, 0, sizeof(*finfo));
}


void cdimage2iso_cleanup (void)
{
    clear_fileinfo (program_options.infile,  0);
    clear_fileinfo (program_options.outfile, 1);
}

// ====================================================

int main (int argc, char **argv)
{
    //FILE *fdin, *fdout;
    int ret, i;
    char *inext;
    char *outext, *outtmp;
    int valid_output_extension = 0;
    int (*isofunc) (struct file_info*, struct file_info*);
    int (*validate_input) (struct file_info*);
    isofunc = NULL;
    validate_input = NULL;
    struct file_info infile =  {0};
    struct file_info outfile =  {0};
    isofunc = NULL;

    // Process options
    memset (&program_options, 0, sizeof(program_options));

    if (argc < 2) {
        show_help (argv[0]);
    }
    for (i = 1; i < argc; i++)
    {
        //puts (argv[i]);
        if (strcmp(argv[i], "--help") == 0) {
             show_help (argv[0]);
             return 0;
        }
        else if (strcmp(argv[i], "--version") == 0) {
            show_version();
            return 0;
        }
        else if ((strcmp(argv[i], "-y") == 0)
              || (strcmp(argv[i], "-f") == 0))
        {
            program_options.overwrite = 1;
        }
        else if (strcmp(argv[i], "-flags") == 0) {
            i++;
            if (i < argc) {
                snprintf (program_options.flags, sizeof(program_options.flags), "%s", argv[i]);
            }
        }
        else if (argv[i][0] == '-') {
            printf ("Unknown paramater: %s\n\n", argv[i]);
            printf ("Type %s --help\n", argv[0]);
            return -1;
        }
        else if (argv[i][0] != '-') {
            break;
        }
    }

    // Input and output file
    if (i == argc) {
        show_help (argv[0]);
        return -1;
    }
    else if (i == (argc-1)) {
        printf ("\nPlease specify the output[.iso] file\n");
        return -1;
    }
    else if (i < (argc-2)) {
        printf ("\nToo many parameters\n");
        return -1;
    }

    program_options.infile  = &infile;
    program_options.outfile = &outfile;

    infile.path  = strdup (argv[i]);
    outtmp = argv[i+1];

    // validate input (and output file) file extension
    inext  = strrchr (infile.path, '.');
    outext = strrchr (outtmp, '.');
    for (i = 0; supported_formats[i].input_ext; i++)
    {
        if (!inext) {
            // input file extension is required for now
            continue;
        }
        if (outext && supported_formats[i].output_ext) {
            // some special conversions are supported
            // both input and output file extensions may match a specific converter
            if (strcasecmp(inext,supported_formats[i].input_ext) == 0
                && strcasecmp(outext,supported_formats[i].output_ext) == 0)
            {
                isofunc = supported_formats[i].isofunc;
                valid_output_extension = 1;
                break;
            }
        } else {
            if (strcasecmp(inext,supported_formats[i].input_ext) == 0) {
                isofunc = supported_formats[i].isofunc;
                break;
            }
        }
    }

    if (isofunc == NULL) {
        printf ("\nInvalid input file extension: %s\n", inext ? inext : "");
        cdimage2iso_cleanup ();
        return -1;
    }

    // open input file
    infile.st = NULL;
    infile.stream = fopen (infile.path, "rb");
    if (!infile.stream) {
        perror ("ERROR");
        cdimage2iso_cleanup ();
        return -1;
    }

    // detect by signature
    infile.format = detect_by_signature (infile.stream);

    // output file
    outfile.st = NULL;
    if (valid_output_extension) {
        outfile.path = strdup (outtmp);
    } else {
        outfile.path = ensure_extension (outtmp, (char*)".iso");
    }
    outfile.stream = fopen (outfile.path, "rb");
    if (outfile.stream)
    {
        fclose (outfile.stream);
        if (program_options.overwrite) {
            remove (outfile.path);
        } else {
            printf ("%s already exists, use -y to overwrite it\n", outfile.path);
            cdimage2iso_cleanup ();
            return -1;
        }
    }
    outfile.stream = fopen (outfile.path,"wb+");
    if (!outfile.stream) {
        printf ("ERROR: can't open %s for writing\n", outfile.path);
        cdimage2iso_cleanup ();
        return -1;
    }

    // Convert to iso or some other format
    ret = isofunc (&infile, &outfile);

    // modules must call this before calling exit()
    cdimage2iso_cleanup ();

    return ret;
}
