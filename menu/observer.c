/**
 * @file
 * Menu notification observers
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
 * @page menu_observer Menu notification observers
 *
 * Menu notification observers
 */

#include "config.h"
#include <stdbool.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "menu/lib.h"
#include "options.h"
#include "type.h"

/**
 * menu_color_observer - Notification that a Color has changed - Implements ::observer_t
 */
static int menu_color_observer(struct NotifyCallback *nc)
{
  if ((nc->event_type != NT_COLOR) || !nc->global_data || !nc->event_data)
    return -1;

  struct EventColor *ev_c = nc->event_data;

  // MT_COLOR_MAX is sent on `uncolor *`
  if ((ev_c->color != MT_COLOR_NORMAL) && (ev_c->color != MT_COLOR_INDICATOR) &&
      (ev_c->color != MT_COLOR_MAX))
  {
    return 0;
  }

  struct Menu *menu = nc->global_data;
  struct MuttWindow *win_menu = menu->win_index;

  menu->redraw = MENU_REDRAW_FULL;
  win_menu->actions |= WA_REPAINT;
  mutt_debug(LL_DEBUG5, "color done, request WA_REPAINT, MENU_REDRAW_FULL\n");

  return 0;
}

/**
 * menu_config_observer - Notification that a Config Variable has changed - Implements ::observer_t
 */
static int menu_config_observer(struct NotifyCallback *nc)
{
  if ((nc->event_type != NT_CONFIG) || !nc->global_data || !nc->event_data)
    return -1;

  struct EventConfig *ev_c = nc->event_data;
  if (!mutt_str_startswith(ev_c->name, "arrow_") && !mutt_str_startswith(ev_c->name, "menu_"))
    return 0;

  struct Menu *menu = nc->global_data;
  struct MuttWindow *win_menu = menu->win_index;

  menu->redraw |= MENU_REDRAW_INDEX;
  win_menu->actions |= WA_RECALC;
  mutt_debug(LL_DEBUG5, "config done, request WA_RECALC, MENU_REDRAW_INDEX\n");
  return 0;
}

/**
 * menu_window_observer - Notification that a Window has changed - Implements ::observer_t
 */
static int menu_window_observer(struct NotifyCallback *nc)
{
  if ((nc->event_type != NT_WINDOW) || !nc->global_data || !nc->event_data)
    return -1;

  struct Menu *menu = nc->global_data;
  if (nc->event_subtype == NT_WINDOW_STATE)
  {
    struct EventWindow *ev_w = nc->event_data;
    struct MuttWindow *win = ev_w->win;

    menu->pagelen = win->state.rows;
    menu->redraw |= MENU_REDRAW_INDEX;

    win->actions |= WA_REPAINT;
    mutt_debug(LL_DEBUG5,
               "window state done, request MENU_REDRAW_INDEX, WA_REPAINT\n");
  }
  else if (nc->event_subtype == NT_WINDOW_DELETE)
  {
    notify_observer_remove(NeoMutt->notify, menu_config_observer, menu);
    notify_observer_remove(menu->win_index->notify, menu_window_observer, menu);
    mutt_color_observer_remove(menu_color_observer, menu);
    mutt_debug(LL_DEBUG5, "window delete done\n");
  }

  return 0;
}

/**
 * menu_add_observers - Add the notification observers
 * @param menu Menu
 */
void menu_add_observers(struct Menu *menu)
{
  notify_observer_add(NeoMutt->notify, NT_CONFIG, menu_config_observer, menu);
  notify_observer_add(menu->win_index->notify, NT_WINDOW, menu_window_observer, menu);
  mutt_color_observer_add(menu_color_observer, menu);
}
