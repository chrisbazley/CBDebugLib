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
  CJB: 25-Aug-26: Add a definition of CONTAINER_OF.
  CJB: 25-Aug-26: Include the header that declares offsetof, on which
                  CONTAINER_OF depends.
  CJB: 21-Sep-26: Make CONTAINER_OF assert that its pointer is non-null in
                  debug builds. In C23, require exact member pointer type,
                  including qualifiers; retain the portable compatibility
                  check for older ISO C versions.
*/

#ifndef CBDebMisc_h
#define CBDebMisc_h

#include <stddef.h>

#ifndef NDEBUG
#include <assert.h>
#endif

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

#ifndef CONTAINER_OF_CHECK
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
/* Require the pointer type implied by addr to exactly match that of member,
 * including the const and volatile qualification of the pointed-to type.
 */
#define CONTAINER_OF_CHECK(addr, type, member) \
  _Generic((&*(addr)), typeof(&((type *)0)->member): (void)0)
#else
/* Before C23, the conditional operator checks that the pointed-to types are
 * compatible, but its result type merges rather than compares qualifiers.
 */
#define CONTAINER_OF_CHECK(addr, type, member) \
  ((void)sizeof(0 ? &((type *)0)->member : (addr)))
#endif
#endif /* CONTAINER_OF_CHECK */

#ifndef CONTAINER_OF_ADDR
#ifndef NDEBUG
static inline const volatile char *cb_non_null(
  const volatile void *const addr)
{
  assert(addr != NULL);
  return addr;
}
#define CONTAINER_OF_ADDR(addr) cb_non_null(&*(addr))
#else
#define CONTAINER_OF_ADDR(addr) ((char *)&*(addr))
#endif
#endif /* CONTAINER_OF_ADDR */

#ifndef CONTAINER_OF
#define CONTAINER_OF(addr, type, member) \
  (CONTAINER_OF_CHECK(addr, type, member), \
   (type *)(CONTAINER_OF_ADDR(addr) - offsetof(type, member)))
#endif /* CONTAINER_OF */

#endif /* CBDebMisc_h */
