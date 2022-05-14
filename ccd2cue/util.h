/*
 Copyright (C) 2013, 2014, 2015 Bruno Félix Rezende Ribeiro <oitofelix@gnu.org>

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 3, or (at your option)
 any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef CCD2CUE_UTIL_H
#define CCD2CUE_UTIL_H

#define __(str) (str)
#define _(str) (str)

/**
 * \file       array.h
 * \brief      Array facilities
 */
/**
 * Concatenate strings
 *
 * \param[in] str,...  The strings to concatenate.
 *
 * \return
 * + Success: the result as a new malloc'ed string.
 * + Failure: a NULL pointer.
 *
 * \attention The last parameter must be a NULL pointer.
 *
 * \note: This function can fail only if it's impossible to malloc the
 * resulting concatenated string.
 *
 */

char * concat (const char *str, ...)
  __attribute__ ((warn_unused_result));

/**
 * Copy a null-terminated string into a newly allocated string.
 *
 * \param[in]  s  String;
 *
 * \return  A pointer to the newly allocated string that is a copy of
 *          the supplied string;
 *
 * This function returns a pointer to a newly allocated string, that
 * is a copy of the supplied one, or exits with failure if the string
 * could not be allocated.  It is just a wrapper around 'strdup'.
 *
 * \sa [Copying and Concatenation] (https://gnu.org/software/libc/manual/html_node/Copying-and-Concatenation.html#Copying-and-Concatenation)
 *
 */
char * xstrdup (const char *s);

/**
 * Remove trailing white space from a string.
 *
 * \param[in] str  The string
 *
 * \return A pointer to the new string.
 *
 * A pointer is returned because the string is reallocated to only
 * occupy the necessary space.  It can be the same pointer provided by
 * the caller, but it is not guaranteed.
 *
 */
char * array_remove_trailing_whitespace (char *str)
  __attribute__ ((nonnull, warn_unused_result));
  
  


/**
 * \file       cdt.h
 * \brief      _CD-Text_ format structure
 */

#include <stddef.h>
#include <stdint.h>

/**
 * _CD-Text_ entry type
 *
 * Each CD-Text entry has a type according to the following
 * enumeration.  The field that holds that type is cdt_data.type,
 * however it is of type uint8_t rather enum cdt_entry_type for
 * portability reasons.  So we can say that effectively it has type
 * ((uint8_t) enum cdt_entry_type).
 *
 * This enumeration is here just for reference and completeness,
 * because it is not used in any way in the conversion routines.  This
 * info is already available in the input _CCD sheet_ and is copied
 * verbatim to the _CD-Text_ output file.
 *
 */

enum cdt_entry_type
  {
    CDT_TITLE = 0x80,       /**< Album name and Track titles. */
    CDT_PERFORMER = 0x81,   /**< Singer/player/conductor/orchestra. */
    CDT_SONGWRITER = 0x82,  /**< Name of the songwriter. */
    CDT_COMPOSER = 0x83,    /**< Name of the composer. */
    CDT_ARRANGER = 0x84,    /**< Name of the arranger. */
    CDT_MESSAGE = 0x85,	    /**< Message from content provider or artist. */
    CDT_DISK_ID = 0x86,	    /**< Disk identification information. */
    CDT_GENRE = 0x87,	    /**< Genre identification / information. */
    CDT_TOC = 0x88,	    /**< TOC information. */
    CDT_TOC2 = 0x89,	    /**< Second TOC. */
    CDT_RES_8A = 0x8A,	    /**< Reserved 8A. */
    CDT_RES_8B = 0x8B,	    /**< Reserved 8B. */
    CDT_RES_8C = 0x8C,	    /**< Reserved 8C. */
    CDT_CLOSED_INFO = 0x8D, /**< For internal use by content provider. */
    CDT_ISRC = 0x8E,	    /**< UPC/EAN code of album and ISRC for tracks. */
    CDT_SIZE = 0x8F	    /**< Size information of the block. */
  };

/**
 * _CD-Text_ data
 *
 * This is the actual _CD-Text_ data structure.  It has 4 header bytes
 * and then 12 bytes of real data.  This structure together with its
 * _CRC_ compose the called _CD-Text_ Entry (cdt_entry).
 *
 * This structure describes an individual and complete "Entry" entry
 * inside a "CDText" section found in _CCD sheets_.  That entry is a
 * sequence of 16 bytes represented as ascii in hexadecimal notation.
 *
 */

struct cdt_data
{
  uint8_t type;	        /**< Entry type ((uint8_t) enum cdt_entry_type). */
  uint8_t track;	/**< Track number (0..99). */
  uint8_t sequence;	/**< Sequence number. */
  uint8_t block;        /**< Block number / Char position. */
  uint8_t text[12];	/**< _CD-Text_ data. */
};

/**
 * _CD-Text_ entry
 *
 * This structure describes an individual and complete entry as it is
 * put inside a _CDT file_.  It is just the _CD-Text_ data, possibly found
 * inside a _CCD sheet_, plus its CRC calculated by ::crc16.
 *
 */

struct cdt_entry
{
  struct cdt_data data;  /**< _CD-Text_ data. */
  uint8_t crc[2];        /**< CRC 16. */
};

/**
 * _CD-Text_ format structure
 *
 * This structure is just the whole collection of _CD-Text_ entries as
 * defined in ::cdt_entry.  It is usually obtained from _CCD sheet_ by
 * means of its correspondent structure and ::ccd2cdt function.  It
 * represents all _CD-Text_ data possibly present in a _CCD sheet_,
 * with its CRC 16 checksum already calculated, and ready to be put in
 * a _CDT file_ by ::cdt2stream.
 *
 */

struct cdt
{
  struct cdt_entry *entry; 	/**< Array of _CDT_ entries.  */
  size_t entries;		/**< Number of _CDT_ entries. */
};

/**
 * Convert _CDT structure_ into a _CDT file_ stream.
 *
 * \param[in]   cdt     Pointer to the _CDT structure_;
 * \param[out]  stream  Stream;
 *
 * \note This function exits if any writing error occurs.
 *
 * This function converts a _CDT structure_ containing all _CD-Text_
 * data and its checksum into a binary _CDT file_ stream.  It is
 * normally used to generate the file with extension ".cdt" that
 * accompany the _CUE sheet_ that reference it with a "CDTEXTFILE"
 * entry.  The _CDT structure_ that this function receives is usually
 * generated by ::ccd2cdt.
 *
 * The resulting stream has a terminating NULL character.  So, the
 * _CDT file_ size has 18 bytes per CDT entry --- 4 for header, 12 for
 * data and 2 for CRC --- and an additional NULL character.  Thus, if
 * ENTRIES is the number of _CDT_ entries, the following formula gives
 * the _CDT file_ size:
 *
 * + size = (4+12+2) * ENTRIES + 1
 *
 * This is the last step in the _CD-Text_ data conversion process.
 *
 * \sa
 * - Previous step:
 *   + ::ccd2cue
 *   + ::ccd2cdt
 * - Parallel step:
 *   + ::cue2stream
 *
 */

void cdt2stream (const struct cdt *cdt, FILE *stream)
  __attribute__ ((nonnull));




/**
 * \file       crc.h
 * \brief      Cyclic Redundancy Check
 */

/* Polynomials */
#define P16CCITT_N 0x1021 	/**< CRC-16-CCITT Normal */

/**
 * Calculate a negated 16 bit Cyclic Redundancy Check using a normal
 * CCITT polynomial.
 *
 * \param[in]  message  A pointer to the message.
 * \param[in]  length   The length of the message in bytes.
 *
 * \return Return the negated normal CRC-16-CCITT.
 *
 * \note This function never raises an error.
 *
 * This function computes the negated 16 bit CRC using the polynomial
 * P16CCITT_N (0x1021).
 *
 * This function is used to calculate the checksum for _CD-Text_
 * entries as required by the _CDT file_ format in the ::ccd2cdt
 * function.
 *
 * \sa cdt_entry.crc
 *
 */

uint16_t crc16 (const void *message, size_t length)
  __attribute__ ((nonnull, warn_unused_result, pure));



/**
 * \file       file.h
 * \brief      File and file name handling
 */
/**
 * \fn char * make_reference_name (const char *filename, const int dirname_flag)
 *
 * Make a reference name for the given file name.
 *
 * \param[in] filename File name used to derive the reference name.
 * \param[in] dirname_flag Whether to keep components names.
 *
 * \return
 * + Success: the reference name as a new malloc'ed string.
 * + Failure: a NULL pointer.
 *
 * \note This function can fail only if it's impossible to malloc the
 * resulting reference name string.
 *
 * ### Concept ###
 *
 * In this context, a file name's _reference name_ is a name used to
 * derive another names for correlate files.  For instance, if we have
 * the file 'qux/foo.bar' a correlate file name would be 'qux/foo.baz'
 * or even 'foo.baz', where the reference names are 'qux/foo' and
 * 'foo', respectively.
 *
 * So, basically, a _reference name_ is the original name without
 * extension, if it has one, regardless if one takes its full
 * component names or only the base name.
 *
 */

char * make_reference_name (const char *filename, const int dirname_flag);




/**
 * \file       io.h
 * \brief      Input/Output convenience functions
 */
/**
 * Optimize reading from and writing to STREAM with MODE.
 *
 * \param[in] stream  The stream;
 * \param[in] mode    One of _IOFBF, _IOLBF or _IONBF;
 *
 * \return
 * - =0  on success;
 * - <0  on failure;
 *
 * This function adjust buffer to the optimal block size for reading
 * from and writing to STREAM with MODE.
 *
 * The mode parameter accepted values has the following meanings:
 *
 *- _IOFBF: fully buffered;
 *- _IOLBF: line buffered;
 *- _IONBF: unbuffered;
 *
 * \sa [Controlling Which Kind of Buffering] (https://gnu.org/software/libc/manual/html_node/Controlling-Buffering.html#Controlling-Buffering)
 *
 **/

int io_optimize_stream_buffer (FILE *stream, int mode)
  __attribute__ ((nonnull));

/**
 * Write a data block to stream.
 *
 * \param[in]   data    Data array;
 * \param[in]   size    Object size in bytes;
 * \param[in]   count   Number of objects;
 * \param[out]  stream  Output stream;
 *
 * \return Return COUNT;
 *
 * This function writes up to COUNT objects of size SIZE from the
 * array DATA, to the stream STREAM.  The return value is always
 * COUNT.
 *
 * If the call fails for an obscure error like running out of space,
 * the function exits with error.  Therefore, this function only
 * returns with success.
 *
 * This function is just a wrapper around 'fwrite' that adds fatal
 * error handling.
 *
 * \sa [Block Input/Output] (https://gnu.org/software/libc/manual/html_node/Block-Input_002fOutput.html#Block-Input_002fOutput)
 *
 */

size_t xfwrite (const void *data, size_t size, size_t count, FILE *stream)
  __attribute__ ((nonnull));

/**
 * Write a character to stream.
 *
 * \param[in]   c       Character;
 * \param[out]  stream  Output stream;
 *
 * \return Return C;
 *
 * The 'fputc' function converts the character C to type 'unsigned
 * char', and writes it to the stream STREAM.  This function always
 * returns the character C.
 *
 * If the call fails for an obscure writing error, the function exits
 * with error.  Therefore, this function only returns with success.
 *
 * This function is just a wrapper around 'putc' macro that adds fatal
 * error handling.
 *
 * \sa [Simple Output] (https://gnu.org/software/libc/manual/html_node/Simple-Output.html#Simple-Output)
 *
 */

int xputc (int c, FILE *stream)
  __attribute__ ((nonnull));

/**
 * Print the optional arguments under the control of the template
 * string TEMPLATE to the stream STREAM.
 *
 * \param[out]  stream    Output stream;
 * \param[in]   template  Template string;
 * \param[in]   ...       Template arguments;
 *
 * \return Return the number of characters printed.
 *
 * If the call fails for an obscure writing error, the function exits
 * with error.  Therefore, this function only returns with success.
 *
 * This function is just a wrapper around 'vfprintf' function that is
 * meant to add fatal error handling to 'fprintf' function.
 *
 * \sa [Formatted Output Functions] (https://gnu.org/software/libc/manual/html_node/Formatted-Output-Functions.html#Formatted-Output-Functions)
 *
 */

int xfprintf (FILE *stream, const char *template, ...)
  __attribute__ ((nonnull));





/**
 * \file       memory.h
 * \brief      Memory management
 */
/**
 * Obstack chunk allocate function;
 *
 * This function is called to allocate chunks of memory into which
 * obstack objects are packed.
 *
 * \sa [Preparing for Using Obstacks] (https://gnu.org/software/libc/manual/html_node/Preparing-for-Obstacks.html#Preparing-for-Obstacks)
 *
 */

#define obstack_chunk_alloc xmalloc

/**
 * Obstack chunk free function;
 *
 * This function is called to return chunks of memory when the obstack
 * objects in them are freed.
 *
 * \sa [Preparing for Using Obstacks] (https://gnu.org/software/libc/manual/html_node/Preparing-for-Obstacks.html#Preparing-for-Obstacks)
 *
 */

#define obstack_chunk_free free

/**
 * Allocate memory.
 *
 * \param[in] size Block size in bytes;
 *
 * \return A pointer to the newly allocated block;
 *
 * This function returns a pointer to a newly allocated block SIZE
 * bytes long, or exits with failure if the block could not be
 * allocated.  It is just a wrapper around 'malloc'.
 *
 * \sa [Basic Memory Allocation] (https://gnu.org/software/libc/manual/html_node/Basic-Allocation.html#Basic-Allocation)
 *
 */

void * xmalloc (size_t size)
  __attribute__ ((malloc, alloc_size (1), warn_unused_result));

/**
 * Reallocate memory.
 *
 * \param[in] ptr Block address;
 * \param[in] newsize New size in bytes;
 *
 * \return The new address of the block;
 *
 * This function changes the size of the block whose address is PTR to
 * be NEWSIZE.  If it succeeds, the address of the newly allocated
 * block is returned.  If it is not possible to reallocate the memory
 * block this function exits with failure.  It is just a wrapper
 * around 'realloc'.
 *
 * \sa [Changing the Size of a Block] (https://gnu.org/software/libc/manual/html_node/Changing-Block-Size.html#Changing-Block-Size)
 *
 **/

void * xrealloc (void *ptr, size_t newsize)
  __attribute__ ((alloc_size (2), warn_unused_result));



#endif	/* CCD2CUE_UTIL_H */
