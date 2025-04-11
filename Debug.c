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

/* History:
  CJB: 15-Jan-05: Moved out from SFeditor sources.
  CJB: 21-Jan-05: Now uses vs[n]printf in place of custom code (much simpler
                  and means that all conversion specifications are understood).
  CJB: 27-Feb-05: Bug fix: hadn't taken into account "Report " prefix when
                  passing maximum output size to vsnprintf(), hence could
                  write past end of array.
  CJB: 13-Oct-06: Qualified returned _kernel_oserror pointers as 'const'.
  CJB: 11-Mar-07: Now invokes SWI Report_Text0 instead of the equivalent star
                  command (significantly faster).
  CJB: 02-May-08: Created a function named dlogf(), which writes formatted
                  debugging output to a log file in the Wimp scrap directory.
  CJB: 29-Aug-09: Subsumed the old dlogf and dprintf functions into a new
                  function debug_vprintf, to allow the output method to be
                  changed without recompiling all the code to be debugged.
                  Added support for output via stdout, stderr, buffered file
                  output, or SysLog. A new function allows client programs to
                  configure the output method.
  CJB: 05-Oct-09: Updated to use new enumerated type and value names.
  CJB: 18-Feb-12: Additional assertions to detect null parameters, string
                  formatting errors and buffer overflow/truncation.
  CJB: 11-Jan-15: Deal nicely with truncated Syslog/Reporter output instead
                  of aborting on assertion failure.
  CJB: 18-Apr-15: Removed assertions to avoid recursion now that they are
                  provided by debug.h.
  CJB: 09-Apr-16: Result of strlen is no longer wrongly treated as signed.
                  A -ve return value from vsprintf is now handled differently
                  from buffer overflow to avoid GNU C compiler warnings.
  CJB: 12-Mar-23: Extended debug_set_output to return the previous mode.
  CJB: 17-Jun-23: Conditionally compiled some functions to guard against
                  multiple definitions of the debug_set_output function.
  CJB: 11-Apr-25: Dogfooding the _Optional qualifier.
                  Stop treating NULL as a valid value of va_list.
*/

/* ISO library headers */
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#ifdef ACORN_C
/* Acorn C/C++ library headers */
#include "kernel.h"
#include "swis.h"
#endif

/* Local headers */
#include "Debug.h"
#include "Internal/CBDebMisc.h"

#define Report_Text0      0x054C80 /* SWI number for the Reporter module */

/* SWI numbers for the SysLog module */
#ifndef SysLog_OpenSessionLog
#define SysLog_OpenSessionLog  0x4C888
#endif
#ifndef SysLog_LogMessage
#define SysLog_LogMessage      0x4C880
#endif
#ifndef SysLog_CloseSessionLog
#define SysLog_CloseSessionLog 0x4C889
#endif

#define SYSLOG_PRIORITY 124

#define TRUNC_STRING "..."
#define BAD_STRING "BAD"

static DebugOutput mode = DebugOutput_None;
static _Optional FILE *log_file;
#ifdef ACORN_C
static int syslog_handle;
#endif

#ifdef DEBUG_OUTPUT
/* ----------------------------------------------------------------------- */
/*                         Private functions                               */

static void _debug_at_exit(void)
{
  /* Called at exit to ensure that any open session log or file is closed */
  debug_set_output(DebugOutput_None, "");
}

/* ----------------------------------------------------------------------- */
/*                         Public functions                                */

DebugOutput debug_set_output(DebugOutput output_mode, const char *log_name)
{
  assert(output_mode < DebugOutput_LAST);
  assert(log_name != NULL);

  if (mode == output_mode)
    return mode;

  static bool atexit_done = false;
  if (!atexit_done)
  {
    atexit(_debug_at_exit);
    atexit_done = true;
  }

  switch (mode)
  {
    case DebugOutput_FlushedFile:
    case DebugOutput_File:
      /* Close the log file in <Wimp$ScrapDir> */
      if (log_file != NULL)
      {
        fclose(&*log_file);
        log_file = NULL;
      }
      break;

#ifdef ACORN_C
    case DebugOutput_SessionLog:
      /* Close the session log and append its data to the main log file */
      if (syslog_handle != 0)
      {
        _swix(SysLog_CloseSessionLog, _IN(0), syslog_handle);
        syslog_handle = 0;
      }
      break;
#endif

    default:
      /* Do nothing */
      break;
  }

  DebugOutput const old_mode = mode;
  mode = output_mode;

  switch (mode)
  {
    case DebugOutput_FlushedFile:
    case DebugOutput_File:
    {
      /* Open a file in <Wimp$ScrapDir> to append debugging output */
      char file_path[256];

      strcpy(file_path, "<Wimp$ScrapDir>.");
      strcat(file_path, log_name);
      log_file = fopen(file_path, "a");
      break;
    }
#ifdef ACORN_C
    case DebugOutput_SysLog:
    {
      syslog_handle = (int)log_name;
      break;
    }
    case DebugOutput_SessionLog:
    {
      /* Open a session log to group events from this instance together */
      _swix(SysLog_OpenSessionLog,
            _INR(0,1)|_OUT(0),
            log_name,
            SYSLOG_PRIORITY,
            &syslog_handle);
      break;
    }
#endif
    default:
    {
      /* Do nothing */
      break;
    }
  }
  return old_mode;
}
#endif

/* ----------------------------------------------------------------------- */

void debug_printf(const char *format, ...)
{
  va_list ap;

  va_start(ap, format);
  debug_vprintf(format, ap);
  va_end(ap);
}

/* ----------------------------------------------------------------------- */

void debug_printfl(const char *format, ...)
{
  va_list ap;

  va_start(ap, format);
  debug_vprintf(format, ap);
  va_end(ap);

  debug_printf("\n");
}

/* ----------------------------------------------------------------------- */

void debug_vprintf(const char *format, va_list arg)
{
  switch (mode)
  {
#ifdef ACORN_C
    case DebugOutput_SplitStdOut:
    {
      /* Issue a VDU command to split the text and graphics cursors */
      _swix(OS_WriteI+4, 0);
      /* fallthrough */
    }
#endif
    case DebugOutput_StdOut:
    {
      /* Send a string constructed from the format string and variadic
         arguments to the standard output stream */
      (void)vprintf(format, arg);
      break;
    }
    case DebugOutput_StdErr:
    {
      /* Send a string constructed from the format string and variadic
         arguments to the standard error stream */
      (void)vfprintf(stderr, format, arg);
      break;
    }
    case DebugOutput_FlushedFile:
    case DebugOutput_File:
    {
      if (log_file != NULL)
      {
        /* Append a string constructed from the format string and variadic
           arguments to the log file */
        (void)vfprintf(&*log_file, format, arg);
        if (mode == DebugOutput_FlushedFile)
        {
          /* Flush the output stream to ensure that all data has been written
             to the log file. */
          fflush(&*log_file);
        }
      }
      break;
    }
#ifdef ACORN_C
    case DebugOutput_Reporter:
    case DebugOutput_SysLog:
    case DebugOutput_SessionLog:
    {
      static unsigned int accumulated = 0;
      static char line[256] = "";
      char formatted[256];
      char *readp;
      int nout;

      /* Generate a string from the format string and variadic arguments */
#ifndef OLD_SCL_STUBS
      nout = vsnprintf(formatted, sizeof(formatted), format, arg);
#else
      nout = vsprintf(formatted, format, arg);
#endif
      if (nout < 0)
      {
        strcpy(formatted, BAD_STRING);
      }
      else if ((size_t)nout >= sizeof(formatted))
      {
        /* String was truncated to fit in the buffer */
        strcpy(formatted + sizeof(formatted) - sizeof(TRUNC_STRING),
               TRUNC_STRING);
      }

      /* Loop until reaching the end of the string to be output */
      readp = formatted;
      while (*readp != '\0')
      {
        char   *eol;
        size_t  len, copied;

        /* Search for the next line feed in the string to be output */
        eol = strchr(readp, '\n');
        if (eol == NULL)
          len = strlen(readp); /* output remainder of string */
        else
          len = eol - readp; /* output substring to next line feed */

        /* Guard against overrunning the end of the buffer
           by discarding the end of overlong lines */
        copied = sizeof(line) - 1 - accumulated;
        if (len < copied)
          copied = len;
        (void)strncat(line, readp, copied);

        if (eol == NULL)
        {
          /* No more line feeds in the string to be output */
          accumulated += copied;
          }
          else
          {
          /* Output the accumulated text line */
          if (mode == DebugOutput_Reporter)
          {
            (void)_swix(Report_Text0, _IN(0), line);
          }
          else
          {
            (void)_swix(SysLog_LogMessage,
                        _INR(0,2),
                        syslog_handle,
                        line,
                        SYSLOG_PRIORITY);
          }

          /* Reset the buffer for the start of the next line */
          accumulated = 0;
          line[accumulated] = '\0';
          len ++; /* eat the line feed */
        }

        /* Advance the read pointer to the end of the string or next line
           feed. Not all of the intervening text was necessarily output. */
        readp += len;
      }
      break;
    }
#endif

    default:
    {
      /* Do nothing */
      break;
    }
  }
}
