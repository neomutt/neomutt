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
#include "lib.h"
#include "menu/lib.h"
#include "keymap.h"

/**
 * make_help - Create one entry for the help bar
 * @param buf    Buffer for the result
 * @param buflen Length of buffer
 * @param txt    Text part, e.g. "delete"
 * @param menu   Current Menu, e.g. #MENU_PAGER
 * @param op     Operation, e.g. OP_DELETE
 * @retval true The keybinding exists
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

  char helpstr[1024] = { 0 };
  compile_help(helpstr, sizeof(helpstr), win_focus->help_menu, win_focus->help_data);

  wdata->help_menu = win_focus->help_menu;
  wdata->help_data = win_focus->help_data;
  mutt_str_replace(&wdata->help_str, helpstr);

  win->actions |= WA_REPAINT;
  mutt_debug(LL_DEBUG5, "recalc done, request WA_REPAINT\n");
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

  mutt_curses_set_color(MT_COLOR_STATUS);
  mutt_window_move(win, 0, 0);
  mutt_paddstr(win, win->state.cols, wdata->help_str);
  mutt_curses_set_color(MT_COLOR_NORMAL);

  mutt_debug(LL_DEBUG5, "repaint done\n");
  return 0;
}

/**
 * helpbar_binding_observer - Key binding has changed - Implements ::observer_t
 */
static int helpbar_binding_observer(struct NotifyCallback *nc)
{
  if ((nc->event_type != NT_BINDING) || !nc->event_data || !nc->global_data)
    return -1;

  if (nc->event_subtype >= NT_MACRO_ADD)
    return 0;

  struct MuttWindow *win_helpbar = nc->global_data;
  struct HelpbarWindowData *wdata = helpbar_wdata_get(win_helpbar);
  if (!wdata)
    return 0;

  struct EventBinding *ev_b = nc->event_data;
  if (wdata->help_menu != ev_b->menu)
    return 0;

  win_helpbar->actions |= WA_RECALC;
  mutt_debug(LL_DEBUG5, "binding done, request WA_RECALC\n");
  return 0;
}

/**
 * helpbar_color_observer - Color has changed - Implements ::observer_t
 */
static int helpbar_color_observer(struct NotifyCallback *nc)
{
  struct EventColor *ev_c = nc->event_data;

  if ((nc->event_type != NT_COLOR) || !nc->event_data || !nc->global_data)
    return -1;

  if (ev_c->color != MT_COLOR_STATUS)
    return 0;

  struct MuttWindow *win_helpbar = nc->global_data;

  win_helpbar->actions |= WA_REPAINT;
  mutt_debug(LL_DEBUG5, "color done, request WA_REPAINT\n");
  return 0;
}

/**
 * helpbar_config_observer - Config has changed - Implements ::observer_t
 */
static int helpbar_config_observer(struct NotifyCallback *nc)
{
  if ((nc->event_type != NT_CONFIG) || !nc->event_data || !nc->global_data)
    return -1;

  struct EventConfig *ev_c = nc->event_data;
  if (!mutt_str_equal(ev_c->name, "help"))
    return 0;

  struct MuttWindow *win_helpbar = nc->global_data;
  win_helpbar->state.visible = cs_subset_bool(NeoMutt->sub, "help");

  win_helpbar->parent->actions |= WA_REFLOW;
  mutt_debug(LL_DEBUG5, "config: '%s', request WA_REFLOW on parent\n", ev_c->name);
  return 0;
}

/**
 * helpbar_window_observer - Window has changed - Implements ::observer_t
 */
static int helpbar_window_observer(struct NotifyCallback *nc)
{
  if ((nc->event_type != NT_WINDOW) || !nc->event_data || !nc->global_data)
    return -1;

  struct MuttWindow *win_helpbar = nc->global_data;

  if (nc->event_subtype == NT_WINDOW_FOCUS)
  {
    if (!mutt_window_is_visible(win_helpbar))
      return 0;

    mutt_debug(LL_NOTIFY, "focus\n");
    win_helpbar->actions |= WA_RECALC;
  }
  else if (nc->event_subtype == NT_WINDOW_DELETE)
  {
    struct EventWindow *ev_w = nc->event_data;
    if (ev_w->win != win_helpbar)
      return 0;

    notify_observer_remove(NeoMutt->notify, helpbar_binding_observer, win_helpbar);
    notify_observer_remove(NeoMutt->notify, helpbar_color_observer, win_helpbar);
    notify_observer_remove(NeoMutt->notify, helpbar_config_observer, win_helpbar);
    notify_observer_remove(NeoMutt->notify, helpbar_window_observer, win_helpbar);
    mutt_debug(LL_DEBUG5, "window delete done\n");
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

  notify_observer_add(NeoMutt->notify, NT_BINDING, helpbar_binding_observer, win);
  notify_observer_add(NeoMutt->notify, NT_COLOR, helpbar_color_observer, win);
  notify_observer_add(NeoMutt->notify, NT_CONFIG, helpbar_config_observer, win);
  notify_observer_add(NeoMutt->notify, NT_WINDOW, helpbar_window_observer, win);
  return win;
}
