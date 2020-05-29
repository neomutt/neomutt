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

/**
 * select_next - Selects the next unhidden mailbox
 * @retval true  Success
 * @retval false Failure
 */
bool select_next(void)
{
  if ((EntryCount == 0) || (HilIndex < 0))
    return false;

  int entry = HilIndex;

  do
  {
    entry++;
    if (entry == EntryCount)
      return false;
  } while (Entries[entry]->is_hidden);

  HilIndex = entry;
  return true;
}

/**
 * select_next_new - Selects the next new mailbox
 * @retval true  Success
 * @retval false Failure
 *
 * Search down the list of mail folders for one containing new mail.
 */
static bool select_next_new(void)
{
  if ((EntryCount == 0) || (HilIndex < 0))
    return false;

  int entry = HilIndex;

  do
  {
    entry++;
    if (entry == EntryCount)
    {
      if (C_SidebarNextNewWrap)
        entry = 0;
      else
        return false;
    }
    if (entry == HilIndex)
      return false;
  } while (!Entries[entry]->mailbox->has_new && (Entries[entry]->mailbox->msg_unread == 0));

  HilIndex = entry;
  return true;
}

/**
 * select_prev - Selects the previous unhidden mailbox
 * @retval true  Success
 * @retval false Failure
 */
static bool select_prev(void)
{
  if ((EntryCount == 0) || (HilIndex < 0))
    return false;

  int entry = HilIndex;

  do
  {
    entry--;
    if (entry < 0)
      return false;
  } while (Entries[entry]->is_hidden);

  HilIndex = entry;
  return true;
}

/**
 * select_prev_new - Selects the previous new mailbox
 * @retval true  Success
 * @retval false Failure
 *
 * Search up the list of mail folders for one containing new mail.
 */
static bool select_prev_new(void)
{
  if ((EntryCount == 0) || (HilIndex < 0))
    return false;

  int entry = HilIndex;

  do
  {
    entry--;
    if (entry < 0)
    {
      if (C_SidebarNextNewWrap)
        entry = EntryCount - 1;
      else
        return false;
    }
    if (entry == HilIndex)
      return false;
  } while (!Entries[entry]->mailbox->has_new && (Entries[entry]->mailbox->msg_unread == 0));

  HilIndex = entry;
  return true;
}

/**
 * select_page_down - Selects the first entry in the next page of mailboxes
 * @retval true  Success
 * @retval false Failure
 */
static bool select_page_down(void)
{
  if ((EntryCount == 0) || (BotIndex < 0))
    return false;

  int orig_hil_index = HilIndex;

  HilIndex = BotIndex;
  select_next();
  /* If the rest of the entries are hidden, go up to the last unhidden one */
  if (Entries[HilIndex]->is_hidden)
    select_prev();

  return (orig_hil_index != HilIndex);
}

/**
 * select_page_up - Selects the last entry in the previous page of mailboxes
 * @retval true  Success
 * @retval false Failure
 */
static bool select_page_up(void)
{
  if ((EntryCount == 0) || (TopIndex < 0))
    return false;

  int orig_hil_index = HilIndex;

  HilIndex = TopIndex;
  select_prev();
  /* If the rest of the entries are hidden, go down to the last unhidden one */
  if (Entries[HilIndex]->is_hidden)
    select_next();

  return (orig_hil_index != HilIndex);
}

/**
 * select_first - Selects the first unhidden mailbox
 * @retval true  Success
 * @retval false Failure
 */
static bool select_first(void)
{
  if ((EntryCount == 0) || (HilIndex < 0))
    return false;

  int orig_hil_index = HilIndex;

  HilIndex = 0;
  if (Entries[HilIndex]->is_hidden)
    if (!select_next())
      HilIndex = orig_hil_index;

  return (orig_hil_index != HilIndex);
}

/**
 * select_last - Selects the last unhidden mailbox
 * @retval true  Success
 * @retval false Failure
 */
static bool select_last(void)
{
  if ((EntryCount == 0) || (HilIndex < 0))
    return false;

  int orig_hil_index = HilIndex;

  HilIndex = EntryCount;
  if (!select_prev())
    HilIndex = orig_hil_index;

  return (orig_hil_index != HilIndex);
}

/**
 * sb_change_mailbox - Change the selected mailbox
 * @param op Operation code
 *
 * Change the selected mailbox, e.g. "Next mailbox", "Previous Mailbox
 * with new mail". The operations are listed in opcodes.h.
 *
 * If the operation is successful, HilMailbox will be set to the new mailbox.
 * This function only *selects* the mailbox, doesn't *open* it.
 *
 * Allowed values are: OP_SIDEBAR_FIRST, OP_SIDEBAR_LAST,
 * OP_SIDEBAR_NEXT, OP_SIDEBAR_NEXT_NEW,
 * OP_SIDEBAR_PAGE_DOWN, OP_SIDEBAR_PAGE_UP, OP_SIDEBAR_PREV,
 * OP_SIDEBAR_PREV_NEW.
 */
void sb_change_mailbox(int op)
{
  if (!C_SidebarVisible)
    return;

  if (HilIndex < 0) /* It'll get reset on the next draw */
    return;

  switch (op)
  {
    case OP_SIDEBAR_FIRST:
      if (!select_first())
        return;
      break;
    case OP_SIDEBAR_LAST:
      if (!select_last())
        return;
      break;
    case OP_SIDEBAR_NEXT:
      if (!select_next())
        return;
      break;
    case OP_SIDEBAR_NEXT_NEW:
      if (!select_next_new())
        return;
      break;
    case OP_SIDEBAR_PAGE_DOWN:
      if (!select_page_down())
        return;
      break;
    case OP_SIDEBAR_PAGE_UP:
      if (!select_page_up())
        return;
      break;
    case OP_SIDEBAR_PREV:
      if (!select_prev())
        return;
      break;
    case OP_SIDEBAR_PREV_NEW:
      if (!select_prev_new())
        return;
      break;
    default:
      return;
  }
  mutt_menu_set_current_redraw(REDRAW_SIDEBAR);
}
