/**
 * @file
 * Dialog Windows
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
 * @page gui_dialog Dialog Windows
 *
 * Dialog Windows
 */

#include "config.h"
#include <stddef.h>
#include <stdbool.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "debug/lib.h"
#include "lib.h"
#include "mutt_menu.h"

/**
 * dialog_find - Find the parent Dialog of a Window
 * @param win Window
 * @retval ptr Dialog
 *
 * Dialog Windows will be owned by a MuttWindow of type #WT_ALL_DIALOGS.
 */
struct MuttWindow *dialog_find(struct MuttWindow *win)
{
  for (; win && win->parent; win = win->parent)
  {
    if (win->parent->type == WT_ALL_DIALOGS)
      return win;
  }

  return NULL;
}

/**
 * dialog_push - Display a Window to the user
 * @param dlg Window to display
 *
 * The Dialog Windows are kept in a stack.
 * The topmost is visible to the user, whilst the others are hidden.
 *
 * When a Window is pushed, the old Window is marked as not visible.
 */
void dialog_push(struct MuttWindow *dlg)
{
  if (!dlg || !AllDialogsWindow)
    return;

  struct MuttWindow *last = TAILQ_LAST(&AllDialogsWindow->children, MuttWindowList);
  if (last)
    last->state.visible = false;

  TAILQ_INSERT_TAIL(&AllDialogsWindow->children, dlg, entries);
  notify_set_parent(dlg->notify, AllDialogsWindow->notify);

  // Notify the world, allowing plugins to integrate
  struct EventWindow ev_w = { dlg, WN_VISIBLE };
  notify_send(dlg->notify, NT_WINDOW, NT_WINDOW_DIALOG, &ev_w);

  dlg->state.visible = true;
  dlg->parent = AllDialogsWindow;
  mutt_window_reflow(AllDialogsWindow);
  window_set_focus(dlg);

#ifdef USE_DEBUG_WINDOW
  debug_win_dump();
#endif
}

/**
 * dialog_pop - Hide a Window from the user
 *
 * The topmost (visible) Window is removed from the stack and the next Window
 * is marked as visible.
 */
void dialog_pop(void)
{
  if (!AllDialogsWindow)
    return;

  struct MuttWindow *last = TAILQ_LAST(&AllDialogsWindow->children, MuttWindowList);
  if (!last)
    return;

  // Notify the world, allowing plugins to clean up
  struct EventWindow ev_w = { last, WN_HIDDEN };
  notify_send(last->notify, NT_WINDOW, NT_WINDOW_DIALOG, &ev_w);

  last->state.visible = false;
  last->parent = NULL;
  TAILQ_REMOVE(&AllDialogsWindow->children, last, entries);

  last = TAILQ_LAST(&AllDialogsWindow->children, MuttWindowList);
  if (last)
  {
    last->state.visible = true;
    mutt_window_reflow(AllDialogsWindow);
  }
  window_set_focus(last);
  mutt_menu_set_current_redraw(REDRAW_FULL);
#ifdef USE_DEBUG_WINDOW
  debug_win_dump();
#endif
}

/**
 * dialog_config_observer - Listen for config changes affecting a Dialog - Implements ::observer_t
 */
static int dialog_config_observer(struct NotifyCallback *nc)
{
  if (!nc->event_data || !nc->global_data)
    return -1;
  if (nc->event_type != NT_CONFIG)
    return 0;

  struct EventConfig *ec = nc->event_data;
  struct MuttWindow *dlg = nc->global_data;

  if (!mutt_str_equal(ec->name, "status_on_top"))
    return 0;

  struct MuttWindow *win_first = TAILQ_FIRST(&dlg->children);

  const bool c_status_on_top = cs_subset_bool(NeoMutt->sub, "status_on_top");
  if ((c_status_on_top && (win_first->type == WT_INDEX)) ||
      (!c_status_on_top && (win_first->type != WT_INDEX)))
  {
    // Swap the Index and the IndexBar Windows
    TAILQ_REMOVE(&dlg->children, win_first, entries);
    TAILQ_INSERT_TAIL(&dlg->children, win_first, entries);
  }

  mutt_window_reflow(dlg);
  return 0;
}

/**
 * dialog_create_simple_index - Create a simple index Dialog
 * @param menu Menu to use
 * @param type Dialog type, e.g. #WT_DLG_ALIAS
 * @retval ptr New Dialog Window
 */
struct MuttWindow *dialog_create_simple_index(struct Menu *menu, enum WindowType type)
{
  if (!menu)
    return NULL;

  struct MuttWindow *dlg =
      mutt_window_new(type, MUTT_WIN_ORIENT_VERTICAL, MUTT_WIN_SIZE_MAXIMISE,
                      MUTT_WIN_SIZE_UNLIMITED, MUTT_WIN_SIZE_UNLIMITED);

  struct MuttWindow *index =
      mutt_window_new(WT_INDEX, MUTT_WIN_ORIENT_VERTICAL, MUTT_WIN_SIZE_MAXIMISE,
                      MUTT_WIN_SIZE_UNLIMITED, MUTT_WIN_SIZE_UNLIMITED);
  dlg->focus = index;

  struct MuttWindow *ibar =
      mutt_window_new(WT_INDEX_BAR, MUTT_WIN_ORIENT_VERTICAL,
                      MUTT_WIN_SIZE_FIXED, MUTT_WIN_SIZE_UNLIMITED, 1);

  const bool c_status_on_top = cs_subset_bool(NeoMutt->sub, "status_on_top");
  if (c_status_on_top)
  {
    mutt_window_add_child(dlg, ibar);
    mutt_window_add_child(dlg, index);
  }
  else
  {
    mutt_window_add_child(dlg, index);
    mutt_window_add_child(dlg, ibar);
  }

  menu->pagelen = index->state.rows;
  menu->win_index = index;
  menu->win_ibar = ibar;

  notify_observer_add(NeoMutt->notify, NT_CONFIG, dialog_config_observer, dlg);
  dialog_push(dlg);

  return dlg;
}

/**
 * dialog_destroy_simple_index - Destroy a simple index Dialog
 * @param ptr Dialog Window to destroy
 */
void dialog_destroy_simple_index(struct MuttWindow **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct MuttWindow *dlg = *ptr;

  dialog_pop();
  notify_observer_remove(NeoMutt->notify, dialog_config_observer, dlg);
  mutt_window_free(ptr);
}
