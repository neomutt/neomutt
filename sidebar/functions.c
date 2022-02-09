/**
 * @file
 * Sidebar functions
 *
 * @authors
 * Copyright (C) 2020 Richard Russon <rich@flatcap.org>
 *
 * @copyright
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @page sidebar_functions Sidebar functions
 *
 * Sidebar functions
 */

#include "config.h"
#include <stddef.h>
#include <stdbool.h>
#include "private.h"
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "lib.h"
#include "opcodes.h"

/**
 * sb_next - Find the next unhidden Mailbox
 * @param wdata Sidebar data
 * @retval bool true if found
 */
bool sb_next(struct SidebarWindowData *wdata)
{
  struct SbEntry **sbep = NULL;
  ARRAY_FOREACH_FROM(sbep, &wdata->entries, wdata->hil_index + 1)
  {
    if (!(*sbep)->is_hidden)
    {
      wdata->hil_index = ARRAY_FOREACH_IDX;
      return true;
    }
  }

  return false;
}

/**
 * sb_next_new - Return the next mailbox with new messages
 * @param wdata Sidebar data
 * @param begin Starting index for searching
 * @param end   Ending index for searching
 * @retval ptr  Pointer to the first entry with new messages
 * @retval NULL None could be found
 */
static struct SbEntry **sb_next_new(struct SidebarWindowData *wdata, size_t begin, size_t end)
{
  struct SbEntry **sbep = NULL;
  ARRAY_FOREACH_FROM_TO(sbep, &wdata->entries, begin, end)
  {
    if ((*sbep)->mailbox->has_new || ((*sbep)->mailbox->msg_unread != 0))
      return sbep;
  }
  return NULL;
}

/**
 * sb_prev - Find the previous unhidden Mailbox
 * @param wdata Sidebar data
 * @retval bool true if found
 */
bool sb_prev(struct SidebarWindowData *wdata)
{
  struct SbEntry **sbep = NULL, **prev = NULL;
  ARRAY_FOREACH_TO(sbep, &wdata->entries, wdata->hil_index)
  {
    if (!(*sbep)->is_hidden)
      prev = sbep;
  }

  if (prev)
  {
    wdata->hil_index = ARRAY_IDX(&wdata->entries, prev);
    return true;
  }

  return false;
}

/**
 * sb_prev_new - Return the previous mailbox with new messages
 * @param wdata Sidebar data
 * @param begin Starting index for searching
 * @param end   Ending index for searching
 * @retval ptr  Pointer to the first entry with new messages
 * @retval NULL None could be found
 */
static struct SbEntry **sb_prev_new(struct SidebarWindowData *wdata, size_t begin, size_t end)
{
  struct SbEntry **sbep = NULL, **prev = NULL;
  ARRAY_FOREACH_FROM_TO(sbep, &wdata->entries, begin, end)
  {
    if ((*sbep)->mailbox->has_new || ((*sbep)->mailbox->msg_unread != 0))
      prev = sbep;
  }

  return prev;
}

// -----------------------------------------------------------------------------

/**
 * op_sidebar_first - Selects the first unhidden mailbox
 * @param wdata Sidebar data
 * @retval true The selection changed
 */
static bool op_sidebar_first(struct SidebarWindowData *wdata)
{
  if (ARRAY_EMPTY(&wdata->entries) || (wdata->hil_index < 0))
    return false;

  int orig_hil_index = wdata->hil_index;

  wdata->hil_index = 0;
  if ((*ARRAY_GET(&wdata->entries, wdata->hil_index))->is_hidden)
    if (!sb_next(wdata))
      wdata->hil_index = orig_hil_index;

  return (orig_hil_index != wdata->hil_index);
}

/**
 * op_sidebar_last - Selects the last unhidden mailbox
 * @param wdata Sidebar data
 * @retval true The selection changed
 */
static bool op_sidebar_last(struct SidebarWindowData *wdata)
{
  if (ARRAY_EMPTY(&wdata->entries) || (wdata->hil_index < 0))
    return false;

  int orig_hil_index = wdata->hil_index;

  wdata->hil_index = ARRAY_SIZE(&wdata->entries);
  if (!sb_prev(wdata))
    wdata->hil_index = orig_hil_index;

  return (orig_hil_index != wdata->hil_index);
}

/**
 * op_sidebar_next - Selects the next unhidden mailbox
 * @param wdata Sidebar data
 * @retval true The selection changed
 */
static bool op_sidebar_next(struct SidebarWindowData *wdata)
{
  if (ARRAY_EMPTY(&wdata->entries) || (wdata->hil_index < 0))
    return false;

  if (!sb_next(wdata))
    return false;

  return true;
}

/**
 * op_sidebar_next_new - Selects the next new mailbox
 * @param wdata         Sidebar data
 * @param next_new_wrap Wrap around when searching for the next mailbox with new mail
 * @retval true The selection changed
 *
 * Search down the list of mail folders for one containing new mail.
 */
static bool op_sidebar_next_new(struct SidebarWindowData *wdata, bool next_new_wrap)
{
  const size_t max_entries = ARRAY_SIZE(&wdata->entries);

  if ((max_entries == 0) || (wdata->hil_index < 0))
    return false;

  struct SbEntry **sbep = NULL;
  if ((sbep = sb_next_new(wdata, wdata->hil_index + 1, max_entries)) ||
      (next_new_wrap && (sbep = sb_next_new(wdata, 0, wdata->hil_index))))
  {
    wdata->hil_index = ARRAY_IDX(&wdata->entries, sbep);
    return true;
  }

  return false;
}

/**
 * op_sidebar_page_down - Selects the first entry in the next page of mailboxes
 * @param wdata Sidebar data
 * @retval true The selection changed
 */
static bool op_sidebar_page_down(struct SidebarWindowData *wdata)
{
  if (ARRAY_EMPTY(&wdata->entries) || (wdata->bot_index < 0))
    return false;

  int orig_hil_index = wdata->hil_index;

  wdata->hil_index = wdata->bot_index;
  sb_next(wdata);
  /* If the rest of the entries are hidden, go up to the last unhidden one */
  if ((*ARRAY_GET(&wdata->entries, wdata->hil_index))->is_hidden)
    sb_prev(wdata);

  return (orig_hil_index != wdata->hil_index);
}

/**
 * op_sidebar_page_up - Selects the last entry in the previous page of mailboxes
 * @param wdata Sidebar data
 * @retval true The selection changed
 */
static bool op_sidebar_page_up(struct SidebarWindowData *wdata)
{
  if (ARRAY_EMPTY(&wdata->entries) || (wdata->top_index < 0))
    return false;

  int orig_hil_index = wdata->hil_index;

  wdata->hil_index = wdata->top_index;
  sb_prev(wdata);
  /* If the rest of the entries are hidden, go down to the last unhidden one */
  if ((*ARRAY_GET(&wdata->entries, wdata->hil_index))->is_hidden)
    sb_next(wdata);

  return (orig_hil_index != wdata->hil_index);
}

/**
 * op_sidebar_prev - Selects the previous unhidden mailbox
 * @param wdata Sidebar data
 * @retval true The selection changed
 */
static bool op_sidebar_prev(struct SidebarWindowData *wdata)
{
  if (ARRAY_EMPTY(&wdata->entries) || (wdata->hil_index < 0))
    return false;

  if (!sb_prev(wdata))
    return false;

  return true;
}

/**
 * op_sidebar_prev_new - Selects the previous new mailbox
 * @param wdata         Sidebar data
 * @param next_new_wrap Wrap around when searching for the next mailbox with new mail
 * @retval true The selection changed
 *
 * Search up the list of mail folders for one containing new mail.
 */
static bool op_sidebar_prev_new(struct SidebarWindowData *wdata, bool next_new_wrap)
{
  const size_t max_entries = ARRAY_SIZE(&wdata->entries);

  if ((max_entries == 0) || (wdata->hil_index < 0))
    return false;

  struct SbEntry **sbep = NULL;
  if ((sbep = sb_prev_new(wdata, 0, wdata->hil_index)) ||
      (next_new_wrap && (sbep = sb_prev_new(wdata, wdata->hil_index + 1, max_entries))))
  {
    wdata->hil_index = ARRAY_IDX(&wdata->entries, sbep);
    return true;
  }

  return false;
}

/**
 * sb_change_mailbox - Perform a Sidebar function
 * @param win Sidebar Window
 * @param op  Operation to perform, e.g. OP_SIDEBAR_NEXT_NEW
 */
void sb_change_mailbox(struct MuttWindow *win, int op)
{
  if (!mutt_window_is_visible(win))
    return;

  struct SidebarWindowData *wdata = sb_wdata_get(win);
  if (!wdata)
    return;

  if (wdata->hil_index < 0) /* It'll get reset on the next draw */
    return;

  bool changed = false;
  const bool c_sidebar_next_new_wrap =
      cs_subset_bool(NeoMutt->sub, "sidebar_next_new_wrap");
  switch (op)
  {
    case OP_SIDEBAR_FIRST:
      changed = op_sidebar_first(wdata);
      break;
    case OP_SIDEBAR_LAST:
      changed = op_sidebar_last(wdata);
      break;
    case OP_SIDEBAR_NEXT:
      changed = op_sidebar_next(wdata);
      break;
    case OP_SIDEBAR_NEXT_NEW:
      changed = op_sidebar_next_new(wdata, c_sidebar_next_new_wrap);
      break;
    case OP_SIDEBAR_PAGE_DOWN:
      changed = op_sidebar_page_down(wdata);
      break;
    case OP_SIDEBAR_PAGE_UP:
      changed = op_sidebar_page_up(wdata);
      break;
    case OP_SIDEBAR_PREV:
      changed = op_sidebar_prev(wdata);
      break;
    case OP_SIDEBAR_PREV_NEW:
      changed = op_sidebar_prev_new(wdata, c_sidebar_next_new_wrap);
      break;
    default:
      return;
  }
  if (changed)
    win->actions |= WA_RECALC;
}
