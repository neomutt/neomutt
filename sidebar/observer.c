/**
 * @file
 * Sidebar observers
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
 * @page sidebar_observers Sidebar observers
 *
 * Sidebar observers
 */

#include "config.h"
#include <stddef.h>
#include <stdbool.h>
#include "private.h"
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "lib.h"
#include "mutt_commands.h"
#include "mutt_globals.h"

void sb_win_remove_observers(struct MuttWindow *win);

/**
 * calc_divider - Decide what actions are required for the divider
 * @param wdata   Sidebar data
 * @param ascii   true, if `$ascii_chars` is set
 * @param div_str Divider string, `$sidebar_divider_char`
 * @retval num Action required, e.g. #WA_REPAINT
 *
 * If the divider changes width, then Window will need to be reflowed.
 */
WindowActionFlags calc_divider(struct SidebarWindowData *wdata, bool ascii, const char *div_str)
{
  // Calculate the width of the delimiter in screen cells
  int width = mutt_strwidth(C_SidebarDividerChar);
  if (width < 1) // Bad character or empty
  {
    width = 1;
  }
  else if (ascii)
  {
    for (int i = 0; i < width; i++)
    {
      if (div_str[i] & ~0x7F) // high-bit is set
      {
        width = 1;
        break;
      }
    }
  }

  WindowActionFlags action = WA_REPAINT;
  if (width != wdata->divider_width)
    action = WA_REFLOW;

  wdata->divider_width = width;
  return action;
}

/**
 * sb_win_init - Initialise and insert the Sidebar Window
 * @param dlg Index Dialog
 */
static struct MuttWindow *sb_win_init(struct MuttWindow *dlg)
{
  dlg->orient = MUTT_WIN_ORIENT_HORIZONTAL;

  struct MuttWindow *index_panel = TAILQ_FIRST(&dlg->children);
  mutt_window_remove_child(dlg, index_panel);

  struct MuttWindow *pager_panel = TAILQ_FIRST(&dlg->children);
  mutt_window_remove_child(dlg, pager_panel);

  struct MuttWindow *cont_right =
      mutt_window_new(WT_CONTAINER, MUTT_WIN_ORIENT_VERTICAL, MUTT_WIN_SIZE_MAXIMISE,
                      MUTT_WIN_SIZE_UNLIMITED, MUTT_WIN_SIZE_UNLIMITED);
  dlg->focus = cont_right;

  mutt_window_add_child(cont_right, index_panel);
  mutt_window_add_child(cont_right, pager_panel);
  cont_right->focus = index_panel;

  struct MuttWindow *win_sidebar =
      mutt_window_new(WT_SIDEBAR, MUTT_WIN_ORIENT_HORIZONTAL, MUTT_WIN_SIZE_FIXED,
                      C_SidebarWidth, MUTT_WIN_SIZE_UNLIMITED);
  win_sidebar->state.visible = C_SidebarVisible && (C_SidebarWidth > 0);
  win_sidebar->wdata = sb_wdata_new();
  win_sidebar->wdata_free = sb_wdata_free;

  if (C_SidebarOnRight)
  {
    mutt_window_add_child(dlg, cont_right);
    mutt_window_add_child(dlg, win_sidebar);
  }
  else
  {
    mutt_window_add_child(dlg, win_sidebar);
    mutt_window_add_child(dlg, cont_right);
  }

  sb_win_add_observers(win_sidebar);

  return win_sidebar;
}

/**
 * sb_init_data - Initialise the Sidebar data
 * @param win Sidebar Window
 */
static void sb_init_data(struct MuttWindow *win)
{
  struct SidebarWindowData *wdata = sb_wdata_get(win);
  if (!wdata)
    return;

  if (!ARRAY_EMPTY(&wdata->entries))
    return;

  struct MailboxList ml = STAILQ_HEAD_INITIALIZER(ml);
  neomutt_mailboxlist_get_all(&ml, NeoMutt, MUTT_MAILBOX_ANY);
  struct MailboxNode *np = NULL;
  STAILQ_FOREACH(np, &ml, entries)
  {
    if (!(np->mailbox->flags & MB_HIDDEN))
      sb_notify_mailbox(win, np->mailbox, SBN_CREATED);
  }
  neomutt_mailboxlist_clear(&ml);
}

/**
 * sb_account_observer - Account has changed - Implements ::observer_t
 */
static int sb_account_observer(struct NotifyCallback *nc)
{
  if ((nc->event_type != NT_ACCOUNT) || !nc->event_data || !nc->global_data)
    return -1;

  struct MuttWindow *win = nc->global_data;

  mutt_debug(LL_NOTIFY, "account\n");
  win->actions |= WA_RECALC;
  return 0;
}

/**
 * sb_color_observer - Color has changed - Implements ::observer_t
 */
static int sb_color_observer(struct NotifyCallback *nc)
{
  if ((nc->event_type != NT_COLOR) || !nc->event_data || !nc->global_data)
    return -1;

  struct MuttWindow *win = nc->global_data;

  enum ColorId color = nc->event_subtype;

  switch (color)
  {
    case MT_COLOR_INDICATOR:
    case MT_COLOR_NORMAL:
    case MT_COLOR_SIDEBAR_DIVIDER:
    case MT_COLOR_SIDEBAR_FLAGGED:
    case MT_COLOR_SIDEBAR_HIGHLIGHT:
    case MT_COLOR_SIDEBAR_INDICATOR:
    case MT_COLOR_SIDEBAR_NEW:
    case MT_COLOR_SIDEBAR_ORDINARY:
    case MT_COLOR_SIDEBAR_SPOOLFILE:
    case MT_COLOR_SIDEBAR_UNREAD:
    case MT_COLOR_MAX: // Sent on `uncolor *`
      mutt_debug(LL_NOTIFY, "color\n");
      win->actions |= WA_REPAINT;
      break;

    default:
      break;
  }
  return 0;
}

/**
 * sb_command_observer - Command has changed - Implements ::observer_t
 */
static int sb_command_observer(struct NotifyCallback *nc)
{
  if ((nc->event_type != NT_COMMAND) || !nc->event_data || !nc->global_data)
    return -1;

  struct Command *cmd = nc->event_data;

  if ((cmd->parse != sb_parse_whitelist) && (cmd->parse != sb_parse_unwhitelist))
    return 0;

  mutt_debug(LL_NOTIFY, "command\n");
  struct MuttWindow *win = nc->global_data;
  win->actions |= WA_RECALC;
  return 0;
}

/**
 * sb_config_observer - Config has changed - Implements ::observer_t
 */
static int sb_config_observer(struct NotifyCallback *nc)
{
  if ((nc->event_type != NT_CONFIG) || !nc->event_data || !nc->global_data)
    return -1;

  if (nc->event_subtype == NT_CONFIG_INITIAL_SET)
    return 0;

  struct EventConfig *ec = nc->event_data;

  if (!mutt_strn_equal(ec->name, "sidebar_", 8) && !mutt_str_equal(ec->name, "ascii_chars") &&
      !mutt_str_equal(ec->name, "folder") && !mutt_str_equal(ec->name, "spoolfile"))
  {
    return 0;
  }

  mutt_debug(LL_NOTIFY, "config: %s\n", ec->name);

  if (mutt_str_equal(ec->name, "sidebar_next_new_wrap"))
    return 0; // Affects the behaviour, but not the display

  struct MuttWindow *win = nc->global_data;

  if (mutt_str_equal(ec->name, "sidebar_visible"))
  {
    window_set_visible(win, C_SidebarVisible);
    win->parent->actions |= WA_REFLOW;
    return 0;
  }

  if (mutt_str_equal(ec->name, "sidebar_width"))
  {
    win->req_cols = C_SidebarWidth;
    win->parent->actions |= WA_REFLOW;
    return 0;
  }

  if (mutt_str_equal(ec->name, "spoolfile"))
  {
    win->actions |= WA_REPAINT;
    return 0;
  }

  if (mutt_str_equal(ec->name, "sidebar_on_right"))
  {
    struct MuttWindow *parent = win->parent;
    struct MuttWindow *first = TAILQ_FIRST(&parent->children);

    if ((C_SidebarOnRight && (first == win)) || (!C_SidebarOnRight && (first != win)))
    {
      // Swap the Sidebar and the Container of the Index/Pager
      TAILQ_REMOVE(&parent->children, first, entries);
      TAILQ_INSERT_TAIL(&parent->children, first, entries);
    }

    win->parent->actions |= WA_REFLOW;
    return 0;
  }

  if (mutt_str_equal(ec->name, "ascii_chars") ||
      mutt_str_equal(ec->name, "sidebar_divider_char"))
  {
    struct SidebarWindowData *wdata = sb_wdata_get(win);
    WindowActionFlags action = calc_divider(wdata, C_AsciiChars, C_SidebarDividerChar);
    if (action == WA_REFLOW)
      win->parent->actions |= WA_REFLOW;
    else
      win->actions |= action;
    return 0;
  }

  // All the remaining config changes...
  win->actions |= WA_RECALC;
  return 0;
}

/**
 * sb_mailbox_observer - Mailbox has changed - Implements ::observer_t
 */
static int sb_mailbox_observer(struct NotifyCallback *nc)
{
  if ((nc->event_type != NT_MAILBOX) || !nc->event_data || !nc->global_data)
    return -1;

  struct MuttWindow *win = nc->global_data;

  mutt_debug(LL_NOTIFY, "mailbox\n");
  win->actions |= WA_RECALC;
  return 0;
}

/**
 * sb_window_observer - Window has changed - Implements ::observer_t
 */
static int sb_window_observer(struct NotifyCallback *nc)
{
  if ((nc->event_type != NT_WINDOW) || !nc->event_data || !nc->global_data)
    return -1;

  struct MuttWindow *win = nc->global_data;

  if (nc->event_subtype == NT_WINDOW_FOCUS)
  {
    if (!mutt_window_is_visible(win))
      return 0;

    mutt_debug(LL_NOTIFY, "focus\n");
    win->actions |= WA_RECALC;
  }
  else if (nc->event_subtype == NT_WINDOW_DELETE)
  {
    struct EventWindow *ew = nc->event_data;
    if (ew->win != win)
      return 0;

    mutt_debug(LL_NOTIFY, "delete\n");
    sb_win_remove_observers(win);
  }
  else if (nc->event_subtype == NT_WINDOW_STATE)
  {
    mutt_debug(LL_DEBUG1, "state\n");
  }
  return 0;
}

/**
 * sb_win_add_observers - Add Observers to the Sidebar Window
 * @param win Sidebar Window
 */
void sb_win_add_observers(struct MuttWindow *win)
{
  if (!win || !NeoMutt)
    return;

  notify_observer_add(NeoMutt->notify, NT_ACCOUNT, sb_account_observer, win);
  notify_observer_add(NeoMutt->notify, NT_COLOR, sb_color_observer, win);
  notify_observer_add(NeoMutt->notify, NT_COMMAND, sb_command_observer, win);
  notify_observer_add(NeoMutt->notify, NT_CONFIG, sb_config_observer, win);
  notify_observer_add(NeoMutt->notify, NT_MAILBOX, sb_mailbox_observer, win);
  notify_observer_add(NeoMutt->notify, NT_WINDOW, sb_window_observer, win);
}

/**
 * sb_win_remove_observers - Remove Observers from the Sidebar Window
 * @param win Sidebar Window
 */
void sb_win_remove_observers(struct MuttWindow *win)
{
  if (!win || !NeoMutt)
    return;

  notify_observer_remove(NeoMutt->notify, sb_account_observer, win);
  notify_observer_remove(NeoMutt->notify, sb_color_observer, win);
  notify_observer_remove(NeoMutt->notify, sb_command_observer, win);
  notify_observer_remove(NeoMutt->notify, sb_config_observer, win);
  notify_observer_remove(NeoMutt->notify, sb_mailbox_observer, win);
  notify_observer_remove(NeoMutt->notify, sb_window_observer, win);
}

/**
 * sb_insertion_observer - Listen for new Dialogs - Implements ::observer_t
 */
int sb_insertion_observer(struct NotifyCallback *nc)
{
  if ((nc->event_type != NT_WINDOW) || (nc->event_subtype != NT_WINDOW_DIALOG))
    return 0;

  struct EventWindow *ew = nc->event_data;
  if (ew->win->type != WT_DLG_INDEX)
    return 0;

  mutt_debug(LL_NOTIFY, "insertion\n");
  if (ew->flags & WN_VISIBLE)
  {
    struct MuttWindow *win_sidebar = sb_win_init(ew->win);
    sb_init_data(win_sidebar);
  }
  else if (ew->flags & WN_HIDDEN)
  {
    sb_win_remove_observers(ew->win);
  }

  return 0;
}
