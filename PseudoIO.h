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

/* PseudoIO.h declares macros to mimic the standard library's stream
   input/output functions in order to redirect function calls to an
   alternative implementation that returns errors if allocations via
   Simon P. Bullen's fortified memory allocation shell fail. This
   allows stress testing.

Dependencies: ANSI C library.
Message tokens: None.
History:
  CJB: 01-Jan-15: Created.
  CJB: 13-Nov-16: Added interceptor versions of fputs and fprintf.
  CJB: 09-Dec-16: Added interceptor versions of fgetc and fputc.
*/

#ifndef PseudoIO_h
#define PseudoIO_h

/* ISO library headers */
#include <stdio.h>

#ifdef FORTIFY

#define fopen(filename, mode) \
          pseudo_fopen(filename, mode, __FILE__, __LINE__)

#define rewind(stream) \
          pseudo_rewind(stream, __FILE__, __LINE__)

#define fseek(stream, offset, whence) \
          pseudo_fseek(stream, offset, whence, __FILE__, __LINE__)

#define ftell(stream) \
          pseudo_ftell(stream, __FILE__, __LINE__)

#define fclose(stream) \
          pseudo_fclose(stream, __FILE__, __LINE__)

#define fwrite(ptr, size, nmemb, stream) \
          pseudo_fwrite(ptr, size, nmemb, stream, __FILE__, __LINE__)

#define fread(ptr, size, nmemb, stream) \
          pseudo_fread(ptr, size, nmemb, stream, __FILE__, __LINE__)

#define fputs(s, stream) \
          pseudo_fputs(s, stream, __FILE__, __LINE__)

#define puts(s) \
          pseudo_puts(s, __FILE__, __LINE__)

#define fprintf pseudo_fprintf

#define fgetc(stream) \
          pseudo_fgetc(stream, __FILE__, __LINE__)

#define fputc(c, stream) \
          pseudo_fputc(c, stream, __FILE__, __LINE__)

#endif

FILE *pseudo_fopen(const char *filename, const char *mode, const char *file, unsigned long line);

void pseudo_rewind(FILE *stream, const char *file, unsigned long line);

int pseudo_fseek(FILE *stream, long offset, int whence, const char *file, unsigned long line);

long pseudo_ftell(FILE *stream, const char *file, unsigned long line);

int pseudo_fclose(FILE *stream, const char *file, unsigned long line);

size_t pseudo_fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream, const char *file, unsigned long line);

size_t pseudo_fread(void *ptr, size_t size, size_t nmemb, FILE *stream, const char *file, unsigned long line);

int pseudo_fputs(const char *s, FILE *stream, const char *file, unsigned long line);

int pseudo_puts(const char *s, const char *file, unsigned long line);

int pseudo_fprintf(FILE *stream, const char *format, ...);

int pseudo_fgetc(FILE *stream, const char *file, unsigned long line);

int pseudo_fputc(int c, FILE *stream, const char *file, unsigned long line);

#endif
