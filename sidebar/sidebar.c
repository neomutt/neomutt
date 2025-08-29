/**
 * @file
 * GUI display the mailboxes in a side panel
 *
 * @authors
 * Copyright (C) 2016-2017 Kevin J. McCarthy <kevin@8t8.us>
 * Copyright (C) 2018-2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2022 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2023 Whitney Cumber
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
 * @page sidebar_sidebar GUI display the mailboxes in a side panel
 *
 * GUI display the mailboxes in a side panel
 */

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include "private.h"
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "lib.h"
#include "index/lib.h"

struct ListHead SidebarPinned = STAILQ_HEAD_INITIALIZER(SidebarPinned); ///< List of mailboxes to always display in the sidebar

/**
 * SbCommands - Sidebar Commands
 */
static const struct Command SbCommands[] = {
  // clang-format off
  { "sidebar_pin",   sb_parse_sidebar_pin,     0 },
  { "sidebar_unpin", sb_parse_sidebar_unpin,   0 },

  { "sidebar_whitelist",   sb_parse_sidebar_pin,     0 },
  { "unsidebar_whitelist", sb_parse_sidebar_unpin,   0 },
  // clang-format on
};

/**
 * sb_get_highlight - Get the Mailbox that's highlighted in the sidebar
 * @param win Sidebar Window
 * @retval ptr Mailbox
 */
struct Mailbox *sb_get_highlight(struct MuttWindow *win)
{
  const bool c_sidebar_visible = cs_subset_bool(NeoMutt->sub, "sidebar_visible");
  if (!c_sidebar_visible)
    return NULL;

  struct SidebarWindowData *wdata = sb_wdata_get(win);
  if (wdata->hil_index < 0)
    return NULL;

  struct SbEntry **sbep = ARRAY_GET(&wdata->entries, wdata->hil_index);
  if (!sbep)
    return NULL;

  return (*sbep)->mailbox;
}

/**
 * sb_add_mailbox - Add a Mailbox to the Sidebar
 * @param wdata Sidebar data
 * @param m     Mailbox to add
 *
 * The Sidebar will be re-sorted, and the indices updated, when sb_recalc() is
 * called.
 */
void sb_add_mailbox(struct SidebarWindowData *wdata, struct Mailbox *m)
{
  if (!m)
    return;

  struct SbEntry **sbep = NULL;
  ARRAY_FOREACH(sbep, &wdata->entries)
  {
    if ((*sbep)->mailbox == m)
      return;
  }

  /* Any new/deleted mailboxes will cause a refresh.  As long as
   * they're valid, our pointers will be updated in prepare_sidebar() */

  struct IndexSharedData *shared = wdata->shared;
  struct SbEntry *entry = MUTT_MEM_CALLOC(1, struct SbEntry);
  entry->mailbox = m;

  if (wdata->top_index < 0)
    wdata->top_index = ARRAY_SIZE(&wdata->entries);
  if (wdata->bot_index < 0)
    wdata->bot_index = ARRAY_SIZE(&wdata->entries);
  if ((wdata->opn_index < 0) && shared->mailbox &&
      mutt_str_equal(m->realpath, shared->mailbox->realpath))
  {
    wdata->opn_index = ARRAY_SIZE(&wdata->entries);
  }

  ARRAY_ADD(&wdata->entries, entry);
}

/**
 * sb_remove_mailbox - Remove a Mailbox from the Sidebar
 * @param wdata Sidebar data
 * @param m     Mailbox to remove
 */
void sb_remove_mailbox(struct SidebarWindowData *wdata, const struct Mailbox *m)
{
  struct SbEntry **sbep = NULL;
  ARRAY_FOREACH(sbep, &wdata->entries)
  {
    if ((*sbep)->mailbox != m)
      continue;

    struct SbEntry *sbe_remove = *sbep;
    ARRAY_REMOVE(&wdata->entries, sbep);
    FREE(&sbe_remove);

    if (wdata->opn_index == ARRAY_FOREACH_IDX)
    {
      // Open item was deleted
      wdata->opn_index = -1;
    }
    else if ((wdata->opn_index > 0) && (wdata->opn_index > ARRAY_FOREACH_IDX))
    {
      // Open item is still visible, so adjust the index
      wdata->opn_index--;
    }

    if (wdata->hil_index == ARRAY_FOREACH_IDX)
    {
      // If possible, keep the highlight where it is
      struct SbEntry **sbep_cur = ARRAY_GET(&wdata->entries, ARRAY_FOREACH_IDX);
      if (!sbep_cur)
      {
        // The last entry was deleted, so backtrack
        sb_prev(wdata);
      }
      else if ((*sbep)->is_hidden)
      {
        // Find the next unhidden entry, or the previous
        if (!sb_next(wdata) && !sb_prev(wdata))
          wdata->hil_index = -1;
      }
    }
    else if ((wdata->hil_index > 0) && (wdata->hil_index > ARRAY_FOREACH_IDX))
    {
      // Highlighted item is still visible, so adjust the index
      wdata->hil_index--;
    }
    break;
  }
}

/**
 * sb_set_current_mailbox - Set the current Mailbox
 * @param wdata Sidebar data
 * @param m     Mailbox
 */
void sb_set_current_mailbox(struct SidebarWindowData *wdata, struct Mailbox *m)
{
  wdata->opn_index = -1;

  struct SbEntry **sbep = NULL;
  ARRAY_FOREACH(sbep, &wdata->entries)
  {
    if (m && m->visible)
    {
      if (mutt_str_equal((*sbep)->mailbox->realpath, m->realpath))
      {
        wdata->opn_index = ARRAY_FOREACH_IDX;
        wdata->hil_index = ARRAY_FOREACH_IDX;
        break;
      }
    }
    (*sbep)->is_hidden = !(*sbep)->mailbox->visible;
  }
}

/**
 * sb_init - Set up the Sidebar
 */
void sb_init(void)
{
  commands_register(SbCommands, mutt_array_size(SbCommands));

  // Listen for dialog creation events
  notify_observer_add(AllDialogsWindow->notify, NT_WINDOW,
                      sb_insertion_window_observer, NULL);
}

/**
 * sb_cleanup - Clean up the Sidebar
 */
void sb_cleanup(void)
{
  if (AllDialogsWindow)
    notify_observer_remove(AllDialogsWindow->notify, sb_insertion_window_observer, NULL);
  mutt_list_free(&SidebarPinned);
}
