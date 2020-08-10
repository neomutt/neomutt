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
#include "core/lib.h"
#include "lib.h"
#include "mutt_menu.h"
#include "opcodes.h"

struct MuttWindow;

/**
 * select_next - Selects the next unhidden mailbox
 * @param wdata Sidebar data
 * @retval bool true if the selection changed
 */
bool select_next(struct SidebarWindowData *wdata)
{
  if (ARRAY_EMPTY(&wdata->entries) || (wdata->hil_index < 0))
    return false;

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
 * next_new - Return the next mailbox with new messages
 * @param wdata SidebarWindowData struct
 * @param begin Starting index for searching
 * @param end   Ending index for searching
 * @retval sbe  Pointer to the first entry with new messages
 * @retval NULL None could be found
 */
static struct SbEntry **next_new(struct SidebarWindowData *wdata, size_t begin, size_t end)
{
  struct SbEntry **sbep = NULL;
  ARRAY_FOREACH_FROM_TO(sbep, &wdata->entries, begin, end)
  {
    if ((*sbep)->mailbox->has_new && (*sbep)->mailbox->msg_unread != 0)
      return sbep;
  }
  return NULL;
}

/**
 * select_next_new - Selects the next new mailbox
 * @param wdata Sidebar data
 * @retval bool true if the selection changed
 *
 * Search down the list of mail folders for one containing new mail.
 */
static bool select_next_new(struct SidebarWindowData *wdata)
{
  const size_t max_entries = ARRAY_SIZE(&wdata->entries);

  if ((max_entries == 0) || (wdata->hil_index < 0))
    return false;

  struct SbEntry **sbep = NULL;
  if ((sbep = next_new(wdata, wdata->hil_index + 1, max_entries)) ||
      (C_SidebarNextNewWrap && (sbep = next_new(wdata, 0, wdata->hil_index))))
  {
    wdata->hil_index = ARRAY_IDX(&wdata->entries, sbep);
    return true;
  }

  return false;
}

/**
 * select_prev - Selects the previous unhidden mailbox
 * @param wdata Sidebar data
 * @retval bool true if the selection changed
 */
static bool select_prev(struct SidebarWindowData *wdata)
{
  if (ARRAY_EMPTY(&wdata->entries) || (wdata->hil_index < 0))
    return false;

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
 * prev_new - Return the previous mailbox with new messages
 * @param wdata SidebarWindowData struct
 * @param begin Starting index for searching
 * @param end   Ending index for searching
 * @retval sbe  Pointer to the first entry with new messages
 * @retval NULL None could be found
 */
static struct SbEntry **prev_new(struct SidebarWindowData *wdata, size_t begin, size_t end)
{
  struct SbEntry **sbep = NULL, **prev = NULL;
  ARRAY_FOREACH_FROM_TO(sbep, &wdata->entries, begin, end)
  {
    if ((*sbep)->mailbox->has_new && (*sbep)->mailbox->msg_unread != 0)
      prev = sbep;
  }

  return prev;
}

/**
 * select_prev_new - Selects the previous new mailbox
 * @param wdata Sidebar data
 * @retval bool true if the selection changed
 *
 * Search up the list of mail folders for one containing new mail.
 */
static bool select_prev_new(struct SidebarWindowData *wdata)
{
  const size_t max_entries = ARRAY_SIZE(&wdata->entries);

  if ((max_entries == 0) || (wdata->hil_index < 0))
    return false;

  struct SbEntry **sbep = NULL;
  if ((sbep = prev_new(wdata, 0, wdata->hil_index)) ||
      (C_SidebarNextNewWrap && (sbep = prev_new(wdata, wdata->hil_index + 1, max_entries))))
  {
    wdata->hil_index = ARRAY_IDX(&wdata->entries, sbep);
    return true;
  }

  return false;
}

/**
 * select_page_down - Selects the first entry in the next page of mailboxes
 * @param wdata Sidebar data
 * @retval bool true if the selection changed
 */
static bool select_page_down(struct SidebarWindowData *wdata)
{
  if (ARRAY_EMPTY(&wdata->entries) || (wdata->bot_index < 0))
    return false;

  int orig_hil_index = wdata->hil_index;

  wdata->hil_index = wdata->bot_index;
  select_next(wdata);
  /* If the rest of the entries are hidden, go up to the last unhidden one */
  if ((*ARRAY_GET(&wdata->entries, wdata->hil_index))->is_hidden)
    select_prev(wdata);

  return (orig_hil_index != wdata->hil_index);
}

/**
 * select_page_up - Selects the last entry in the previous page of mailboxes
 * @param wdata Sidebar data
 * @retval bool true if the selection changed
 */
static bool select_page_up(struct SidebarWindowData *wdata)
{
  if (ARRAY_EMPTY(&wdata->entries) || (wdata->top_index < 0))
    return false;

  int orig_hil_index = wdata->hil_index;

  wdata->hil_index = wdata->top_index;
  select_prev(wdata);
  /* If the rest of the entries are hidden, go down to the last unhidden one */
  if ((*ARRAY_GET(&wdata->entries, wdata->hil_index))->is_hidden)
    select_next(wdata);

  return (orig_hil_index != wdata->hil_index);
}

/**
 * select_first - Selects the first unhidden mailbox
 * @param wdata Sidebar data
 * @retval bool true if the selection changed
 */
static bool select_first(struct SidebarWindowData *wdata)
{
  if (ARRAY_EMPTY(&wdata->entries) || (wdata->hil_index < 0))
    return false;

  int orig_hil_index = wdata->hil_index;

  wdata->hil_index = 0;
  if ((*ARRAY_GET(&wdata->entries, wdata->hil_index))->is_hidden)
    if (!select_next(wdata))
      wdata->hil_index = orig_hil_index;

  return (orig_hil_index != wdata->hil_index);
}

/**
 * select_last - Selects the last unhidden mailbox
 * @retval true  Success
 * @retval false Failure
 */
static bool select_last(struct SidebarWindowData *wdata)
{
  if (ARRAY_EMPTY(&wdata->entries) || (wdata->hil_index < 0))
    return false;

  int orig_hil_index = wdata->hil_index;

  wdata->hil_index = ARRAY_SIZE(&wdata->entries);
  if (!select_prev(wdata))
    wdata->hil_index = orig_hil_index;

  return (orig_hil_index != wdata->hil_index);
}

/**
 * sb_change_mailbox - Perform a Sidebar function
 * @param win Sidebar Window
 * @param op  Operation to perform, e.g. OP_SIDEBAR_NEXT_NEW
 */
void sb_change_mailbox(struct MuttWindow *win, int op)
{
  if (!C_SidebarVisible)
    return;

  struct SidebarWindowData *wdata = sb_wdata_get(win);
  if (!wdata)
    return;

  if (wdata->hil_index < 0) /* It'll get reset on the next draw */
    return;

  switch (op)
  {
    case OP_SIDEBAR_FIRST:
      if (!select_first(wdata))
        return;
      break;
    case OP_SIDEBAR_LAST:
      if (!select_last(wdata))
        return;
      break;
    case OP_SIDEBAR_NEXT:
      if (!select_next(wdata))
        return;
      break;
    case OP_SIDEBAR_NEXT_NEW:
      if (!select_next_new(wdata))
        return;
      break;
    case OP_SIDEBAR_PAGE_DOWN:
      if (!select_page_down(wdata))
        return;
      break;
    case OP_SIDEBAR_PAGE_UP:
      if (!select_page_up(wdata))
        return;
      break;
    case OP_SIDEBAR_PREV:
      if (!select_prev(wdata))
        return;
      break;
    case OP_SIDEBAR_PREV_NEW:
      if (!select_prev_new(wdata))
        return;
      break;
    default:
      return;
  }
  mutt_menu_set_current_redraw(REDRAW_SIDEBAR);
}
