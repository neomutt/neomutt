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
 * Compose Bar (status)
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
 * compose_format_str - Create the status bar string for compose mode - Implements ::format_t
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
 * cbar_recalc - Recalculate the Window data - Implements MuttWindow::recalc()
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
  }

  return 0;
}

/**
 * cbar_repaint - Repaint the Window - Implements MuttWindow::repaint()
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

  return 0;
}

/**
 * cbar_color_observer - Listen for changes to the Colours - Implements ::observer_t
 */
int cbar_color_observer(struct NotifyCallback *nc)
{
  if (!nc->event_data || !nc->global_data)
    return -1;
  if (nc->event_type != NT_COLOR)
    return 0;

  struct EventColor *ev_c = nc->event_data;

  // MT_COLOR_MAX is sent on `uncolor *`
  if ((ev_c->color != MT_COLOR_STATUS) && (ev_c->color != MT_COLOR_MAX))
    return 0;

  struct MuttWindow *win_cbar = nc->global_data;
  win_cbar->actions |= WA_REPAINT;

  return 0;
}

/**
 * cbar_compose_observer - Listen for changes to the Colours - Implements ::observer_t
 */
int cbar_compose_observer(struct NotifyCallback *nc)
{
  if (!nc->event_data || !nc->global_data)
    return -1;
  if (nc->event_type != NT_COMPOSE)
    return 0;

  struct MuttWindow *win_cbar = nc->global_data;
  win_cbar->actions |= WA_REPAINT;

  return 0;
}

/**
 * cbar_config_observer - Listen for changes to the Config - Implements ::observer_t
 */
int cbar_config_observer(struct NotifyCallback *nc)
{
  if (!nc->event_data || !nc->global_data)
    return -1;
  if (nc->event_type != NT_CONFIG)
    return 0;
  if (nc->event_subtype == NT_CONFIG_INITIAL_SET)
    return 0;

  struct EventConfig *ev_c = nc->event_data;
  if (!mutt_str_equal(ev_c->name, "compose_format"))
    return 0;

  struct MuttWindow *win_cbar = nc->global_data;
  win_cbar->actions |= WA_RECALC;

  return 0;
}

/**
 * cbar_window_observer - Listen for changes to the Window - Implements ::observer_t
 */
int cbar_window_observer(struct NotifyCallback *nc)
{
  if ((nc->event_type != NT_WINDOW) || !nc->event_data || !nc->global_data)
    return -1;

  struct MuttWindow *win_cbar = nc->global_data;

  if (nc->event_subtype == NT_WINDOW_STATE)
  {
    win_cbar->actions |= WA_RECALC;
    mutt_debug(LL_NOTIFY, "state change, request WA_RECALC\n");
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
