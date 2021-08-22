/**
 * @file
 * Compose Bar (status)
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
 * @page compose_cbar Compose Bar (status)
 *
 * ## Overview
 *
 * The Compose Bar Window displays status info about the email.
 *
 * ## Windows
 *
 * | Name               | Type          | See Also   |
 * | :----------------- | :------------ | :--------- |
 * | Compose Bar Window | WT_STATUS_BAR | cbar_new() |
 *
 * **Parent**
 * - @ref compose_dialog
 *
 * **Children**
 *
 * None.
 *
 * ## Data
 * - #ComposeBarData
 *
 * The Compose Bar Window stores its data (#ComposeBarData) in
 * MuttWindow::wdata.
 *
 * ## Events
 *
 * Once constructed, it is controlled by the following events:
 *
 * | Event Type            | Handler                 |
 * | :-------------------- | :---------------------- | 
 * | #NT_COLOR             | cbar_color_observer()   |
 * | #NT_COMPOSE           | cbar_compose_observer() |
 * | #NT_CONFIG            | cbar_config_observer()  |
 * | #NT_WINDOW            | cbar_window_observer()  |
 * | MuttWindow::recalc()  | cbar_recalc()           |
 * | MuttWindow::repaint() | cbar_repaint()          |
 */

#include "config.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "private.h"
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "color/lib.h"
#include "index/lib.h"
#include "menu/lib.h"
#include "attach_data.h"
#include "cbar_data.h"
#include "format_flags.h"
#include "mutt_globals.h"
#include "muttlib.h"
#include "shared_data.h"

/**
 * num_attachments - Count the number of attachments
 * @param adata Attachment data
 * @retval num Number of attachments
 */
int num_attachments(struct ComposeAttachData *adata)
{
  if (!adata || !adata->menu)
    return 0;
  return adata->menu->max;
}

/**
 * compose_format_str - Create the status bar string for compose mode - Implements ::format_t - @ingroup expando_api
 *
 * | Expando | Description
 * |:--------|:--------------------------------------------------------
 * | \%a     | Total number of attachments
 * | \%h     | Local hostname
 * | \%l     | Approximate size (in bytes) of the current message
 * | \%v     | NeoMutt version string
 */
static const char *compose_format_str(char *buf, size_t buflen, size_t col, int cols,
                                      char op, const char *src, const char *prec,
                                      const char *if_str, const char *else_str,
                                      intptr_t data, MuttFormatFlags flags)
{
  char fmt[128], tmp[128];
  bool optional = (flags & MUTT_FORMAT_OPTIONAL);
  struct ComposeSharedData *shared = (struct ComposeSharedData *) data;

  *buf = '\0';
  switch (op)
  {
    case 'a': /* total number of attachments */
      snprintf(fmt, sizeof(fmt), "%%%sd", prec);
      snprintf(buf, buflen, fmt, num_attachments(shared->adata));
      break;

    case 'h': /* hostname */
      snprintf(fmt, sizeof(fmt), "%%%ss", prec);
      snprintf(buf, buflen, fmt, NONULL(ShortHostname));
      break;

    case 'l': /* approx length of current message in bytes */
      snprintf(fmt, sizeof(fmt), "%%%ss", prec);
      mutt_str_pretty_size(tmp, sizeof(tmp),
                           cum_attachs_size(shared->sub, shared->adata));
      snprintf(buf, buflen, fmt, tmp);
      break;

    case 'v':
      snprintf(buf, buflen, "%s", mutt_make_version());
      break;

    case 0:
      *buf = '\0';
      return src;

    default:
      snprintf(buf, buflen, "%%%s%c", prec, op);
      break;
  }

  if (optional)
  {
    mutt_expando_format(buf, buflen, col, cols, if_str, compose_format_str, data, flags);
  }
  // This format function doesn't have any optional expandos,
  // so there's no `else if (flags & MUTT_FORMAT_OPTIONAL)` clause

  return src;
}

/**
 * cbar_recalc - Recalculate the Window data - Implements MuttWindow::recalc() - @ingroup window_recalc
 */
static int cbar_recalc(struct MuttWindow *win)
{
  char buf[1024] = { 0 };
  struct ComposeSharedData *shared = win->parent->wdata;

  const char *const c_compose_format =
      cs_subset_string(shared->sub, "compose_format");
  mutt_expando_format(buf, sizeof(buf), 0, win->state.cols, NONULL(c_compose_format),
                      compose_format_str, (intptr_t) shared, MUTT_FORMAT_NO_FLAGS);

  struct ComposeBarData *cbar_data = win->wdata;
  if (!mutt_str_equal(buf, cbar_data->compose_format))
  {
    mutt_str_replace(&cbar_data->compose_format, buf);
    win->actions |= WA_REPAINT;
    mutt_debug(LL_DEBUG5, "recalc done, request WA_REPAINT\n");
  }

  return 0;
}

/**
 * cbar_repaint - Repaint the Window - Implements MuttWindow::repaint() - @ingroup window_repaint
 */
static int cbar_repaint(struct MuttWindow *win)
{
  if (!mutt_window_is_visible(win))
    return 0;

  struct ComposeBarData *cbar_data = win->wdata;

  mutt_window_move(win, 0, 0);
  mutt_curses_set_color(MT_COLOR_STATUS);
  mutt_window_clrtoeol(win);

  mutt_window_move(win, 0, 0);
  mutt_draw_statusline(win, win->state.cols, cbar_data->compose_format,
                       mutt_str_len(cbar_data->compose_format));
  mutt_curses_set_color(MT_COLOR_NORMAL);
  mutt_debug(LL_DEBUG5, "repaint done\n");

  return 0;
}

/**
 * cbar_color_observer - Notification that a Color has changed - Implements ::observer_t
 */
int cbar_color_observer(struct NotifyCallback *nc)
{
  if ((nc->event_type != NT_COLOR) || !nc->global_data || !nc->event_data)
    return -1;

  struct EventColor *ev_c = nc->event_data;

  // MT_COLOR_MAX is sent on `uncolor *`
  if ((ev_c->color != MT_COLOR_STATUS) && (ev_c->color != MT_COLOR_MAX))
    return 0;

  struct MuttWindow *win_cbar = nc->global_data;
  win_cbar->actions |= WA_REPAINT;
  mutt_debug(LL_DEBUG5, "color done, request WA_REPAINT\n");

  return 0;
}

/**
 * cbar_compose_observer - Notification that the Compose data has changed - Implements ::observer_t
 */
int cbar_compose_observer(struct NotifyCallback *nc)
{
  if ((nc->event_type != NT_COMPOSE) || !nc->global_data || !nc->event_data)
    return -1;

  struct MuttWindow *win_cbar = nc->global_data;
  win_cbar->actions |= WA_REPAINT;
  mutt_debug(LL_DEBUG5, "compose done, request WA_REPAINT\n");

  return 0;
}

/**
 * cbar_config_observer - Notification that a Config Variable has changed - Implements ::observer_t
 */
int cbar_config_observer(struct NotifyCallback *nc)
{
  if ((nc->event_type != NT_CONFIG) || !nc->global_data || !nc->event_data)
    return -1;

  struct EventConfig *ev_c = nc->event_data;
  if (!mutt_str_equal(ev_c->name, "compose_format"))
    return 0;

  struct MuttWindow *win_cbar = nc->global_data;
  win_cbar->actions |= WA_RECALC;
  mutt_debug(LL_DEBUG5, "config done, request WA_RECALC\n");

  return 0;
}

/**
 * cbar_window_observer - Notification that a Window has changed - Implements ::observer_t
 */
int cbar_window_observer(struct NotifyCallback *nc)
{
  if ((nc->event_type != NT_WINDOW) || !nc->global_data || !nc->event_data)
    return -1;

  struct MuttWindow *win_cbar = nc->global_data;
  struct EventWindow *ev_w = nc->event_data;
  if (ev_w->win != win_cbar)
    return 0;

  if (nc->event_subtype == NT_WINDOW_STATE)
  {
    win_cbar->actions |= WA_RECALC;
    mutt_debug(LL_DEBUG5, "window state done, request WA_RECALC\n");
  }
  else if (nc->event_subtype == NT_WINDOW_DELETE)
  {
    struct MuttWindow *dlg = win_cbar->parent;
    struct ComposeSharedData *shared = dlg->wdata;

    notify_observer_remove(NeoMutt->notify, cbar_color_observer, win_cbar);
    notify_observer_remove(shared->notify, cbar_compose_observer, win_cbar);
    notify_observer_remove(NeoMutt->notify, cbar_config_observer, win_cbar);
    notify_observer_remove(win_cbar->notify, cbar_window_observer, win_cbar);

    mutt_debug(LL_DEBUG5, "window delete done\n");
  }

  return 0;
}

/**
 * cbar_new - Create the Compose Bar (status)
 * @param parent Parent Window
 * @param shared Shared compose data
 */
struct MuttWindow *cbar_new(struct MuttWindow *parent, struct ComposeSharedData *shared)
{
  struct MuttWindow *win_cbar =
      mutt_window_new(WT_STATUS_BAR, MUTT_WIN_ORIENT_VERTICAL,
                      MUTT_WIN_SIZE_FIXED, MUTT_WIN_SIZE_UNLIMITED, 1);

  win_cbar->wdata = cbar_data_new();
  win_cbar->wdata_free = cbar_data_free;
  win_cbar->recalc = cbar_recalc;
  win_cbar->repaint = cbar_repaint;

  notify_observer_add(NeoMutt->notify, NT_COLOR, cbar_color_observer, win_cbar);
  notify_observer_add(shared->notify, NT_COMPOSE, cbar_compose_observer, win_cbar);
  notify_observer_add(NeoMutt->notify, NT_CONFIG, cbar_config_observer, win_cbar);
  notify_observer_add(win_cbar->notify, NT_WINDOW, cbar_window_observer, win_cbar);

  return win_cbar;
}
