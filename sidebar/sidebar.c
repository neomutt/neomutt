/**
 * @file
 * GUI display the mailboxes in a side panel
 *
 * @authors
 * Copyright (C) 2004 Justin Hibbits <jrh29@po.cwru.edu>
 * Copyright (C) 2004 Thomer M. Gil <mutt@thomer.com>
 * Copyright (C) 2015-2020 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2016-2017 Kevin J. McCarthy <kevin@8t8.us>
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
#include <stdio.h>
#include "private.h"
#include "mutt/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "lib.h"
#include "context.h"
#include "mutt_globals.h"

struct ListHead SidebarWhitelist = STAILQ_HEAD_INITIALIZER(SidebarWhitelist); ///< List of mailboxes to always display in the sidebar

/**
 * sb_get_highlight - Get the Mailbox that's highlighted in the sidebar
 * @param win Sidebar Window
 * @retval ptr Mailbox
 */
struct Mailbox *sb_get_highlight(struct MuttWindow *win)
{
  if (!C_SidebarVisible)
    return NULL;

  struct SidebarWindowData *wdata = sb_wdata_get(win);
  if (ARRAY_EMPTY(&wdata->entries) || (wdata->hil_index < 0))
    return NULL;

  return (*ARRAY_GET(&wdata->entries, wdata->hil_index))->mailbox;
}

/**
 * sb_notify_mailbox - The state of a Mailbox is about to change
 * @param wdata Sidebar data
 * @param m     Mailbox
 */
void sb_notify_mailbox(struct SidebarWindowData *wdata, struct Mailbox *m)
{
  if (!m)
    return;

  /* Any new/deleted mailboxes will cause a refresh.  As long as
   * they're valid, our pointers will be updated in prepare_sidebar() */

  struct SbEntry *entry = mutt_mem_calloc(1, sizeof(struct SbEntry));
  entry->mailbox = m;

  if (wdata->top_index < 0)
    wdata->top_index = ARRAY_SIZE(&wdata->entries);
  if (wdata->bot_index < 0)
    wdata->bot_index = ARRAY_SIZE(&wdata->entries);
  if ((wdata->opn_index < 0) && Context &&
      mutt_str_equal(m->realpath, Context->mailbox->realpath))
  {
    wdata->opn_index = ARRAY_SIZE(&wdata->entries);
  }

  ARRAY_ADD(&wdata->entries, entry);
}

/**
 * sb_init - Set up the Sidebar
 */
void sb_init(void)
{
  // Listen for dialog creation events
  notify_observer_add(AllDialogsWindow->notify, NT_WINDOW, sb_insertion_observer, NULL);
}

/**
 * sb_shutdown - Clean up the Sidebar
 */
void sb_shutdown(void)
{
  if (AllDialogsWindow)
    notify_observer_remove(AllDialogsWindow->notify, sb_insertion_observer, NULL);
  mutt_list_free(&SidebarWhitelist);
}
