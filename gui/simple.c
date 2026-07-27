/**
 * @file
 * Simple Dialog
 *
 * @authors
 * Copyright (C) 2021-2023 Richard Russon <rich@flatcap.org>
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
 * @page gui_simple Simple Dialog
 *
 * The Simple Dialog is an interactive set of windows containing a Menu and a
 * status bar.
 *
 * ## Windows
 *
 * | Name          | Type     | See Also            |
 * | :------------ | :------- | :------------------ |
 * | Simple Dialog | Variable | simple_dialog_new() |
 *
 * The type of the Window is determined by the caller.
 *
 * **Parent**
 * - @ref gui_dialog
 *
 * **Children**
 * - @ref menu_window
 * - @ref gui_sbar
 *
 * ## Data
 * - #Menu
 * - #Menu::mdata
 *
 * The Simple Dialog exposes access to the Menu in MuttWindow::wdata.
 * The caller may set Menu::mdata to their own data.
 *
 * ## Events
 *
 * Once constructed, it is controlled by the following events:
 *
 * | Event Type | Handler                  |
 * | :--------- | :----------------------- |
 * | #NT_CONFIG | simple_config_observer() |
 * | #NT_WINDOW | simple_window_observer() |
 *
 * The Simple Dialog does not implement MuttWindow::recalc() or MuttWindow::repaint().
 * They are handled by the child windows.
 */

#include "config.h"
#include <stdbool.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "simple.h"
#include "menu/lib.h"
#include "dialog.h"
#include "mutt_window.h"
#include "sbar.h"

/**
 * simple_config_observer - Notification that a Config Variable has changed - Implements ::observer_t - @ingroup observer_api
 *
 * The Simple Dialog is affected by changes to `$status_on_top`.
 */
static int simple_config_observer(struct NotifyCallback *nc)
{
  if (nc->event_type != NT_CONFIG)
    return 0;
  if (!nc->global_data || !nc->event_data)
    return -1;

  struct EventConfig *ev_c = nc->event_data;
  if (!mutt_str_equal(ev_c->name, "status_on_top"))
    return 0;

  struct MuttWindow *dlg = nc->global_data;
  window_status_on_top(dlg, NeoMutt->sub);
  mutt_debug(LL_DEBUG5, "config done\n");
  return 0;
}

/**
 * simple_window_observer - Notification that a Window has changed - Implements ::observer_t - @ingroup observer_api
 *
 * This function is triggered by changes to the windows.
 *
 * - Delete (this window): clean up the resources held by the Simple Dialog
 */
static int simple_window_observer(struct NotifyCallback *nc)
{
  if (nc->event_type != NT_WINDOW)
    return 0;
  if (!nc->global_data || !nc->event_data)
    return -1;
  if (nc->event_subtype != NT_WINDOW_DELETE)
    return 0;

  struct MuttWindow *dlg = nc->global_data;
  struct EventWindow *ev_w = nc->event_data;
  if (ev_w->win != dlg)
    return 0;

  notify_observer_remove(NeoMutt->sub->notify, simple_config_observer, dlg);
  notify_observer_remove(dlg->notify, simple_window_observer, dlg);

  mutt_debug(LL_DEBUG5, "window delete done\n");
  return 0;
}

/**
 * simple_dialog_new - Create a simple index Dialog
 * @param md        Menu Definition
 * @param wtype     Dialog type, e.g. #WT_DLG_ALIAS
 * @param help_data Data for the Help Bar
 * @retval obj SimpleDialogWindows Tuple containing Dialog, SimpleBar and Menu pointers
 */
struct SimpleDialogWindows simple_dialog_new(const struct MenuDefinition *md,
                                             enum WindowType wtype,
                                             const struct Mapping *help_data)
{
  struct MuttWindow *dlg = mutt_window_new(wtype, MUTT_WIN_ORIENT_VERTICAL,
                                           MUTT_WIN_SIZE_MAXIMISE, MUTT_WIN_SIZE_UNLIMITED,
                                           MUTT_WIN_SIZE_UNLIMITED);
  dlg->help_md = md;
  dlg->help_data = help_data;

  struct MuttWindow *win_menu = menu_window_new(md, NeoMutt->sub);
  dlg->wdata = win_menu->wdata;

  struct MuttWindow *win_sbar = sbar_new();
  const bool c_status_on_top = cs_subset_bool(NeoMutt->sub, "status_on_top");
  if (c_status_on_top)
  {
    mutt_window_add_child(dlg, win_sbar);
    mutt_window_add_child(dlg, win_menu);
  }
  else
  {
    mutt_window_add_child(dlg, win_menu);
    mutt_window_add_child(dlg, win_sbar);
  }

  notify_observer_add(NeoMutt->sub->notify, NT_CONFIG, simple_config_observer, dlg);
  notify_observer_add(dlg->notify, NT_WINDOW, simple_window_observer, dlg);
  dialog_push(dlg);

  return (struct SimpleDialogWindows) { dlg, win_sbar, win_menu->wdata };
}

/**
 * simple_dialog_free - Destroy a simple index Dialog
 * @param ptr Dialog Window to destroy
 */
void simple_dialog_free(struct MuttWindow **ptr)
{
  if (!ptr || !*ptr)
    return;

  dialog_pop();
  mutt_window_free(ptr);
}
