/**
 * @file
 * Help Bar
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
 * @page helpbar_helpbar Help Bar
 *
 * Help Bar
 */

#include "config.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include "private.h"
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "keymap.h"

static int helpbar_observer(struct NotifyCallback *nc);

/**
 * make_help - Create one entry for the help bar
 * @param buf    Buffer for the result
 * @param buflen Length of buffer
 * @param txt    Text part, e.g. "delete"
 * @param menu   Current Menu, e.g. #MENU_PAGER
 * @param op     Operation, e.g. OP_DELETE
 * @retval true If the keybinding exists
 *
 * This will return something like: "d:delete"
 */
static bool make_help(char *buf, size_t buflen, const char *txt, enum MenuType menu, int op)
{
  char tmp[128];

  if (km_expand_key(tmp, sizeof(tmp), km_find_func(menu, op)) ||
      km_expand_key(tmp, sizeof(tmp), km_find_func(MENU_GENERIC, op)))
  {
    snprintf(buf, buflen, "%s:%s", tmp, txt);
    return true;
  }

  buf[0] = '\0';
  return false;
}

/**
 * compile_help - Create the text for the help menu
 * @param buf    Buffer for the result
 * @param buflen Length of buffer
 * @param menu   Current Menu, e.g. #MENU_PAGER
 * @param items  Map of functions to display in the help bar
 * @retval ptr Buffer containing result
 */
static char *compile_help(char *buf, size_t buflen, enum MenuType menu,
                          const struct Mapping *items)
{
  char *pbuf = buf;

  for (int i = 0; items[i].name && (buflen > 2); i++)
  {
    if (!make_help(pbuf, buflen, _(items[i].name), menu, items[i].value))
      continue;

    const size_t len = mutt_str_len(pbuf);
    pbuf += len;
    buflen -= len;

    *pbuf++ = ' ';
    *pbuf++ = ' ';
    buflen -= 2;
  }
  return buf;
}

/**
 * helpbar_recalc - Recalculate the display of the Help Bar
 * @param win Help Bar Window
 *
 * The Help Bar isn't drawn, yet.
 */
static int helpbar_recalc(struct MuttWindow *win)
{
  if (!win)
    return -1;

  struct MuttWindow *win_focus = window_get_focus();
  if (!win_focus)
    return 0;

  // Ascend the Window tree until we find help_data
  while (win_focus && !win_focus->help_data)
    win_focus = win_focus->parent;

  if (!win_focus)
    return 0;

  struct HelpbarWindowData *wdata = helpbar_wdata_get(win);
  if (!wdata)
    return 0;

  mutt_debug(LL_NOTIFY, "recalc\n");

  char helpstr[1024] = { 0 };
  compile_help(helpstr, sizeof(helpstr), win_focus->help_menu, win_focus->help_data);

  wdata->help_menu = win_focus->help_menu;
  wdata->help_data = win_focus->help_data;
  mutt_str_replace(&wdata->help_str, helpstr);
  win->actions |= WA_REPAINT;

  return 0;
}

/**
 * helpbar_repaint - Redraw the Help Bar
 * @param win Help Bar Window
 *
 * The Help Bar is drawn from the data cached in the HelpbarWindowData.
 * No calculation is performed.
 */
static int helpbar_repaint(struct MuttWindow *win)
{
  if (!mutt_window_is_visible(win))
    return 0;

  struct HelpbarWindowData *wdata = helpbar_wdata_get(win);
  if (!wdata)
    return 0;

  mutt_debug(LL_NOTIFY, "repaint\n");

  mutt_curses_set_color(MT_COLOR_STATUS);
  mutt_window_move(win, 0, 0);
  mutt_paddstr(win->state.cols, wdata->help_str);
  mutt_curses_set_color(MT_COLOR_NORMAL);

  return 0;
}

/**
 * helpbar_binding_event - Key binding has changed
 * @param nc Notification
 */
static void helpbar_binding_event(struct NotifyCallback *nc)
{
  if (nc->event_subtype >= NT_MACRO_NEW)
    return;

  struct MuttWindow *win_helpbar = nc->global_data;
  struct HelpbarWindowData *wdata = helpbar_wdata_get(win_helpbar);
  if (!wdata)
    return;

  struct EventBinding *eb = nc->event_data;
  if (wdata->help_menu != eb->menu)
    return;

  mutt_debug(LL_NOTIFY, "binding");
  win_helpbar->actions |= WA_RECALC;
}

/**
 * helpbar_color_event - Color has changed
 * @param nc Notification
 */
static void helpbar_color_event(struct NotifyCallback *nc)
{
  if (nc->event_subtype != MT_COLOR_STATUS)
    return;

  struct MuttWindow *win_helpbar = nc->global_data;

  mutt_debug(LL_NOTIFY, "color\n");
  win_helpbar->actions |= WA_REPAINT;
}

/**
 * helpbar_config_event - Config has changed
 * @param nc Notification
 */
static void helpbar_config_event(struct NotifyCallback *nc)
{
  struct EventConfig *ec = nc->event_data;
  if (!mutt_str_equal(ec->name, "help"))
    return;

  struct MuttWindow *win_helpbar = nc->global_data;
  win_helpbar->state.visible = cs_subset_bool(NeoMutt->sub, "help");

  mutt_debug(LL_NOTIFY, "config\n");
  win_helpbar->parent->actions |= WA_REFLOW;
}

/**
 * helpbar_window_event - Window has changed
 * @param nc Notification
 */
static void helpbar_window_event(struct NotifyCallback *nc)
{
  struct MuttWindow *win_helpbar = nc->global_data;

  if (nc->event_subtype == NT_WINDOW_FOCUS)
  {
    if (!mutt_window_is_visible(win_helpbar))
      return;

    mutt_debug(LL_NOTIFY, "focus\n");
    win_helpbar->actions |= WA_RECALC;
  }
  else if (nc->event_subtype == NT_WINDOW_DELETE)
  {
    struct EventWindow *ew = nc->event_data;
    if (ew->win != win_helpbar)
      return;

    mutt_debug(LL_NOTIFY, "delete\n");
    notify_observer_remove(nc->current, helpbar_observer, win_helpbar);
  }
}

/**
 * helpbar_observer - Listen for events affecting the Help Bar Window - Implements ::observer_t
 */
static int helpbar_observer(struct NotifyCallback *nc)
{
  if (!nc->event_data || !nc->global_data)
    return -1;

  switch (nc->event_type)
  {
    case NT_BINDING:
      helpbar_binding_event(nc);
      break;
    case NT_COLOR:
      helpbar_color_event(nc);
      break;
    case NT_CONFIG:
      helpbar_config_event(nc);
      break;
    case NT_WINDOW:
      helpbar_window_event(nc);
      break;
    default:
      break;
  }

  return 0;
}

/**
 * helpbar_create - Create the Help Bar Window
 * @retval ptr New Window
 */
struct MuttWindow *helpbar_create(void)
{
  struct MuttWindow *win =
      mutt_window_new(WT_HELP_BAR, MUTT_WIN_ORIENT_VERTICAL,
                      MUTT_WIN_SIZE_FIXED, MUTT_WIN_SIZE_UNLIMITED, 1);
  win->state.visible = cs_subset_bool(NeoMutt->sub, "help");

  win->recalc = helpbar_recalc;
  win->repaint = helpbar_repaint;

  win->wdata = helpbar_wdata_new();
  win->wdata_free = helpbar_wdata_free;

  notify_observer_add(NeoMutt->notify, helpbar_observer, win);
  return win;
}
