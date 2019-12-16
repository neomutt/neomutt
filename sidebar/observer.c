/**
 * @file
 * Sidebar observers
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
 * @page sidebar_observers Sidebar observers
 *
 * Sidebar observers
 */

#include "config.h"
#include <stdbool.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "gui/lib.h"
#include "lib.h"
#include "mutt_menu.h"

/**
 * sb_observer - Listen for config changes affecting the sidebar - Implements ::observer_t
 * @param nc Notification data
 * @retval bool True, if successful
 */
int sb_observer(struct NotifyCallback *nc)
{
  if (!nc->event_data || !nc->global_data)
    return -1;
  if (nc->event_type != NT_CONFIG)
    return 0;

  struct MuttWindow *win = nc->global_data;
  struct EventConfig *ec = nc->event_data;

  if (!mutt_strn_equal(ec->name, "sidebar_", 8))
    return 0;

  bool repaint = false;

  if (win->state.visible != C_SidebarVisible)
  {
    window_set_visible(win, C_SidebarVisible);
    repaint = true;
  }

  if (win->req_cols != C_SidebarWidth)
  {
    win->req_cols = C_SidebarWidth;
    repaint = true;
  }

  struct MuttWindow *parent = win->parent;
  struct MuttWindow *first = TAILQ_FIRST(&parent->children);

  if ((C_SidebarOnRight && (first == win)) || (!C_SidebarOnRight && (first != win)))
  {
    // Swap the Sidebar and the Container of the Index/Pager
    TAILQ_REMOVE(&parent->children, first, entries);
    TAILQ_INSERT_TAIL(&parent->children, first, entries);
    repaint = true;
  }

  if (repaint)
  {
    mutt_debug(LL_NOTIFY, "repaint sidebar\n");
    mutt_window_reflow(MuttDialogWindow);
    mutt_menu_set_current_redraw_full();
  }

  return 0;
}

/**
 * sb_insertion_observer - Listen for new Dialogs - Implements ::observer_t
 */
int sb_insertion_observer(struct NotifyCallback *nc)
{
  if ((nc->event_type != NT_WINDOW) || (nc->event_subtype != NT_WINDOW_DIALOG))
    return 0;

  struct EventWindow *ew = nc->event_data;
  if (ew->win->type != WT_DLG_INDEX)
    return 0;

  if (ew->flags & WN_VISIBLE)
    sb_win_init(ew->win);
  else if (ew->flags & WN_HIDDEN)
    sb_win_shutdown(ew->win);

  return 0;
}
