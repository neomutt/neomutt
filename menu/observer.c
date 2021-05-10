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
#include "email/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "menu/lib.h"
#include "context.h"
#include "mutt_globals.h"
#include "options.h"
#include "type.h"

/**
 * menu_color_observer - Listen for colour changes affecting the menu - Implements ::observer_t
 */
static int menu_color_observer(struct NotifyCallback *nc)
{
  if (!nc->event_data || !nc->global_data)
    return -1;
  if (nc->event_type != NT_COLOR)
    return 0;

  struct EventColor *ev_c = nc->event_data;

  int c = ev_c->color;

  // MT_COLOR_MAX is sent on `uncolor *`
  bool simple = (c == MT_COLOR_INDEX_COLLAPSED) || (c == MT_COLOR_INDEX_DATE) ||
                (c == MT_COLOR_INDEX_LABEL) || (c == MT_COLOR_INDEX_NUMBER) ||
                (c == MT_COLOR_INDEX_SIZE) || (c == MT_COLOR_INDEX_TAGS) ||
                (c == MT_COLOR_MAX);
  bool lists = (c == MT_COLOR_ATTACH_HEADERS) || (c == MT_COLOR_BODY) ||
               (c == MT_COLOR_HEADER) || (c == MT_COLOR_INDEX) ||
               (c == MT_COLOR_INDEX_AUTHOR) || (c == MT_COLOR_INDEX_FLAGS) ||
               (c == MT_COLOR_INDEX_SUBJECT) || (c == MT_COLOR_INDEX_TAG) ||
               (c == MT_COLOR_MAX);

  // The changes aren't relevant to the index menu
  if (!simple && !lists)
    return 0;

  // Colour deleted from a list
  struct Mailbox *m = ctx_mailbox(Context);
  if ((nc->event_subtype == NT_COLOR_RESET) && lists && m)
  {
    // Force re-caching of index colors
    for (int i = 0; i < m->msg_count; i++)
    {
      struct Email *e = m->emails[i];
      if (!e)
        break;
      e->pair = 0;
    }
  }

  struct Menu *menu = nc->global_data;
  menu->redraw = REDRAW_FULL;

  return 0;
}

/**
 * menu_config_observer - Listen for config changes affecting the menu - Implements ::observer_t
 */
static int menu_config_observer(struct NotifyCallback *nc)
{
  if (!nc->event_data || !nc->global_data)
    return -1;
  if (nc->event_type != NT_CONFIG)
    return 0;

  struct EventConfig *ec = nc->event_data;

  const struct ConfigDef *cdef = ec->he->data;
  ConfigRedrawFlags flags = cdef->type & R_REDRAW_MASK;

  if (flags == R_REDRAW_NO_FLAGS)
    return 0;

  struct Menu *menu = nc->global_data;
  if ((menu->type == MENU_MAIN) && (flags & R_INDEX))
    menu->redraw |= REDRAW_FULL;
  if ((menu->type == MENU_PAGER) && (flags & R_PAGER))
    menu->redraw |= REDRAW_FULL;
  if (flags & R_PAGER_FLOW)
  {
    menu->redraw |= REDRAW_FULL | REDRAW_FLOW;
  }

  if (flags & R_RESORT_SUB)
    OptSortSubthreads = true;
  if (flags & R_RESORT)
    OptNeedResort = true;
  if (flags & R_RESORT_INIT)
    OptResortInit = true;
  if (flags & R_TREE)
    OptRedrawTree = true;

  if (flags & R_MENU)
    menu->redraw |= REDRAW_FULL;

  return 0;
}

/**
 * menu_window_observer - Listen for Window changes affecting the menu - Implements ::observer_t
 */
static int menu_window_observer(struct NotifyCallback *nc)
{
  if (!nc->event_data || !nc->global_data)
    return -1;
  if (nc->event_type != NT_WINDOW)
    return 0;
  if (nc->event_subtype != NT_WINDOW_STATE)
    return 0;

  struct Menu *menu = nc->global_data;
  struct EventWindow *ev_w = nc->event_data;
  struct MuttWindow *win = ev_w->win;

  menu->pagelen = win->state.rows;
  menu->redraw = REDRAW_FULL;

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

/**
 * menu_remove_observers - Remove the notification observers
 * @param menu Menu
 */
void menu_remove_observers(struct Menu *menu)
{
  notify_observer_remove(NeoMutt->notify, menu_config_observer, menu);
  notify_observer_remove(menu->win_index->notify, menu_window_observer, menu);
  mutt_color_observer_remove(menu_color_observer, menu);
}
