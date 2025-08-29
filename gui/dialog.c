/**
 * @file
 * Dialog Windows
 *
 * @authors
 * Copyright (C) 2020-2023 Richard Russon <rich@flatcap.org>
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
 * A Dialog is an interactive set of windows allowing the user to perform some
 * task.  See @ref gui_dlg
 *
 * @defgroup gui_dlg GUI: Dialog Windows
 *
 * A Dialog is an interactive set of windows allowing the user to perform some
 * task.
 *
 * The All Dialogs window is a container window and not visible.  All active dialogs
 * will be children of this window, though only one will be active at a time.
 *
 * ## Windows
 *
 * | Name        | Type            | Constructor      |
 * | :---------- | :-------------- | :--------------- |
 * | All Dialogs | #WT_ALL_DIALOGS | alldialogs_new() |
 *
 * **Parent**
 * - @ref gui_rootwin
 *
 * **Children**
 *
 * The All Dialogs window has many possible children, e.g.
 *
 * - @ref alias_dlg_alias
 * - @ref compose_dlg_compose
 * - @ref crypt_dlg_gpgme
 * - ...
 *
 * ## Data
 *
 * The All Dialogs window has no data.
 *
 * ## Events
 *
 * Once constructed, it is controlled by the following events:
 *
 * | Event Type  | Handler                      |
 * | :---------- | :--------------------------- |
 * | #NT_WINDOW  | alldialogs_window_observer() |
 *
 * The All Dialogs window does not implement MuttWindow::recalc() or MuttWindow::repaint().
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include "mutt/lib.h"
#include "dialog.h"
#include "mutt_window.h"
#ifdef USE_DEBUG_WINDOW
#include "debug/lib.h"
#endif

struct MuttWindow *AllDialogsWindow = NULL; ///< Parent of all Dialogs

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
  mutt_debug(LL_NOTIFY, "NT_WINDOW_DIALOG visible: %s, %p\n",
             mutt_window_win_name(dlg), (void *) dlg);
  struct EventWindow ev_w = { dlg, WN_VISIBLE };
  notify_send(dlg->notify, NT_WINDOW, NT_WINDOW_DIALOG, &ev_w);

  dlg->state.visible = true;
  dlg->parent = AllDialogsWindow;
  mutt_window_reflow(AllDialogsWindow);

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
  mutt_debug(LL_NOTIFY, "NT_WINDOW_DIALOG hidden: %s, %p\n",
             mutt_window_win_name(last), (void *) last);
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
  else
  {
    AllDialogsWindow->focus = NULL;
  }
#ifdef USE_DEBUG_WINDOW
  debug_win_dump();
#endif
}

/**
 * alldialogs_window_observer - Notification that a Window has changed - Implements ::observer_t - @ingroup observer_api
 *
 * This function is triggered by changes to the windows.
 *
 * - Delete (this window): clean up the resources held by the All Dialogs window
 */
static int alldialogs_window_observer(struct NotifyCallback *nc)
{
  if (nc->event_type != NT_WINDOW)
    return 0;
  if (!nc->global_data || !nc->event_data)
    return -1;
  if (nc->event_subtype != NT_WINDOW_DELETE)
    return 0;

  struct MuttWindow *win_alldlgs = nc->global_data;
  struct EventWindow *ev_w = nc->event_data;
  if (ev_w->win != win_alldlgs)
    return 0;

  notify_observer_remove(win_alldlgs->notify, alldialogs_window_observer, win_alldlgs);

  AllDialogsWindow = NULL;
  mutt_debug(LL_DEBUG5, "window delete done\n");
  return 0;
}

/**
 * alldialogs_new - Create the AllDialogs Window
 * @retval ptr New AllDialogs Window
 *
 * Create the container for all the Dialogs.
 */
struct MuttWindow *alldialogs_new(void)
{
  struct MuttWindow *win_alldlgs = mutt_window_new(WT_ALL_DIALOGS, MUTT_WIN_ORIENT_VERTICAL,
                                                   MUTT_WIN_SIZE_MAXIMISE, MUTT_WIN_SIZE_UNLIMITED,
                                                   MUTT_WIN_SIZE_UNLIMITED);

  notify_observer_add(win_alldlgs->notify, NT_WINDOW, alldialogs_window_observer, win_alldlgs);

  AllDialogsWindow = win_alldlgs;

  return win_alldlgs;
}
