/*
 * CBDebugLib: Error injection veneer to standard library I/O functions
 * Copyright (C) 2015 Christopher Bazley
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* History:
  CJB: 01-Jan-15: Created this source file.
  CJB: 18-Apr-15: Assertions are now provided by debug.h.
  CJB: 13-Nov-16: Added interceptor versions of fputs and fprintf.
  CJB: 09-Dec-16: Added interceptor versions of fgetc and fputc.
  CJB: 13-Jun-20: Use new Fortify_AllowAllocate to avoid accumulating huge
                  numbers of 'freed' dummy memory allocations.
*/

#undef FORTIFY /* Prevent macro redirection of IO function calls to
                  pseudo_... functions within this source file. */

/* ISO library headers */
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>

/* Local headers */
#include "Debug.h"
#include "Internal/CBDebMisc.h"
#include "PseudoIO.h"
#include "LinkedList.h"

#include "fortify.h"

static bool io_succeeds(const char *file, unsigned long line)
{
  /* CJB's extra Fortify function to avoid accumulating
     huge numbers of 'freed' dummy memory allocations. */
  return Fortify_AllowAllocate(file, line);
}

FILE *pseudo_fopen(const char *filename, const char *mode, const char *file, unsigned long line)
{
  FILE *fh;
  assert(filename);
  assert(mode);
  if (io_succeeds(file, line))
  {
    fh = fopen(filename, mode);
  }
  else
  {
    errno = ERANGE;
    fh = NULL;
  }
  return fh;
}

void pseudo_rewind(FILE *stream, const char *file, unsigned long line)
{
  assert(stream);
  if (io_succeeds(file, line))
  {
    rewind(stream);
  }
  else
  {
    errno = ERANGE;
  }
}

int pseudo_fseek(FILE *stream, long offset, int whence, const char *file, unsigned long line)
{
  int err;
  assert(stream);
  assert(whence == SEEK_SET || whence == SEEK_CUR || whence == SEEK_END);
  if (io_succeeds(file, line))
  {
    err = fseek(stream, offset, whence);
  }
  else
  {
    errno = ERANGE;
    err = -1;
  }
  return err;
}

long pseudo_ftell(FILE *stream, const char *file, unsigned long line)
{
  long fpos;
  assert(stream);
  if (io_succeeds(file, line))
  {
    fpos = ftell(stream);
  }
  else
  {
    errno = ERANGE;
    fpos = -1;
  }
  return fpos;
}

int pseudo_fclose(FILE *stream, const char *file, unsigned long line)
{
  int err;
  assert(stream);
  /* Close the file even if simulating failure, to prevent leakage of
     file handles. */
  err = fclose(stream);
  if (!io_succeeds(file, line))
  {
    errno = ERANGE;
    err = EOF;
  }
  return err;
}

size_t pseudo_fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream, const char *file, unsigned long line)
{
  size_t nwritten;
  assert(ptr);
  assert(stream);
  if ((stream == stderr) || io_succeeds(file, line))
  {
    nwritten = fwrite(ptr, size, nmemb, stream);
  }
  else
  {
    stream->__flag |= _IOERR;
    errno = ERANGE;
    nwritten = 0;
  }
  return nwritten;
}

size_t pseudo_fread(void *ptr, size_t size, size_t nmemb, FILE *stream, const char *file, unsigned long line)
{
  size_t nread;
  assert(ptr);
  assert(stream);
  if (io_succeeds(file, line))
  {
    nread = fread(ptr, size, nmemb, stream);
  }
  else
  {
    stream->__flag |= _IOERR;
    errno = ERANGE;
    nread = 0;
  }
  return nread;
}

int pseudo_fputs(const char *s, FILE *stream, const char *file, unsigned long line)
{
  int err;
  assert(s);
  assert(stream);
  if ((stream == stderr) || io_succeeds(file, line))
  {
    err = fputs(s, stream);
  }
  else
  {
    stream->__flag |= _IOERR;
    errno = ERANGE;
    err = EOF;
  }
  return err;
}

int pseudo_puts(const char *s, const char *file, unsigned long line)
{
  int err;
  assert(s);
  if (io_succeeds(file, line))
  {
    err = puts(s);
  }
  else
  {
    errno = ERANGE;
    err = EOF;
  }
  return err;
}

int pseudo_fprintf(FILE *stream, const char *format, ...)
{
  int nchars;
  assert(stream);
  assert(format);

  if ((stream == stderr) || io_succeeds(__FILE__, __LINE__))
  {
    va_list arg;
    va_start(arg, format);
    nchars = vfprintf(stream, format, arg);
    va_end(arg);
  }
  else
  {
    stream->__flag |= _IOERR;
    errno = ERANGE;
    nchars = -1;
  }
  return nchars;
}

int pseudo_fgetc(FILE *stream, const char *file, unsigned long line)
{
  int c;
  assert(stream);
  if (io_succeeds(file, line))
  {
    c = fgetc(stream);
  }
  else
  {
    stream->__flag |= _IOERR;
    errno = ERANGE;
    c = EOF;
  }
  return c;
}

int pseudo_fputc(int c, FILE *stream, const char *file, unsigned long line)
{
  int err;
  assert(stream);
  if ((stream == stderr) || io_succeeds(file, line))
  {
    err = fputc(c, stream);
  }
  else
  {
    stream->__flag |= _IOERR;
    errno = ERANGE;
    err = EOF;
  }
  return err;
}
