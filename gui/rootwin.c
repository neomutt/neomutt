/**
 * @file
 * Root Window
 *
 * @authors
 * Copyright (C) 2021 Richard Russon <rich@flatcap.org>
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
 * @page gui_rootwin Root Window
 *
 * Root Window
 */

#include "config.h"
#include <stdbool.h>
#include <string.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "helpbar/lib.h"
#include "msgwin.h"
#include "mutt_window.h"

struct MuttWindow *RootWindow = NULL; ///< Parent of all Windows

/**
 * rootwin_config_observer - Listen for config changes affecting the Root Window - Implements ::observer_t
 */
static int rootwin_config_observer(struct NotifyCallback *nc)
{
  if (!nc->event_data || !nc->global_data)
    return -1;
  if (nc->event_type != NT_CONFIG)
    return 0;

  struct EventConfig *ev_c = nc->event_data;
  struct MuttWindow *root_win = nc->global_data;

  if (mutt_str_equal(ev_c->name, "status_on_top"))
  {
    struct MuttWindow *first = TAILQ_FIRST(&root_win->children);
    if (!first)
      return -1;

    mutt_debug(LL_DEBUG5, "config: '%s'\n", ev_c->name);
    const bool c_status_on_top = cs_subset_bool(NeoMutt->sub, "status_on_top");
    if ((c_status_on_top && (first->type == WT_HELP_BAR)) ||
        (!c_status_on_top && (first->type != WT_HELP_BAR)))
    {
      // Swap the HelpBar and the AllDialogsWindow
      struct MuttWindow *next = TAILQ_NEXT(first, entries);
      if (!next)
        return -1;
      TAILQ_REMOVE(&root_win->children, next, entries);
      TAILQ_INSERT_HEAD(&root_win->children, next, entries);

      mutt_debug(LL_DEBUG5, "config done, request WA_REFLOW\n");
    }
  }

  mutt_window_reflow(root_win);
  return 0;
}

/**
 * rootwin_free - Free all the default Windows
 */
void rootwin_free(void)
{
  if (NeoMutt)
    notify_observer_remove(NeoMutt->notify, rootwin_config_observer, RootWindow);
  AllDialogsWindow = NULL;
  mutt_window_free(&RootWindow);
}

/**
 * rootwin_new - Create the default Windows
 *
 * Create the Help, Index, Status, Message and Sidebar Windows.
 */
void rootwin_new(void)
{
  if (RootWindow)
    return;

  RootWindow =
      mutt_window_new(WT_ROOT, MUTT_WIN_ORIENT_VERTICAL, MUTT_WIN_SIZE_FIXED, 0, 0);
  notify_set_parent(RootWindow->notify, NeoMutt->notify);

  struct MuttWindow *win_helpbar = helpbar_new();

  AllDialogsWindow = mutt_window_new(WT_ALL_DIALOGS, MUTT_WIN_ORIENT_VERTICAL,
                                     MUTT_WIN_SIZE_MAXIMISE, MUTT_WIN_SIZE_UNLIMITED,
                                     MUTT_WIN_SIZE_UNLIMITED);

  struct MuttWindow *win_msg = msgwin_new();

  const bool c_status_on_top = cs_subset_bool(NeoMutt->sub, "status_on_top");
  if (c_status_on_top)
  {
    mutt_window_add_child(RootWindow, AllDialogsWindow);
    mutt_window_add_child(RootWindow, win_helpbar);
  }
  else
  {
    mutt_window_add_child(RootWindow, win_helpbar);
    mutt_window_add_child(RootWindow, AllDialogsWindow);
  }

  mutt_window_add_child(RootWindow, win_msg);
  notify_observer_add(NeoMutt->notify, NT_CONFIG, rootwin_config_observer, RootWindow);
}

/**
 * rootwin_set_size - Set the dimensions of the Root Window
 * @param rows
 * @param cols
 */
void rootwin_set_size(int cols, int rows)
{
  if (!RootWindow)
    return;

  bool changed = false;

  if (RootWindow->state.rows != rows)
  {
    RootWindow->state.rows = rows;
    changed = true;
  }

  if (RootWindow->state.cols != cols)
  {
    RootWindow->state.cols = cols;
    changed = true;
  }

  if (changed)
  {
    mutt_window_reflow(RootWindow);
  }
}
