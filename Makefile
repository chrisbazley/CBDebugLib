# Project:   CBDebugLib
include MakeCommon

# Tools
CC = gcc
LibFile = ar

# Toolflags:
CCFlags = -g -c -Wall -Wextra -pedantic -std=c99 -MMD -MP -DDEBUG_OUTPUT -o $@
CCModuleFlags = $(CCFlags) -mmodule
LibFileFlags = -rcs $@

DebugObjects = $(addsuffix .o,$(ObjectList))
ModuleObjects = $(addsuffix .oz,$(ObjectList))

# Final targets:
all: lib$(LibName).a

lib$(LibName).a: $(DebugObjects)
	$(LibFile) $(LibFileFlags) $(DebugObjects)

# User-editable dependencies:
# All of these suffixes must also be specified in UnixEnv$*$sfix
.SUFFIXES: .o .c .oz
.c.o:
	${CC} $(CCFlags) -MF $*.d $<

# These files are generated during compilation to track C header #includes.
# It's not an error if they don't exist.
-include $(addsuffix .d,$(ObjectList))
