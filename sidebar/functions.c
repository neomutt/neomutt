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
#include <stdbool.h>
#include "private.h"
#include "core/lib.h"
#include "lib.h"
#include "mutt_menu.h"
#include "opcodes.h"

struct MuttWindow;

/**
 * select_next - Selects the next unhidden mailbox
 * @retval true  Success
 * @retval false Failure
 */
bool select_next(struct SidebarWindowData *wdata)
{
  if ((wdata->entry_count == 0) || (wdata->hil_index < 0))
    return false;

  int entry = wdata->hil_index;

  do
  {
    entry++;
    if (entry == wdata->entry_count)
      return false;
  } while (wdata->entries[entry]->is_hidden);

  wdata->hil_index = entry;
  return true;
}

/**
 * select_next_new - Selects the next new mailbox
 * @retval true  Success
 * @retval false Failure
 *
 * Search down the list of mail folders for one containing new mail.
 */
static bool select_next_new(struct SidebarWindowData *wdata)
{
  if ((wdata->entry_count == 0) || (wdata->hil_index < 0))
    return false;

  int entry = wdata->hil_index;

  do
  {
    entry++;
    if (entry == wdata->entry_count)
    {
      if (C_SidebarNextNewWrap)
        entry = 0;
      else
        return false;
    }
    if (entry == wdata->hil_index)
      return false;
  } while (!wdata->entries[entry]->mailbox->has_new &&
           (wdata->entries[entry]->mailbox->msg_unread == 0));

  wdata->hil_index = entry;
  return true;
}

/**
 * select_prev - Selects the previous unhidden mailbox
 * @retval true  Success
 * @retval false Failure
 */
static bool select_prev(struct SidebarWindowData *wdata)
{
  if ((wdata->entry_count == 0) || (wdata->hil_index < 0))
    return false;

  int entry = wdata->hil_index;

  do
  {
    entry--;
    if (entry < 0)
      return false;
  } while (wdata->entries[entry]->is_hidden);

  wdata->hil_index = entry;
  return true;
}

/**
 * select_prev_new - Selects the previous new mailbox
 * @retval true  Success
 * @retval false Failure
 *
 * Search up the list of mail folders for one containing new mail.
 */
static bool select_prev_new(struct SidebarWindowData *wdata)
{
  if ((wdata->entry_count == 0) || (wdata->hil_index < 0))
    return false;

  int entry = wdata->hil_index;

  do
  {
    entry--;
    if (entry < 0)
    {
      if (C_SidebarNextNewWrap)
        entry = wdata->entry_count - 1;
      else
        return false;
    }
    if (entry == wdata->hil_index)
      return false;
  } while (!wdata->entries[entry]->mailbox->has_new &&
           (wdata->entries[entry]->mailbox->msg_unread == 0));

  wdata->hil_index = entry;
  return true;
}

/**
 * select_page_down - Selects the first entry in the next page of mailboxes
 * @retval true  Success
 * @retval false Failure
 */
static bool select_page_down(struct SidebarWindowData *wdata)
{
  if ((wdata->entry_count == 0) || (wdata->bot_index < 0))
    return false;

  int orig_hil_index = wdata->hil_index;

  wdata->hil_index = wdata->bot_index;
  select_next(wdata);
  /* If the rest of the entries are hidden, go up to the last unhidden one */
  if (wdata->entries[wdata->hil_index]->is_hidden)
    select_prev(wdata);

  return (orig_hil_index != wdata->hil_index);
}

/**
 * select_page_up - Selects the last entry in the previous page of mailboxes
 * @retval true  Success
 * @retval false Failure
 */
static bool select_page_up(struct SidebarWindowData *wdata)
{
  if ((wdata->entry_count == 0) || (wdata->top_index < 0))
    return false;

  int orig_hil_index = wdata->hil_index;

  wdata->hil_index = wdata->top_index;
  select_prev(wdata);
  /* If the rest of the entries are hidden, go down to the last unhidden one */
  if (wdata->entries[wdata->hil_index]->is_hidden)
    select_next(wdata);

  return (orig_hil_index != wdata->hil_index);
}

/**
 * select_first - Selects the first unhidden mailbox
 * @retval true  Success
 * @retval false Failure
 */
static bool select_first(struct SidebarWindowData *wdata)
{
  if ((wdata->entry_count == 0) || (wdata->hil_index < 0))
    return false;

  int orig_hil_index = wdata->hil_index;

  wdata->hil_index = 0;
  if (wdata->entries[wdata->hil_index]->is_hidden)
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
  if ((wdata->entry_count == 0) || (wdata->hil_index < 0))
    return false;

  int orig_hil_index = wdata->hil_index;

  wdata->hil_index = wdata->entry_count;
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
