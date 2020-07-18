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
#include "mutt/lib.h"
#include "debug/lib.h"
#include "lib.h"
#include "mutt_menu.h"

/**
 * mutt_window_dialog - Find the parent Dialog of a Window
 * @param win Window
 * @retval ptr Dialog
 *
 * Dialog Windows will be owned by a MuttWindow of type #WT_ALL_DIALOGS.
 */
struct MuttWindow *mutt_window_dialog(struct MuttWindow *win)
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
  if (!dlg || !MuttDialogWindow)
    return;

  struct MuttWindow *last = TAILQ_LAST(&MuttDialogWindow->children, MuttWindowList);
  if (last)
    last->state.visible = false;

  TAILQ_INSERT_TAIL(&MuttDialogWindow->children, dlg, entries);
  notify_set_parent(dlg->notify, MuttDialogWindow->notify);

  // Notify the world, allowing plugins to integrate
  struct EventWindow ev_w = { dlg, WN_VISIBLE };
  notify_send(dlg->notify, NT_WINDOW, NT_WINDOW_DIALOG, &ev_w);

  dlg->state.visible = true;
  dlg->parent = MuttDialogWindow;
  mutt_window_reflow(MuttDialogWindow);
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
  if (!MuttDialogWindow)
    return;

  struct MuttWindow *last = TAILQ_LAST(&MuttDialogWindow->children, MuttWindowList);
  if (!last)
    return;

  // Notify the world, allowing plugins to clean up
  struct EventWindow ev_w = { last, WN_HIDDEN };
  notify_send(last->notify, NT_WINDOW, NT_WINDOW_DIALOG, &ev_w);

  last->state.visible = false;
  last->parent = NULL;
  TAILQ_REMOVE(&MuttDialogWindow->children, last, entries);

  last = TAILQ_LAST(&MuttDialogWindow->children, MuttWindowList);
  if (last)
  {
    last->state.visible = true;
    mutt_window_reflow(MuttDialogWindow);
  }
  mutt_menu_set_current_redraw(REDRAW_FULL);
#ifdef USE_DEBUG_WINDOW
  debug_win_dump();
#endif
}

