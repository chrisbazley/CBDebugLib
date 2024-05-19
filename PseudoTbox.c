/*
 * CBDebugLib: Error injection veneer to Acorn's toolbox library
 * Copyright (C) 2014 Christopher Bazley
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
  CJB: 23-Dec-14: Created this source file.
  CJB: 19-Apr-15: Now intercepts extra functions: iconbar_get_icon_handle,
                  window_force_redraw, menu_set_fade and menu_get_fade.
                  Added hooks for monitoring Toolbox_ObjectAutoCreated and
                  Toolbox_ObjectDeleted events.
                  Added one function to query whether an object has been shown
                  by toolbox_show_object and another to reset the mechanism.
                  The parameters of saveas_file_save_completed and
                  saveas_buffer_filled are now recorded only if primed by a
                  unit test (in which case the call is now suppressed too).
  CJB: 16-Sep-15: Guard against being asked to show or hide unrecognized objects
                  (because event_initialise was not intercepted and therefore object
                  auto-creation was not recorded).
  CJB: 07-Dec-15: Fixed bugs in the pseudo_toolbox_create_object function:
                  Previously the (unpredictable) initial value of an output
                  parameter was printed instead of the new object's ID, and
                  passing a null pointer resulted in undefined behaviour.
                  The window_set_extent function is now intercepted too.
  CJB: 08-Feb-16: Increased the size of the buffer used to record data passed to
                  saveas_buffer_filled for unit tests from 256 bytes to 128 KB.
  CJB: 07-Mar-16: The gadget_get_bbox function is now intercepted too.
  CJB: 21-Apr-16: Modified assertions and added casts to avoid GNU C compiler
                  warnings.
  CJB: 27-Nov-21: Added interception of the gadget_set_focus function.
                  More debugging output from other functions.
  CJB: 17-Jun-23: Annotated unused variables to suppress warnings when
                  debug output is disabled at compile time.
*/

#undef FORTIFY /* Prevent macro redirection of toolbox_... calls to
                  pseudo_toolbox_... functions within this source file. */

/* ISO library headers */
#include <stddef.h>
#include <string.h>
#include <stdbool.h>

/* Acorn C/C++ library headers */
#include "kernel.h"
#include "swis.h"
#include "toolbox.h"
#include "iconbar.h"
#include "saveas.h"
#include "proginfo.h"
#include "scale.h"
#include "fontdbox.h"
#include "quit.h"
#include "dcs.h"
#include "printdbox.h"
#include "colourdbox.h"
#include "fileinfo.h"
#include "window.h"
#include "menu.h"
#include "gadgets.h"

/* Local headers */
#include "Internal/CBDebMisc.h"
#include "PseudoTbox.h"
#include "PseudoKern.h"
#include "LinkedList.h"
#include "Debug.h"

/* This list of objects is currently used only to detect leaks */
static LinkedList objects;

typedef struct
{
  LinkedListItem list_item;
  ObjectId object_id;
  bool is_showing;
}
PseudoTbox_Object;

static bool reset_object_record(LinkedList *list, LinkedListItem *item, void *arg)
{
  PseudoTbox_Object * const record = (PseudoTbox_Object *)item;

  assert(list == &objects);
  NOT_USED(arg);
  record->is_showing = false;
  return false;
}

void pseudo_toolbox_reset(void)
{
  DEBUGF("PseudoTbox: Reset\n");
  linkedlist_for_each(&objects, reset_object_record, NULL);
  pseudo_saveas_reset_file_save_completed();
  pseudo_saveas_reset_buffer_filled();
}

_kernel_oserror *pseudo_toolbox_initialise( unsigned int flags,
                                            int wimp_version,
                                            int *wimp_messages,
                                            int *toolbox_events,
                                            char *directory,
                                            MessagesFD *mfd,
                                            IdBlock *idb,
                                            int *current_wimp_version,
                                            int *task,
                                            void **sprite_area,
                                            const char *file,
                                            unsigned long line
                                          )
{
  _kernel_oserror *e = pseudokern_fail(file, line);

  if (e == NULL)
  {
    linkedlist_init(&objects);
    e = toolbox_initialise(flags, wimp_version, wimp_messages, toolbox_events, directory, mfd, idb, current_wimp_version, task, sprite_area);
  }

  return e;

}

static bool template_name_matches(LinkedList *list, LinkedListItem *item, void *arg)
{
  const PseudoTbox_Object * const record = (PseudoTbox_Object *)item;
  const char *template_name = arg;
  char buffer[256];
  int nbytes;
  _kernel_oserror *e;

  assert(list == &objects);
  assert(record != NULL);
  assert(template_name != NULL);

  e = toolbox_get_template_name(0, record->object_id, buffer, sizeof(buffer), &nbytes);
  if (e != NULL)
  {
    DEBUGF("toolbox_get_template_name error: 0x%x %s\n", e->errnum, e->errmess);
    return false;
  }
  else
  {
    assert(nbytes >= 0);
    assert((size_t)nbytes <= sizeof(buffer));

    DEBUG_VERBOSEF("PseudoTbox: Compare %s with %s\n", buffer, template_name);
    return !strcmp(buffer, template_name);
  }
}

ObjectId pseudo_toolbox_find_by_template_name(char *template_name)
{
  LinkedListItem *item;
  ObjectId id;

  DEBUGF("PseudoTbox: Finding object created from template '%s'\n", template_name);
  assert(template_name != NULL);
  item = linkedlist_for_each(&objects, template_name_matches, template_name);
  if (item == NULL)
  {
    id = NULL_ObjectId;
  }
  else
  {
    id = ((PseudoTbox_Object *)item)->object_id;
  }
  return id;
}

void pseudo_toolbox_object_created(ObjectId id)
{
  PseudoTbox_Object * const record = malloc(sizeof(*record));

  DEBUGF("PseudoTbox: Object 0x%x was auto-created\n", id);
  if (record != NULL)
  {
    record->object_id = id;
    record->is_showing = false;
    linkedlist_insert(&objects, NULL, &record->list_item);
  }
  else
  {
    DEBUGF("PseudoTbox: Not enough memory to create record of object\n");
  }
}

static bool object_id_matches(LinkedList *list, LinkedListItem *item, void *arg)
{
  const PseudoTbox_Object * const record = (PseudoTbox_Object *)item;
  const ObjectId * const id = arg;

  assert(list == &objects);
  assert(record != NULL);
  assert(id != NULL);
  return *id == record->object_id;
}

void pseudo_toolbox_object_deleted(ObjectId id)
{
  LinkedListItem *item;

  DEBUGF("PseudoTbox: Object 0x%x was deleted\n", id);
  item = linkedlist_for_each(&objects, object_id_matches, &id);
  if (item != NULL)
  {
    linkedlist_remove(&objects, item);
    free(item);
  }
}

_kernel_oserror *pseudo_toolbox_create_object(unsigned int flags, void *name_or_template, ObjectId *id, const char *file, unsigned long line)
{
  _kernel_oserror *e = NULL;
  PseudoTbox_Object * const record = malloc(sizeof(*record));

  if (record != NULL)
  {
    e = pseudokern_fail(file, line);
    if (e == NULL)
    {
      e = toolbox_create_object(flags, name_or_template, &record->object_id);

      DEBUGF("toolbox_create_object 0x%x from %s at %s:%lu\n",
             record->object_id,
             (flags & Toolbox_CreateObject_InCore) ? "template" : (char *)name_or_template,
             file, line);

      if (id != NULL)
        *id = record->object_id;
    }

    if (e == NULL)
    {
      record->is_showing = false;
      linkedlist_insert(&objects, NULL, &record->list_item);
    }
    else
    {
      free(record);
    }
  }
  else
  {
    static const _kernel_oserror temp = {DUMMY_ERRNO, "NoMem"};
    _kernel_swi_regs regs;

    DEBUGF("PseudoTbox: Not enough memory to create record of object\n");

    regs.r[0] = (int)&temp;
    regs.r[1] = 0; /* use global messages */
    regs.r[2] = 0; /* use an internal buffer */
    regs.r[3] = 0; /* buffer size */
    e = _kernel_swi(MessageTrans_ErrorLookup, &regs, &regs);
  }

  return e;
}

_kernel_oserror *pseudo_toolbox_delete_object(unsigned int flags, ObjectId id, const char *file, unsigned long line)
{
  pseudo_toolbox_object_deleted(id);
  DEBUGF("toolbox_delete_object 0x%x at %s:%lu\n", id, file, line);
  NOT_USED(file);
  NOT_USED(line);
  return toolbox_delete_object(flags, id);
}

_kernel_oserror *pseudo_toolbox_show_object(unsigned int flags, ObjectId id, int show_type, void *type, ObjectId parent, ComponentId parent_component, const char *file, unsigned long line)
{
  _kernel_oserror *e = pseudokern_fail(file, line);

  if (e == NULL)
  {
    PseudoTbox_Object * const record = (PseudoTbox_Object *)linkedlist_for_each(&objects, object_id_matches, &id);

    /* We can't rely on auto-created objects having been recorded because
       event library initialization may not have been intercepted. */
    if (record != NULL)
      record->is_showing = true;

    DEBUGF("toolbox_show_object 0x%x at %s:%lu\n", id, file, line);
    e = toolbox_show_object(flags, id, show_type, type, parent, parent_component);
  }

  return e;
}

_kernel_oserror *pseudo_toolbox_hide_object(unsigned int flags, ObjectId id, const char *file, unsigned long line)
{
  _kernel_oserror *e = pseudokern_fail(file, line);

  if (e == NULL)
  {
    PseudoTbox_Object * const record = (PseudoTbox_Object *)linkedlist_for_each(&objects, object_id_matches, &id);

    /* We can't rely on auto-created objects having been recorded because
       event library initialization may not have been intercepted. */
    if (record != NULL)
      record->is_showing = false;

    DEBUGF("toolbox_hide_object 0x%x at %s:%lu\n", id, file, line);
    e = toolbox_hide_object(flags, id);
  }

  return e;
}

bool pseudo_toolbox_object_is_showing(ObjectId id)
{
  const PseudoTbox_Object *record;
  bool is_showing = false;

  record = (PseudoTbox_Object *)linkedlist_for_each(&objects, object_id_matches, &id);

  /* We can't rely on auto-created objects having been recorded because
     event library initialization may not have been intercepted. */
  if (record != NULL)
    is_showing = record->is_showing;

  DEBUGF("PseudoTbox: Object 0x%x is %sshowing\n", id, is_showing ? "" : "not ");
  return is_showing;
}

_kernel_oserror *pseudo_toolbox_set_client_handle(unsigned int flags, ObjectId id, void *client_handle, const char *file, unsigned long line)
{
  _kernel_oserror *e = pseudokern_fail(file, line);

  if (e == NULL)
    e = toolbox_set_client_handle(flags, id, client_handle);

  return e;
}

_kernel_oserror *pseudo_toolbox_get_client_handle(unsigned int flags, ObjectId id, void *client_handle, const char *file, unsigned long line)
{
  _kernel_oserror *e = pseudokern_fail(file, line);

  if (e == NULL)
    e = toolbox_get_client_handle(flags, id, client_handle);

  return e;
}

_kernel_oserror *pseudo_toolbox_get_object_class(unsigned int flags, ObjectId id, ObjectClass *object_class, const char *file, unsigned long line)
{
  _kernel_oserror *e = pseudokern_fail(file, line);

  if (e == NULL)
    e = toolbox_get_object_class(flags, id, object_class);

  return e;
}

_kernel_oserror *pseudo_toolbox_get_object_state(unsigned int flags, ObjectId id, unsigned int *state, const char *file, unsigned long line)
{
  _kernel_oserror *e = pseudokern_fail(file, line);

  if (e == NULL)
    e = toolbox_get_object_state(flags, id, state);

  return e;
}

_kernel_oserror *pseudo_iconbar_get_icon_handle(unsigned int flags, ObjectId iconbar, int *icon_handle, const char *file, unsigned long line)
{
  _kernel_oserror *e = pseudokern_fail(file, line);

  if (e == NULL)
    e = iconbar_get_icon_handle(flags, iconbar, icon_handle);

  return e;
}

_kernel_oserror *pseudo_saveas_set_file_name(unsigned int flags, ObjectId saveas, char *file_name, const char *file, unsigned long line)
{
  _kernel_oserror *e = pseudokern_fail(file, line);

  if (e == NULL)
    e = saveas_set_file_name(flags, saveas, file_name);

  return e;
}

_kernel_oserror *pseudo_saveas_set_file_type(unsigned int flags, ObjectId saveas, int file_type, const char *file, unsigned long line)
{
  _kernel_oserror *e = pseudokern_fail(file, line);

  if (e == NULL)
    e = saveas_set_file_type(flags, saveas, file_type);

  return e;
}

_kernel_oserror *pseudo_saveas_get_file_type(unsigned int flags, ObjectId saveas, int *file_type, const char *file, unsigned long line)
{
  _kernel_oserror *e = pseudokern_fail(file, line);
  if (e == NULL)
    e = saveas_get_file_type(flags, saveas, file_type);

  return e;
}

_kernel_oserror *pseudo_saveas_set_file_size(unsigned int flags, ObjectId saveas, int file_size, const char *file, unsigned long line)
{
  _kernel_oserror *e = pseudokern_fail(file, line);

  if (e == NULL)
    e = saveas_set_file_size(flags, saveas, file_size);

  return e;
}

static unsigned int buffer_filled_flags = 0;
static ObjectId buffer_filled_id = NULL_ObjectId;
static char buffer_filled_buffer[128 << 10];
static int buffer_filled_bytes_written = 0;
static bool intercept_saveas_buffer_filled = false;

void pseudo_saveas_reset_buffer_filled(void)
{
  buffer_filled_flags = 0;
  buffer_filled_id = NULL_ObjectId;
  memset(buffer_filled_buffer, 0, sizeof(buffer_filled_buffer));
  buffer_filled_bytes_written = 0;
  intercept_saveas_buffer_filled = true;
}

ObjectId pseudo_saveas_get_buffer_filled(unsigned int *flags, void *buffer, int buff_size, int *nbytes)
{
  if (flags != NULL)
    *flags = buffer_filled_flags;

  if (buffer != NULL)
    memcpy(buffer, buffer_filled_buffer, LOWEST(buff_size, buffer_filled_bytes_written));

  if (nbytes != NULL)
    *nbytes = buffer_filled_bytes_written;

  return buffer_filled_id;
}

_kernel_oserror *pseudo_saveas_buffer_filled(unsigned int flags, ObjectId saveas, void *buffer, int bytes_written, const char *file, unsigned long line)
{
  DEBUGF("saveas_buffer_filled called with flags 0x%x, object 0x%x, buffer %p, bytes %d at %s:%lu\n",
         flags, (unsigned)saveas, buffer, bytes_written, file, line);

  /* It's not clear how fill buffer event handlers are meant to handle
     errors. Record the parameters anyway for consistency with
     pseudo_saveas_file_save_completed. */
  if (intercept_saveas_buffer_filled)
  {
    buffer_filled_flags = flags;
    assert(bytes_written >= 0);
    assert((size_t)bytes_written <= sizeof(buffer_filled_buffer));
    memcpy(buffer_filled_buffer, buffer, bytes_written);
    buffer_filled_bytes_written = bytes_written;
    buffer_filled_id = saveas;
    intercept_saveas_buffer_filled = false;
    return NULL;
  }
  else
  {
    _kernel_oserror *e = pseudokern_fail(file, line);
    if (e == NULL)
      e = saveas_buffer_filled(flags, saveas, buffer, bytes_written);
    return e;
  }
}

static unsigned int file_save_completed_flags = 0;
static ObjectId file_save_completed_id = NULL_ObjectId;
static char file_save_completed_filename[sizeof(SaveAsSaveToFileEvent) -
                                         offsetof(SaveAsSaveToFileEvent, filename)] = "";
static bool intercept_file_save_completed = false;

void pseudo_saveas_reset_file_save_completed(void)
{
  file_save_completed_flags = 0;
  file_save_completed_id = NULL_ObjectId;
  strcpy(file_save_completed_filename, "");
  intercept_file_save_completed = true;
}

ObjectId pseudo_saveas_get_file_save_completed(unsigned int *flags, char *buffer, int buff_size, int *nbytes)
{
  if (flags != NULL)
    *flags = file_save_completed_flags;

  if (buffer != NULL && buff_size > 0)
  {
    *buffer = '\0';
    strncat(buffer, file_save_completed_filename, buff_size-1);
  }

  if (nbytes != NULL)
    *nbytes = strlen(file_save_completed_filename)+1;

  return file_save_completed_id;
}

_kernel_oserror *pseudo_saveas_file_save_completed(unsigned int flags, ObjectId saveas, char *filename, const char *file, unsigned long line)
{
  DEBUGF("saveas_file_save_completed called with flags 0x%x, object 0x%x, filename %s at %s:%lu\n", flags, (unsigned)saveas, filename, file, line);

  /* We have to treat this function specially for several reasons:
     - tests expect it to have been called on error and have no way of
       knowing that this function rather than any other failed.
     - the Toolbox doesn't set up all of the state required to handle calls
       to this function properly, if SaveAs events have been faked by a test
       (e.g. unknown whether the save destination was safe or not). */
  if (intercept_file_save_completed)
  {
    file_save_completed_flags = flags;
    file_save_completed_id = saveas;
    STRCPY_SAFE(file_save_completed_filename, filename);
    intercept_file_save_completed = false;
    return NULL;
  }
  else
  {
    _kernel_oserror *e = pseudokern_fail(file, line);
    if (e == NULL)
      e = saveas_file_save_completed(flags, saveas, filename);
    return e;
  }
}

_kernel_oserror *pseudo_saveas_get_window_id(unsigned int flags, ObjectId saveas, ObjectId *window, const char *file, unsigned long line)
{
  _kernel_oserror *e = pseudokern_fail(file, line);
  if (e == NULL)
    e = saveas_get_window_id(flags, saveas, window);

  return e;

}

_kernel_oserror *pseudo_radiobutton_set_state(unsigned int flags, ObjectId window, ComponentId radio_button, int state, const char *file, unsigned long line)
{
  _kernel_oserror *e = pseudokern_fail(file, line);

  if (e == NULL)
    e = radiobutton_set_state(flags, window, radio_button, state);

  return e;
}

_kernel_oserror *pseudo_radiobutton_get_state(unsigned int flags, ObjectId window, ComponentId radio_button, int *state, ComponentId *selected, const char *file, unsigned long line)
{
  _kernel_oserror *e = pseudokern_fail(file, line);
  if (e == NULL)
    e = radiobutton_get_state(flags, window, radio_button, state, selected);

  return e;
}

_kernel_oserror *pseudo_optionbutton_set_state(unsigned int flags, ObjectId window, ComponentId option_button, int state, const char *file, unsigned long line)
{
  _kernel_oserror *e = pseudokern_fail(file, line);

  if (e == NULL)
    e = optionbutton_set_state(flags, window, option_button, state);

  return e;
}

_kernel_oserror *pseudo_optionbutton_get_state(unsigned int flags, ObjectId window, ComponentId option_button, int *state, const char *file, unsigned long line)
{
  _kernel_oserror *e = pseudokern_fail(file, line);

  if (e == NULL)
    e = optionbutton_get_state(flags, window, option_button, state);

  return e;
}

_kernel_oserror *pseudo_window_set_title(unsigned int flags, ObjectId window, char *title, const char *file, unsigned long line)
{
  _kernel_oserror *e = pseudokern_fail(file, line);
  if (e == NULL)
    e = window_set_title(flags, window, title);

  return e;
}

_kernel_oserror *pseudo_window_set_extent(unsigned int flags, ObjectId window, BBox *extent, const char *file, unsigned long line)
{
  _kernel_oserror *e = pseudokern_fail(file, line);
  if (e == NULL)
    e = window_set_extent(flags, window, extent);

  return e;
}

_kernel_oserror *pseudo_window_get_extent(unsigned int flags, ObjectId window, BBox *extent, const char *file, unsigned long line)
{
  _kernel_oserror *e = pseudokern_fail(file, line);
  if (e == NULL)
    e = window_get_extent(flags, window, extent);

  return e;
}

_kernel_oserror *pseudo_window_set_pointer(unsigned int flags, ObjectId window, char *sprite_name, int x_hot_spot, int y_hot_spot, const char *file, unsigned long line)
{
  _kernel_oserror *e = pseudokern_fail(file, line);
  if (e == NULL)
    e = window_set_pointer(flags, window, sprite_name, x_hot_spot, y_hot_spot);

  return e;
}

_kernel_oserror *pseudo_window_get_wimp_handle(unsigned int flags, ObjectId window, int *window_handle, const char *file, unsigned long line)
{
  _kernel_oserror *e = pseudokern_fail(file, line);

  if (e == NULL)
    e = window_get_wimp_handle(flags, window, window_handle);

  return e;
}

_kernel_oserror *pseudo_window_get_tool_bars(unsigned int flags, ObjectId window, ObjectId *ibl, ObjectId *itl, ObjectId *ebl, ObjectId *etl, const char *file, unsigned long line)
{
  _kernel_oserror *e = pseudokern_fail(file, line);

  if (e == NULL)
    e = window_get_tool_bars(flags, window, ibl, itl, ebl, etl);

  return e;
}

_kernel_oserror *pseudo_window_get_pointer_info(unsigned int flags, int *x_pos, int *y_pos, int *buttons, ObjectId *window, ComponentId *component, const char *file, unsigned long line)
{
  _kernel_oserror *e = pseudokern_fail(file, line);

  if (e == NULL)
    e = window_get_pointer_info(flags, x_pos, y_pos, buttons, window, component);

  return e;
}

_kernel_oserror *pseudo_window_force_redraw(unsigned int flags, ObjectId window, BBox *redraw_box, const char *file, unsigned long line)
{
  _kernel_oserror *e = pseudokern_fail(file, line);

  if (e == NULL)
    e = window_force_redraw(flags, window, redraw_box);

  return e;
}

_kernel_oserror *pseudo_actionbutton_set_text(unsigned int flags, ObjectId window, ComponentId action_button, char *text, const char *file, unsigned long line)
{
  _kernel_oserror *e = pseudokern_fail(file, line);

  if (e == NULL)
    e = actionbutton_set_text(flags, window, action_button, text);

  return e;
}

_kernel_oserror *pseudo_gadget_get_bbox(unsigned int flags, ObjectId window, ComponentId gadget, BBox *bbox, const char *file, unsigned long line)
{
  _kernel_oserror *e = pseudokern_fail(file, line);

  if (e == NULL)
    e = gadget_get_bbox(flags, window, gadget, bbox);

  return e;
}

_kernel_oserror *pseudo_gadget_set_help_message(unsigned int flags, ObjectId window, ComponentId gadget, char *message_text, const char *file, unsigned long line)
{
  DEBUGF("gadget_set_help_message called with flags 0x%x, object 0x%x, component 0x%x, message_text %s at %s:%lu\n", flags, (unsigned)window, (unsigned)gadget, message_text, file, line);
  _kernel_oserror *e = pseudokern_fail(file, line);

  if (e == NULL)
    e = gadget_set_help_message(flags, window, gadget, message_text);

  return e;
}

_kernel_oserror *pseudo_gadget_set_focus(unsigned int flags, ObjectId window, ComponentId component, const char *file, unsigned long line)
{
  DEBUGF("gadget_set_focus called with flags 0x%x, object 0x%x, component 0x%x at %s:%lu\n", flags, (unsigned)window, (unsigned)component, file, line);
  _kernel_oserror *e = pseudokern_fail(file, line);

  if (e == NULL)
    e = gadget_set_focus(flags, window, component);

  return e;
}

_kernel_oserror *pseudo_button_set_value(unsigned int flags, ObjectId window, ComponentId button, char *value, const char *file, unsigned long line)
{
  _kernel_oserror *e = pseudokern_fail(file, line);

  if (e == NULL)
    e = button_set_value(flags, window, button, value);

  return e;
}

_kernel_oserror *pseudo_button_get_value(unsigned int flags, ObjectId window, ComponentId button, char *buffer, int buff_size, int *nbytes, const char *file, unsigned long line)
{
  _kernel_oserror *e = pseudokern_fail(file, line);

  if (e == NULL)
    e = button_get_value(flags, window, button, buffer, buff_size, nbytes);

  return e;
}

_kernel_oserror *pseudo_button_set_validation(unsigned int flags, ObjectId window, ComponentId button, char *value, const char *file, unsigned long line)
{
  _kernel_oserror *e = pseudokern_fail(file, line);
  if (e == NULL)
    e = button_set_validation(flags, window, button, value);

  return e;
}

_kernel_oserror *pseudo_numberrange_set_value(unsigned int flags, ObjectId window, ComponentId number_range, int value, const char *file, unsigned long line)
{
  _kernel_oserror *e = pseudokern_fail(file, line);

  if (e == NULL)
    e = numberrange_set_value(flags, window, number_range, value);

  return e;
}

_kernel_oserror *pseudo_numberrange_get_value(unsigned int flags, ObjectId window, ComponentId number_range, int *value, const char *file, unsigned long line)
{
  _kernel_oserror *e = pseudokern_fail(file, line);

  if (e == NULL)
    e = numberrange_get_value(flags, window, number_range, value);

  return e;
}

_kernel_oserror *pseudo_slider_set_value(unsigned int flags, ObjectId window, ComponentId slider, int value, const char *file, unsigned long line)
{
  DEBUGF("slider_set_value called with flags 0x%x, object 0x%x, component 0x%x, value %d at %s:%lu\n", flags, (unsigned)window, (unsigned)slider, value, file, line);
  _kernel_oserror *e = pseudokern_fail(file, line);

  if (e == NULL)
    e = slider_set_value(flags, window, slider, value);

  return e;
}

_kernel_oserror *pseudo_slider_set_colour(unsigned int flags, ObjectId window, ComponentId slider, int bar_colour, int back_colour, const char *file, unsigned long line)
{
  DEBUGF("slider_set_colour called with flags 0x%x, object 0x%x, component 0x%x, bar_colour %d, back_colour %d at %s:%lu\n", flags, (unsigned)window, (unsigned)slider, bar_colour, back_colour, file, line);
  _kernel_oserror *e = pseudokern_fail(file, line);

  if (e == NULL)
    e = slider_set_colour(flags, window, slider, bar_colour, back_colour);

  return e;
}

_kernel_oserror *pseudo_menu_set_tick(unsigned int flags, ObjectId menu, ComponentId entry, int tick, const char *file, unsigned long line)
{
  DEBUGF("menu_set_tick with flags 0x%x, component 0x%x, object 0x%x, tick %d at %s:%lu\n",
         flags, entry, menu, tick, file, line);
  _kernel_oserror *e = pseudokern_fail(file, line);

  if (e == NULL)
    e = menu_set_tick(flags, menu, entry, tick);

  return e;
}

_kernel_oserror *pseudo_menu_get_tick(unsigned int flags, ObjectId menu, ComponentId entry, int *tick, const char *file, unsigned long line)
{
  _kernel_oserror *e = pseudokern_fail(file, line);

  if (e == NULL)
    e = menu_get_tick(flags, menu, entry, tick);

  return e;
}

_kernel_oserror *pseudo_menu_set_fade(unsigned int flags, ObjectId menu, ComponentId entry, int fade, const char *file, unsigned long line)
{
  DEBUGF("menu_set_fade with flags 0x%x, component 0x%x, object 0x%x, fade %d at %s:%lu\n",
         flags, entry, menu, fade, file, line);
  _kernel_oserror *e = pseudokern_fail(file, line);

  if (e == NULL)
    e = menu_set_fade(flags, menu, entry, fade);

  return e;
}

_kernel_oserror *pseudo_menu_get_fade(unsigned int flags, ObjectId menu, ComponentId entry, int *fade, const char *file, unsigned long line)
{
  _kernel_oserror *e = pseudokern_fail(file, line);

  if (e == NULL)
    e = menu_get_fade(flags, menu, entry, fade);

  return e;
}

_kernel_oserror *pseudo_menu_add_entry(unsigned int flags, ObjectId menu, ComponentId at_entry, char *entry_description, ComponentId *new_entry, const char *file, unsigned long line)
{
  _kernel_oserror *e = pseudokern_fail(file, line);

  if (e == NULL)
    e = menu_add_entry(flags, menu, at_entry, entry_description, new_entry);

  return e;
}

_kernel_oserror *pseudo_menu_set_entry_text(unsigned int flags, ObjectId menu, ComponentId entry, char *text, const char *file, unsigned long line)
{
  _kernel_oserror *e = pseudokern_fail(file, line);

  if (e == NULL)
    e = menu_set_entry_text(flags, menu, entry, text);

  return e;
}

_kernel_oserror *pseudo_quit_set_message(unsigned int flags, ObjectId quit, char *message, const char *file, unsigned long line)
{
  _kernel_oserror *e = pseudokern_fail(file, line);

  if (e == NULL)
    e = quit_set_message(flags, quit, message);

  return e;
}

_kernel_oserror *pseudo_colourdbox_get_wimp_handle(unsigned int flags, ObjectId colourdbox, int *wimp_handle, const char *file, unsigned long line)
{
  _kernel_oserror *e = pseudokern_fail(file, line);

  if (e == NULL)
    e = colourdbox_get_wimp_handle(flags, colourdbox, wimp_handle);

  return e;
}

_kernel_oserror *pseudo_fileinfo_get_window_id(unsigned int flags, ObjectId fileinfo, ObjectId *window, const char *file, unsigned long line)
{
  _kernel_oserror *e = pseudokern_fail(file, line);

  if (e == NULL)
    e = fileinfo_get_window_id(flags, fileinfo, window);

  return e;
}

_kernel_oserror *pseudo_proginfo_get_window_id(unsigned int flags, ObjectId proginfo, ObjectId *window, const char *file, unsigned long line)
{
  _kernel_oserror *e = pseudokern_fail(file, line);

  if (e == NULL)
    e = proginfo_get_window_id(flags, proginfo, window);

  return e;
}

_kernel_oserror *pseudo_scale_get_window_id(unsigned int flags, ObjectId scale, ObjectId *window, const char *file, unsigned long line)
{
  _kernel_oserror *e = pseudokern_fail(file, line);

  if (e == NULL)
    e = scale_get_window_id(flags, scale, window);

  return e;
}

_kernel_oserror *pseudo_fontdbox_get_window_id(unsigned int flags, ObjectId fontdbox, ObjectId *window, const char *file, unsigned long line)
{
  _kernel_oserror *e = pseudokern_fail(file, line);

  if (e == NULL)
    e = fontdbox_get_window_id(flags, fontdbox, window);

  return e;
}

_kernel_oserror *pseudo_quit_get_window_id(unsigned int flags, ObjectId quit, ObjectId *window, const char *file, unsigned long line)
{
  _kernel_oserror *e = pseudokern_fail(file, line);

  if (e == NULL)
    e = quit_get_window_id(flags, quit, window);

  return e;
}

_kernel_oserror *pseudo_dcs_get_window_id(unsigned int flags, ObjectId dcs, ObjectId *window, const char *file, unsigned long line)
{
  _kernel_oserror *e = pseudokern_fail(file, line);

  if (e == NULL)
    e = dcs_get_window_id(flags, dcs, window);

  return e;
}

_kernel_oserror *pseudo_printdbox_get_window_id(unsigned int flags, ObjectId printdbox, ObjectId *window, const char *file, unsigned long line)
{
  _kernel_oserror *e = pseudokern_fail(file, line);

  if (e == NULL)
    e = printdbox_get_window_id(flags, printdbox, window);

  return e;
}
