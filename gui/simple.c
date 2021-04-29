/**
 * @file
 * Simple Dialog Windows
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
 * @page gui_simple Simple Dialog Windows
 *
 * Simple Dialog Windows
 */

#include "config.h"
#include <stdbool.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "lib.h"
#include "keymap.h"
#include "mutt_menu.h"
#include "sbar.h"

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
  if ((c_status_on_top && (win_first->type == WT_MENU)) ||
      (!c_status_on_top && (win_first->type != WT_MENU)))
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
 * @param mtype     Menu type, e.g. #MENU_ALIAS
 * @param wtype     Dialog type, e.g. #WT_DLG_ALIAS
 * @param help_data Data for the Help Bar
 * @retval ptr New Dialog Window
 */
struct MuttWindow *dialog_create_simple_index(enum MenuType mtype, enum WindowType wtype,
                                              const struct Mapping *help_data)
{
  struct MuttWindow *dlg =
      mutt_window_new(wtype, MUTT_WIN_ORIENT_VERTICAL, MUTT_WIN_SIZE_MAXIMISE,
                      MUTT_WIN_SIZE_UNLIMITED, MUTT_WIN_SIZE_UNLIMITED);
  dlg->help_menu = mtype;
  dlg->help_data = help_data;

  struct MuttWindow *index =
      mutt_window_new(WT_MENU, MUTT_WIN_ORIENT_VERTICAL, MUTT_WIN_SIZE_MAXIMISE,
                      MUTT_WIN_SIZE_UNLIMITED, MUTT_WIN_SIZE_UNLIMITED);
  dlg->focus = index;

  struct Menu *menu = menu_new(index, mtype);
  dlg->wdata = menu;

  const bool c_status_on_top = cs_subset_bool(NeoMutt->sub, "status_on_top");
  if (c_status_on_top)
  {
    sbar_add(dlg);
    mutt_window_add_child(dlg, index);
  }
  else
  {
    mutt_window_add_child(dlg, index);
    sbar_add(dlg);
  }

  menu_push_current(menu);

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

  struct Menu *menu = dlg->wdata;
  menu_pop_current(menu);
  menu_free(&menu);

  dialog_pop();
  notify_observer_remove(NeoMutt->notify, dialog_config_observer, dlg);
  mutt_window_free(ptr);
}
