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
 * NeoMutt is built from a set of nested windows.  Each window defines a region
 * of the screen which is responsible for a single concept.  This could be a high-level
 * component like the @ref compose_dialog, or a single element like the @ref index_ibar.
 *
 * The **Root Window** is (grand-)parent of all those windows.
 *
 * The Root Window is container window and not visible.
 *
 * ### Definitions
 *
 * Every window in the hierarchy is struct MuttWindow, however in these docs
 * they're often given different descriptions.
 *
 * - **Window**:
 *   A region of the screen.  A Window can be: fixed size; set to maximise
 *   (as limited by its parent); set to minimise (around its children).
 *   Everything below is also a Window.
 *
 * - **Dialog**:
 *   A set of nested Windows that form an interactive component.  This is the
 *   main way that users interact with NeoMutt. e.g. @ref index_dlg_index,
 *   @ref compose_dialog.
 *
 * - **Panel**
 *   A small sub-division of a Dialog.  The Panels are sets of Windows that can
 *   be reused in other Dialogs.
 *
 * - **Container**:
 *   An invisible non-interactive Window used for shaping, aligning or limiting
 *   the size of its children.
 *
 * - **Bar**:
 *   A one-line high Window used for displaying help or status info, e.g.
 *   @ref helpbar_helpbar, @ref index_ibar.
 *
 * ## Windows
 *
 * | Name        | Type     | Constructor   |
 * | :---------- | :------- | :------------ |
 * | Root Window | #WT_ROOT | rootwin_new() |
 *
 * **Parent**
 * - None
 *
 * **Children**
 * - @ref helpbar_helpbar
 * - @ref gui_dialog
 * - @ref gui_msgwin
 *
 * ## Data
 *
 * The Root Window has no data.
 *
 * ## Events
 *
 * Once constructed, it is controlled by the following events:
 *
 * | Event Type  | Handler                                             |
 * | :---------- | :-------------------------------------------------- |
 * | #NT_CONFIG  | rootwin_config_observer()                           |
 * | #NT_WINDOW  | rootwin_window_observer()                           |
 * | SIGWINCH    | rootwin_set_size() (called by mutt_resize_screen()) |
 *
 * The Root Window does not implement MuttWindow::recalc() or MuttWindow::repaint().
 */

#include "config.h"
#include <stdbool.h>
#include <string.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "helpbar/lib.h"
#include "dialog.h"
#include "msgcont.h"
#include "msgwin.h"
#include "mutt_window.h"

struct MuttWindow *RootWindow = NULL; ///< Parent of all Windows

/**
 * rootwin_config_observer - Notification that a Config Variable has changed - Implements ::observer_t - @ingroup observer_api
 *
 * The Root Window is affected by changes to `$status_on_top`.
 */
static int rootwin_config_observer(struct NotifyCallback *nc)
{
  if ((nc->event_type != NT_CONFIG) || !nc->global_data || !nc->event_data)
    return -1;

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
 * rootwin_window_observer - Notification that a Window has changed - Implements ::observer_t - @ingroup observer_api
 *
 * This function is triggered by changes to the windows.
 *
 * - Delete (this window): clean up the resources held by the Root Window
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

  notify_observer_remove(win_root->notify, rootwin_window_observer, win_root);
  if (NeoMutt)
    notify_observer_remove(NeoMutt->notify, rootwin_config_observer, win_root);

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
  struct MuttWindow *win_root = mutt_window_new(WT_ROOT, MUTT_WIN_ORIENT_VERTICAL,
                                                MUTT_WIN_SIZE_FIXED, 0, 0);
  notify_set_parent(win_root->notify, NeoMutt->notify);
  RootWindow = win_root;

  struct MuttWindow *win_helpbar = helpbar_new();
  struct MuttWindow *win_alldlgs = alldialogs_new();

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

  struct MuttWindow *win_cont = msgcont_new();
  struct MuttWindow *win_msg = msgwin_new();
  mutt_window_add_child(win_cont, win_msg);
  mutt_window_add_child(win_root, win_cont);

  notify_observer_add(NeoMutt->notify, NT_CONFIG, rootwin_config_observer, win_root);
  notify_observer_add(win_root->notify, NT_WINDOW, rootwin_window_observer, win_root);
}

/**
 * rootwin_set_size - Set the dimensions of the Root Window
 * @param rows Number of rows on the screen
 * @param cols Number of columns on the screen
 *
 * This function is called after NeoMutt receives a SIGWINCH signal.
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
