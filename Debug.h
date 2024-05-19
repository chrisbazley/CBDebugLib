/*
 * CBDebugLib: Output text with parameter substitution as a debugging aid
 * Copyright (C) 2005 Christopher Bazley
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

/* Debug.h declares macros to allow debugging statements to be inserted into a
   program without binding it to a particular output system or introducing
   unwanted link-time dependencies in release builds.

Dependencies: ANSI C library & Acorn library kernel.
History:
  CJB: 15-Jan-05: Moved out from SFeditor sources.
  CJB: 21-Jan-05: Updated documentation on dprintf as appropriate.
  CJB: 05-Mar-05: Inclusion of debugging code is now predicated only upon the
                  relevant USE_... macro, not NDEBUG also.
  CJB: 16-May-05: Added missing '#define Debug_h' to prevent multiple inclusion.
  CJB: 13-Oct-06: Qualified returned _kernel_oserror pointers as 'const'.
  CJB: 02-May-08: If USE_LOGFILE has been defined then debugging output will
                  now be directed to a log file in the Wimp scrap directory.
  CJB: 29-Aug-09: Replaced dlogf and dprintf with a new function debug_printfl
                  which handles all output methods. Added prototypes of other
                  new functions: debug_[v]printf allows lines of output to be
                  built up piecemeal, and debug_set_output configures the output
                  method. Macros avoid link-time dependencies on the new
                  functions in release builds. DEBUG is aliased to DEBUGFL to
                  allow existing code to be recompiled without modification.
  CJB: 05-Oct-09: Renamed the enumerated type 'Debug_Output' and its values.
  CJB: 26-Jun-10: Made definition of deprecated type and constant names
                  conditional upon definition of CBLIB_OBSOLETE.
  CJB: 07-Jan-11: Added a set of DEBUG_VERBOSE... macros which behave like
                  their DEBUG... counterparts but produce no output unless
                  DEBUG_VERBOSE_OUTPUT is defined.
  CJB: 18-Apr-15: Added alternative definitions of the standard C library's
                  assert macro to ensure that the reason for any assertion
                  failure is properly logged.
  CJB: 09-Apr-16: Used CHECK_PRINTF to check the parameters passed to the
                  debugging output functions.
  CJB: 29-Dec-19: Changed a definition of the assert macro to declare the
                  stringified expression as a separate string to prevent a
                  percent sign being misinterpreted as a format specifier.
  CJB: 11-Dec-20: Removed redundant uses of the 'extern' keyword.
  CJB: 12-Mar-23: Extended debug_set_output to return the previous mode.
  CJB: 01-Jun-23: Added a definition of the LOCATION macro.
                  Do-nothing macros are now defined as do {} while(0)
                  instead of if (0)... so their arguments needn't be valid
                  and to prevent unintended association with 'else' blocks.
*/

#ifndef Debug_h
#define Debug_h

/* ISO library headers */
#include <stdarg.h>
#include <stdlib.h>
#include <assert.h>

/* Convert a built-in  (e.g. __LINE__) to a string literal.
 */
#ifndef STRINGIFY
#define STRINGIFY2(n) #n
#define STRINGIFY(n) STRINGIFY2(n)
#endif

/* String representing the current source code location
 */
#ifndef LOCATION
#define LOCATION __FILE__ ":" STRINGIFY(__LINE__)
#endif

/* Check the arguments passed to a printf-like function.
 * string_index is the index of the format string argument (starting from 1).
 * arg_index is the index of the substitution arguments to be checked.
 * Set arg_index to 0 if the substitution arguments cannot be checked at
 * compile-time (e.g. because they are supplied as a va_list).
 */
#ifndef CHECK_PRINTF
#ifdef __GNUC__
#define CHECK_PRINTF(string_index, arg_index) \
  __attribute__((format(printf, (string_index), (arg_index))))
#else
#define CHECK_PRINTF(string_index, arg_index)
#endif
#endif

/* The standard C library's implementation of assert() doesn't print
   anything useful upon assertion failure in Wimp tasks. */
#undef assert
#ifdef NDEBUG
#define assert(ignore) ((void)0)
#else
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901)
#define assert(e) \
do \
{ \
  if (!(e)) \
  { \
    char const *const s = #e; \
    DEBUG("Assertion %s failed in function %s at " LOCATION, s, __func__); \
    (void)s; \
    abort(); \
  } \
} \
while(0)
#else
#define assert(e) \
do \
{ \
  if (!(e)) \
  { \
    char const *const s = #e; \
    DEBUG("Assertion %s failed at " LOCATION, s); \
    (void)s; \
    abort(); \
  } \
} \
while(0)
#endif /* (__STDC_VERSION__ >= 199901) */
#endif /* NDEBUG */

typedef enum
{
  DebugOutput_None = 0,     /* No debugging output */
  DebugOutput_StdOut,       /* To standard output stream
                               (VDU, unless redirected) */
  DebugOutput_StdErr,       /* To standard error stream
                               (VDU, unless redirected) */
  DebugOutput_File,         /* Append to a file in <Wimp$ScrapDir>
                               (buffered, fast but may lose data in crash) */
  DebugOutput_FlushedFile,  /* Append to a file in <Wimp$ScrapDir>
                               (unbuffered, slow but more secure) */
#ifdef ACORN_C
  DebugOutput_SplitStdOut,  /* Standard output stream
                               (first splitting text and graphics cursors) */
  DebugOutput_Reporter,     /* To Martin Avison's Reporter module */
  DebugOutput_SysLog,       /* To Doggysoft/Gerph's SysLog module */
  DebugOutput_SessionLog,   /* To Doggysoft/Gerph's SysLog module
                               (session log to group output from this task) */
#endif
  DebugOutput_LAST
}
DebugOutput;

#ifdef DEBUG_OUTPUT

#define DEBUGFL(...) debug_printfl(__VA_ARGS__)
#define DEBUGF(...) debug_printf(__VA_ARGS__)
#define DEBUGVF(...) debug_vprintf(__VA_ARGS__)
#define DEBUG_SET_OUTPUT(output_mode, log_name) debug_set_output(output_mode, log_name)

#else /* DEBUG_OUTPUT */

#define DEBUGFL(...) do {} while(0)
#define DEBUGF(...) do {} while(0)
#define DEBUGVF(...) do {} while(0)

static inline DebugOutput debug_set_output(DebugOutput output_mode, const char *log_name)
{
  (void)output_mode;
  (void)log_name;
  return DebugOutput_None;
}
#define DEBUG_SET_OUTPUT(output_mode, log_name) debug_set_output(output_mode, log_name)

#endif /* DEBUG_OUTPUT */

#if defined(DEBUG_VERBOSE_OUTPUT) && defined (DEBUG_OUTPUT)

#define DEBUG_VERBOSEFL(...) debug_printfl(__VA_ARGS__)
#define DEBUG_VERBOSEF(...) debug_printf(__VA_ARGS__)
#define DEBUG_VERBOSEVF(...) debug_vprintf(__VA_ARGS__)

#else /* DEBUG_VERBOSE_OUTPUT && DEBUG_OUTPUT */

#define DEBUG_VERBOSEFL(...) do {} while(0)
#define DEBUG_VERBOSEF(...) do {} while(0)
#define DEBUG_VERBOSEVF(...) do {} while(0)

#endif  /* DEBUG_VERBOSE_OUTPUT && DEBUG_OUTPUT */

#define DEBUG(...) DEBUGFL(__VA_ARGS__)
#define DEBUG_VERBOSE(...) DEBUG_VERBOSEFL(__VA_ARGS__)

DebugOutput debug_set_output(DebugOutput  /*output_mode*/,
                             const char  */*log_name*/);
   /*
    * Configures how debugging text should subsequently be output by the
    * debug_printf function (e.g. appended to a file in <Wimp$ScrapDir>, sent
    * to Martin Avison's 'Reporter' module, or a system log). The log name
    * should probably be the name of the application being debugged so that it
    * doesn't get mixed up with output from other applications.
    * Returns: The previous output mode (in case it needs to be restored).
    */

void debug_printf(const char */*format*/, ...) CHECK_PRINTF(1, 2);
   /*
    * Equivalent to the standard ANSI C library function printf except that
    * the formatted output is logged according to the output mode configured by
    * debug_set_output.
    */

void debug_printfl(const char */*format*/, ...) CHECK_PRINTF(1, 2);
   /*
    * Equivalent to the standard ANSI C library function printf except that
    * the formatted output is logged according to the output mode configured by
    * debug_set_output, and a line feed is automatically appended to it.
    */

void debug_vprintf(const char */*format*/, va_list /*arg*/) CHECK_PRINTF(1, 0);
   /*
    * Equivalent to the standard ANSI C library function vprintf except that
    * the formatted output is logged according to the output mode configured by
    * debug_set_output.
    */

/* Deprecated type and enumeration constant names */
#define Debug_Output DebugOutput
#define Debug_Output_Reporter DebugOutput_Reporter

#endif /* Debug_h */
