# CBDebugLib
(C) 2018 Christopher Bazley

Release 8 (11 Apr 2025)

Introduction
------------
  This C library provides debugging facilities for RISC OS software
development. It can have a large number of external dependencies, including
most of the libraries supplied with the Acorn C/C++ package.

  Its most useful feature is the ability to usurp calls to Acorn's flex
library functions and redirect them to Simon P. Bullen's fortified memory
allocation shell. This allows debugging of out-of-bounds memory accesses and
simulation of allocation failure.

  Similarly, calls to many functions in the Toolbox and Wimp libraries can
be intercepted in order to simulate them returning an error value. This
allows stress-testing of application programs.

Fortified memory allocation
---------------------------
  I use Fortify to find memory leaks in my applications, detect corruption
of the heap (e.g. caused by writing beyond the end of a heap block), and do
stress testing (by causing some memory allocations to fail). Fortify is
available separately from this web site:
http://web.archive.org/web/20020615230941/www.geocities.com/SiliconValley/Horizon/8596/fortify.html

  By default, Fortify only intercepts the ANSI standard memory allocation
functions (e.g. 'malloc', 'free' and 'realloc'). This limits its usefulness
as a debugging tool if your program also uses different memory allocator such
as Acorn's 'flex' library.

  Therefore, I wrote the 'PseudoFlex' module of CBDebugLib, to enable Fortify
to be used to debug code that was designed to use the 'flex' library. It uses
macros to intercept calls to 'flex_alloc', 'flex_midextend', 'flex_free',
etc. These are replaced with calls to the equivalent 'PseudoFlex' functions,
which emulate the 'flex' library using Fortify versions of the ANSI C memory
allocation functions.

  The source file name and line number of the calling code are passed
straight through so that you can see where leaked memory blocks were actually
allocated (i.e. at the intercepted call to 'flex_alloc' rather than the
subsequent invocation of 'Fortify_malloc' within c.PseudoFlex).

  Programs linked with CBDebugLib must also be linked with Fortify, for
example by adding 'C:o.Fortify' to the list of object files specified to the
linker. Otherwise, you will get build-time errors like this:
```
ARM Linker: (Error) Undefined symbol(s).
ARM Linker:     Fortify_malloc, referred to from C:o.CBDebugLib(PseudoIO).
ARM Linker:     Fortify_free, referred to from C:o.CBDebugLib(PseudoIO).
```
  It is important that Fortify is also enabled when compiling code to be
linked with CBDebugLib. That means #including the "Fortify.h" and
"PseudoFlex.h" headers in each of your source files, and pre-defining the
C pre-processor symbol 'FORTIFY'. If you are using the Acorn C compiler
then this can be done by adding '-DFORTIFY' to the command line.

  Linking unfortified programs with CBDebugLib will cause run time errors
when your program tries to reallocate or free a heap block allocated within
CBDebugLib, or CBDebugLib tries to reallocate or free a block allocated by
your program. Typically this manifests as 'Flex memory error' or
'Unrecoverable error in run time system: free failed, (heap overwritten)'.

Rebuilding the library
----------------------
  You should ensure that the Acorn C/C++ library directories clib, tboxlibs
and flexlib are on your C$Path, otherwise the compiler won't be able to find
the required header files.

  Three make files are supplied:

- 'Makefile' is intended for use with GNU Make and the GNU C Compiler on Linux.
- 'NMakefile' is intended for use with Acorn Make Utility (AMU) and the
   Norcroft C compiler supplied with the Acorn C/C++ Development Suite.
- 'GMakefile' is intended for use with GNU Make and the GNU C Compiler on RISC OS.

These make files share some variable definitions (lists of objects to be
built) by including a common make file.

  The APCS variant specified for the Norcroft compiler is 32 bit for
compatibility with ARMv5 and fpe2 for compatibility with older versions of
the floating point emulator. Generation of unaligned data loads/stores is
disabled for compatibility with ARM v6.

  The suffix rules generate output files with different suffixes (or in
different subdirectories, if using the supplied make files on RISC OS),
depending on the compiler options used to compile the source code:

o: Assertions and debugging output are enabled. The code includes
   symbolic debugging data (e.g. for use with DDT).

oz: Identical to suffix 'o' except that the code is suitable for inclusion
    in a relocatable module (multiple instantiation of static data and stack
    limit checking disabled).

d: 'GMakefile' passes '-MMD' when invoking gcc so that dynamic dependencies
   are generated from the #include commands in each source file and output
   to a temporary file in the directory named 'd'. GNU Make cannot
   understand rules that contain RISC OS paths such as /C:Macros.h as
   prerequisites, so 'sed', a stream editor, is used to strip those rules
   when copying the temporary file to the final dependencies file.

  The above suffixes must be specified in various system variables which
control filename suffix translation on RISC OS, including at least
UnixEnv$ar$sfix, UnixEnv$gcc$sfix and UnixEnv$make$sfix.
Unfortunately GNU Make doesn't apply suffix rules to make object files in
subdirectories referenced by path even if the directory name is in
UnixEnv$make$sfix, which is why 'GMakefile' uses the built-in function
addsuffix instead of addprefix to construct lists of the objects to be
built (e.g. foo.o instead of o.foo).

  Before compiling the library for RISC OS, move the C source and header
files with .c and .h suffixes into subdirectories named 'c' and 'h' and
remove those suffixes from their names. You probably also need to create
'o', 'oz' and 'd' subdirectories for compiler output.

Licence and disclaimer
----------------------
  This library is free software; you can redistribute it and/or modify it
under the terms of the GNU Lesser General Public License as published by the
Free Software Foundation; either version 2.1 of the License, or (at your
option) any later version.

  This library is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License
for more details.

  You should have received a copy of the GNU Lesser General Public License
along with this library; if not, write to the Free Software Foundation,
Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

Credits
-------
  This library was derived from CBLibrary. Both libraries are free software,
available under the same license.

History
-------
Release 1 (04 Nov 2018)
- Extracted the relevant components from CBLib to make a standalone library.

Release 2 (10 Feb 2019)
- Fixed a bug in PseudoFlex_midextend(), which called free() instead of
  Fortify_free(). This made it unusable with negative 'by' values if
  compiled without FORTIFY defined (as is the norm).

Release 3 (30 Sep 2020)
- Fixed a null pointer dereference in event_poll when null is passed instead
  of an event_code address.
- pseudo_event_wait_for_idle() no longer prints or dispatches the final null
  event it receives.
- Reduced the volume of debugging output about flex memory usage.
- More local const declarations with initializers.
- Uses a new external function, Fortify_AllowAllocate(), to avoid
  accumulating huge numbers of 'freed' dummy memory allocations.
- Changed a definition of the assert macro to declare the
  stringified expression as a separate string to prevent a
  percent sign being misinterpreted as a format specifier.

Release 4 (01 Dec 2020)
- Fixed a null pointer dereference in event_poll_idle() when null is passed
  instead of an event_code address.

Release 5 (28 Jul 2022)
- More data in debugging output.
- Added interception of the gadget_set_focus function.
- Handle negative or zero values passed to wimp_drag_box (for cancellation).
- Removed redundant uses of the 'extern' keyword.
- Added #include commands to ensure that everything declared by the original
  Toolbox library headers is declared before defining macro versions of
  library functions (since it's impossible to do so afterwards).

Release 6 (17 Jun 2023)
- Extended the debug_set_output function to return the previous mode.
- Added a macro definition to generate a source code location as a string.
- Do-nothing macros are now defined as do {} while(0) instead of if (0)...
  so their arguments needn't be valid and to prevent unintended association
  with 'else' blocks.
- The header file PseudoTbox.h can now be used as a proxy for "textarea.h".
- Conditionally compiled some functions to guard against multiple
  definitions of the debug_set_output function.
- Annotated unused variables to suppress warnings when debug output is
  disabled at compile time.

Release 7 (19 May 2024)
- Improved the README.md file for Linux users.
- A new file, MakeROCom, is used instead of MakeCommon when building for
  RISC OS (untested), to ensure that most of the library isn't compiled
  when building for Linux.
- Code that depends on Acorn's C library is now compiled only if macro
  ACORN_C is defined.

Release 8 (11 Apr 2025)
- Dogfooding the _Optional qualifier.
- Stop treating NULL as a valid value of va_list.

Contact details
---------------
Christopher Bazley

Email: mailto:cs99cjb@gmail.com

WWW:   http://starfighter.acornarcade.com/mysite/
