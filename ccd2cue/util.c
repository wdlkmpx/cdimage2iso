/*
 util.c -- misc functions

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

#define _GNU_SOURCE

//#include <config.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>
#include <sysexits.h>
//#include <err.h>
//#include <errno.h>
#include <error.h>
#include <inttypes.h>

#include <sys/stat.h>

#include "util.h"
#include "errors.h"


/**
 * \file       array.c
 * \brief      Array facilities
 */

char * concat (const char *str, ...)
{
  va_list ap, ap2;
  size_t total = 1;		/* We have one fixed argument. */
  const char *s;
  char *result, *result_end;

  va_start (ap, str);

  /* You will need to scroll through the arguments twice: to determine
     the space needed and to copy each string.  So, backup the
     argument pointer. */
  __va_copy (ap2, ap);

  /* Determine how much space we need.  */
  for (s = str; s != NULL; s = va_arg (ap, const char *))
    total += strlen (s);

  /* We are done with the first argument pointer. */
  va_end (ap);

  /* Allocate the room needed and push an error, returning NULL, if it
     isn't possible. */
  result = malloc (total);
  if (result == NULL)
    error_push_lib (malloc, NULL, _("cannot concatenate strings"));

  /* Copy the strings.  */
  result_end = result;
  for (s = str; s != NULL; s = va_arg (ap2, const char *))
    result_end = stpcpy (result_end, s);

  /* We are done with the second and last argument pointer. */
  va_end (ap2);

  /* Return the intended string concatenation */
  return result;
}

char * xstrdup (const char *s)
{
  /* Assert the supplied string is valid. */
  assert (s != NULL);

  /* Try to copy the string to a new allocated space. */
  register char *str = strdup (s);

  /* If it is not possible exit with failure. */
  if (str == NULL)
    error (EX_OSERR, errno, _("%s: error copying string"), __func__);

  /* If allocation succeeded return the address to it. */
  return str;
}

char * array_remove_trailing_whitespace (char *str)
{
  /* This pointer will be place in the strings' end and will be
     decremented until there is no more whitespace.  */
  char *j;
  /* Assert that the string is valid. */
  assert (str != NULL);
  /* If the string is empty there is nothing to do, so just return. */
  if (str[0] == '\0') return str;
  /* Find the last non-null character of the string. */
  j = strchr (str, '\0') - 1;
  /* Find the first non-white space character backwards. */
  while (isspace (*j)) j--;
  /* Mark the strings' new end */
  *(j + 1) = '\0';
  /* Reallocate the string to the new size. */
  return realloc (str, j - str + 2);
}



/**
 * \file       cdt.c
 * \brief      _CD-Text_ format structure
 */

void cdt2stream (const struct cdt *cdt, FILE *stream)
{
  /* Assert the CDT structure is valid. */
  assert (cdt != NULL);
  /* Assert the stream is valid. */
  assert (stream != NULL);
  /* Write the _CD-Text_ structure to the stream. */
  xfwrite (cdt->entry, sizeof (*cdt->entry), cdt->entries, stream);
  /* Put the terminating NULL character. */
  xputc (0, stream);
}



/**
 * \file       crc.c
 * \brief      Cyclic Redundancy Check
 */

uint16_t crc16 (const void *message, size_t length)
{
  /* Assert the message pointer is valid. */
  assert (message != NULL);

  size_t i;			/* Offset inside the message;  */
  int j;			/* Bit offset inside current message's
				   byte; */
  uint16_t crc = 0;		/* CRC accumulator; */
  uint16_t polynomial = P16CCITT_N; /* CRC polynomial; */

  /* Process all bytes from message. */
  for (i = 0; i < length; i++)
    {
      crc ^= (uint16_t) *((uint8_t *) message + i) << 8;
      /* Process all bits from the current byte. */
      for (j = 0; j < 8; j++)
	crc = crc & (1 << 15) ? (crc << 1) ^ polynomial : crc << 1;
    }

  /* Return the negated CRC. */
  return ~crc;
}


/**
 * \file       errors.c
 * \brief      Error treatment
 */

/**
 * Error stack;
 *
 * This variable holds an array of error strings.  Each time
 * ::error_push_f is called, possibly by its higher level interface
 * macros, explicitly by ::error_push or ::error_push_lib, or implicitly
 * by ::error_pop, ::error_pop_lib, ::error_fatal_pop and
 * ::error_fatal_pop_lib, it pushes a error to this stack.
 *
 */

static char **error_stack = NULL;

/**
 * Error stack entries count;
 *
 * This variable holds an integer indicating the error stack's size in
 * error messages unity.
 *
 */

static size_t error_entries = 0;

void error_push_f (const char function_name[], const char *template, ...)
{
  va_list ap;
  char *str;
  va_start (ap, template);

  /* Allocate space on the error stack for the new error message. */
  error_stack = xrealloc (error_stack, sizeof (*error_stack) * ++error_entries);
  /* Store error message, with the supplied function name, on top of error
     stack. */
  if (vasprintf (&str, template, ap) < 0)
    error (EX_OSERR, errno, _("%s: cannot push error into stack"), __func__);
  if (asprintf (&error_stack[error_entries - 1], "%s: %s", function_name, str) < 0)
    error (EX_OSERR, errno, _("%s: cannot push error into stack"), __func__);
  free (str);

  va_end (ap);
}


void error_pop_f (void)
{
  size_t i;
  /* Print every error message from error stack in a bottom-top
     fashion.  Empty error stack. */
  for (i = 0; i < error_entries; i++)
    {
      error (0, 0, "%s", error_stack[i]);
      free (error_stack[i]);
    }

  free (error_stack);
  error_stack = NULL;
  error_entries = 0;
}



/**
 * \file       file.c
 * \brief      File and file name handling
 */

char * make_reference_name (const char *filename, const int dirname_flag)
{
  char *str, *str_end;

  /* Don't modify the original string 'FILENAME', generate your own
     copy of it in 'str'.  If 'dirname_flag' is true, conserve the
     components names, otherwise only take the base name. */
  if (dirname_flag) str = xstrdup (filename);
  else str = xstrdup (basename (filename));

  /* To make a reference name it's necessary to discard any possible
     extension from the file name.  If 'STR' has an extension,
     it's all trailing the last dot.  Try to find that last dot.*/
  str_end = strrchr (str, '.');

  /* If you have found the referred dot, 'STR' has an extension, thus
     free the space occupied by it and mark the new end of 'STR': the
     location of that last dot. */
  if (str_end != NULL)
    {
      str = realloc (str, str_end - str + 1);
      *str_end = '\0';
    }

  /* Return to the caller the wanted reference name */
  return str;
}



/**
 * \file       io.c
 * \brief      Input/Output convenience functions
 */

int io_optimize_stream_buffer (FILE *stream, int mode)
{
  /* Information about STREAM's attributes; */
  struct stat stat;
  /* STREAM's file descriptor number; */
  int fd;
  /* Assert that STREAM is not NULL. */
  assert (stream != NULL);
  /* Assert that MODE is valid. */
  assert (mode == _IOFBF || mode == _IOLBF || mode == _IONBF);
  /* Get the file descriptor associated with STREAM.  Return with
     failure if you cannot. */
  fd = fileno (stream);
  if (fd == -1) return -1;
  /* Get the information about STREAM's attributes. */
  if (fstat (fd, &stat) == -1) return -1;
  /* Adjust buffer to the optimal block size for reading from and
     writing to STREAM with MODE. */
  if (setvbuf (stream, NULL, mode, stat.st_blksize) != 0) return -1;
  /* Return success. */
  return 0;
}

size_t xfwrite (const void *data, size_t size, size_t count, FILE *stream)
{
  /* Assert DATA is a valid pointer. */
  assert (data != NULL);
  /* Assert STREAM is a valid pointer. */
  assert (stream != NULL);
  /* Write to stream, and if occurs an error exit with failure. */
  if (fwrite (data, size, count, stream) != count)
    error (EX_OSERR, errno, _("%s: error writing to stream"), __func__);
  /* Return the number of objects actually written. */
  return count;
}

int xputc (int c, FILE *stream)
{
  /* Assert STREAM is a valid pointer. */
  assert (stream != NULL);

  /* Write to stream, and if occurs an error exit with failure. */
  if (putc (c, stream) == EOF)
    error (EX_OSERR, errno, _("%s: error writing to stream"), __func__);

  /* Return the character just written. */
  return c;
}

int xfprintf (FILE *stream, const char *template, ...)
{
  va_list ap;			/* Argument pointer; */
  int retval;			/* Return value; */
  /* Assert STREAM is a valid pointer. */
  assert (stream != NULL);
  /* Assert template is a valid string. */
  assert (template != NULL);
  /* Initialize argument pointer. */
  va_start (ap, template);
  /* Print to stream. */
  retval = vfprintf (stream, template, ap);
  /* If occurred an output error exit with failure. */
  if (retval < 0)
    error (EX_OSERR, errno, _("%s: error writing to stream"), __func__);
  /* Return the number of characters just written. */
  return retval;
}




/**
 * \file       memory.c
 * \brief      Memory management
 */
/**
 * Obstack failed allocation handler
 *
 * This function prints an error and exits with failure when it is not
 * possible to allocate memory on obstack functions.
 *
 * \sa [Preparing for Using Obstacks] (https://gnu.org/software/libc/manual/html_node/Preparing-for-Obstacks.html#Preparing-for-Obstacks)
 *
 **/
static void memory_obstack_alloc_failed (void);

/**
 * Obstack failed allocation handler
 *
 * The Obstack failed allocation handler specified here gets called
 * when 'obstack_chunck_alloc' fails to allocate memory.
 *
 * \sa [Preparing for Using Obstacks] (https://gnu.org/software/libc/manual/html_node/Preparing-for-Obstacks.html#Preparing-for-Obstacks)
 *
 */

void (*obstack_alloc_failed_handler) (void) = memory_obstack_alloc_failed;


void * xmalloc (size_t size)
{
  /* Try to allocate SIZE memory bytes. */
  register void *value = malloc (size);

  /* If it is not possible exit with failure. */
  if (value == NULL)
    error (EX_OSERR, errno, _("%s: error allocating memory"), __func__);
  /* If allocation succeeded return the address to it. */
  return value;
}

void * xrealloc (void *ptr, size_t newsize)
{
  /* Try to reallocate data in PTR to a chunk SIZE bytes long. */
  register void *value = realloc (ptr, newsize);

  /* If it is not possible exit with failure. */
  if (value == NULL)
    error (EX_OSERR, errno, _("%s: error reallocating memory"), __func__);

  /* If reallocation succeeded return the address to it. */
  return value;
}

static void memory_obstack_alloc_failed (void)
{
  /* Print an error message and exit with failure. */
  error (EX_OSERR, errno, _("%s: error allocating chunk for obstack"), __func__);
}
