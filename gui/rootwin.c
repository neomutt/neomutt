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
#include "dialog.h"
#include "msgwin.h"
#include "mutt_window.h"

struct MuttWindow *RootWindow = NULL; ///< Parent of all Windows

/**
 * rootwin_config_observer - Notification that a Config Variable has changed - Implements ::observer_t
 */
static int rootwin_config_observer(struct NotifyCallback *nc)
{
  if ((nc->event_type != NT_CONFIG) || !nc->global_data || !nc->event_data)
    return -1;

  if (nc->event_subtype == NT_CONFIG_INITIAL_SET)
    return 0;

  struct EventConfig *ev_c = nc->event_data;
  struct MuttWindow *win_root = nc->global_data;

  if (!mutt_str_equal(ev_c->name, "status_on_top"))
    return 0;

  struct MuttWindow *first = TAILQ_FIRST(&win_root->children);
  if (!first)
    return 0;

  const bool c_status_on_top = cs_subset_bool(NeoMutt->sub, "status_on_top");
  if ((c_status_on_top && (first->type == WT_HELP_BAR)) ||
      (!c_status_on_top && (first->type != WT_HELP_BAR)))
  {
    // Swap the HelpBar and the AllDialogsWindow
    struct MuttWindow *next = TAILQ_NEXT(first, entries);
    if (!next)
      return 0;
    TAILQ_REMOVE(&win_root->children, next, entries);
    TAILQ_INSERT_HEAD(&win_root->children, next, entries);

    mutt_window_reflow(win_root);
    mutt_debug(LL_DEBUG5, "config done, request WA_REFLOW\n");
  }

  return 0;
}

/**
 * rootwin_window_observer - Notification that a Window has changed - Implements ::observer_t
 */
static int rootwin_window_observer(struct NotifyCallback *nc)
{
  if ((nc->event_type != NT_WINDOW) || !nc->global_data || !nc->event_data)
    return -1;

  if (nc->event_subtype != NT_WINDOW_DELETE)
    return 0;

  struct MuttWindow *win_root = nc->global_data;
  struct EventWindow *ev_w = nc->event_data;
  if (ev_w->win != win_root)
    return 0;

  if (NeoMutt)
  {
    notify_observer_remove(NeoMutt->notify, rootwin_config_observer, win_root);
    notify_observer_remove(NeoMutt->notify, rootwin_window_observer, win_root);
  }

  mutt_debug(LL_DEBUG5, "window delete done\n");
  return 0;
}

/**
 * rootwin_free - Free all the default Windows
 */
void rootwin_free(void)
{
  mutt_window_free(&RootWindow);
}

/**
 * rootwin_new - Create the default Windows
 *
 * Create the Help, Index, Status, Message and Sidebar Windows.
 */
void rootwin_new(void)
{
  struct MuttWindow *win_root =
      mutt_window_new(WT_ROOT, MUTT_WIN_ORIENT_VERTICAL, MUTT_WIN_SIZE_FIXED, 0, 0);
  notify_set_parent(win_root->notify, NeoMutt->notify);

  struct MuttWindow *win_helpbar = helpbar_new();
  struct MuttWindow *win_alldlgs = alldialogs_new();
  struct MuttWindow *win_msg = msgwin_new();

  const bool c_status_on_top = cs_subset_bool(NeoMutt->sub, "status_on_top");
  if (c_status_on_top)
  {
    mutt_window_add_child(win_root, win_alldlgs);
    mutt_window_add_child(win_root, win_helpbar);
  }
  else
  {
    mutt_window_add_child(win_root, win_helpbar);
    mutt_window_add_child(win_root, win_alldlgs);
  }

  mutt_window_add_child(win_root, win_msg);

  notify_observer_add(NeoMutt->notify, NT_CONFIG, rootwin_config_observer, win_root);
  notify_observer_add(NeoMutt->notify, NT_WINDOW, rootwin_window_observer, win_root);

  RootWindow = win_root;
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
