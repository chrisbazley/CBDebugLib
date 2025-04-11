/*
 * CBDebugLib: Miscellaneous macro definitions
 * Copyright (C) 2018 Christopher Bazley
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
  CJB: 11-Apr-25: Dogfooding the _Optional qualifier.
*/

#ifndef CBDebMisc_h
#define CBDebMisc_h

#ifdef USE_OPTIONAL
#include <stdio.h>

#undef NULL
#define NULL ((_Optional void *)0)

static inline _Optional FILE *optional_fopen(const char *name, const char *mode)
{
    return fopen(name, mode);
}
#undef fopen
#define fopen(p, n) optional_fopen(p, n)

#else
#define _Optional
#endif

#define NOT_USED(x) ((void)(x))

#define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))

#define LOWEST(a, b) ((a) < (b) ? (a) : (b))

/* Copy a string into a character array of known size, truncating it to fit if
 * necessary. Unlike strncpy(), this macro ensures that the copied string is NUL
 * terminated if it has to be truncated.
 */
#define STRCPY_SAFE(string_1, string_2) do { \
  strncpy((string_1), (string_2), sizeof(string_1) - 1); \
  string_1[sizeof(string_1) - 1]='\0'; \
} while (0)

/* I believe that using error number 0 can have unpleasant side-effects */
enum
{
  DUMMY_ERRNO = 255
};

#endif /* CBDebMisc_h */
