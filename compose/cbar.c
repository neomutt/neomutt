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
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "index/lib.h"
#include "menu/lib.h"
#include "send/lib.h"
#include "format_flags.h"
#include "mutt_globals.h"
#include "muttlib.h"
#include "redraw.h"

/**
 * struct CBarPrivateData - Data to draw the Compose Bar
 */
struct CBarPrivateData
{
  struct ComposeRedrawData *rd; ///< Shared Compose data
  char *compose_format;         ///< Cached status string
};

/**
 * cum_attachs_size - Cumulative Attachments Size
 * @param menu Menu listing attachments
 * @retval num Bytes in attachments
 *
 * Returns the total number of bytes used by the attachments in the attachment
 * list _after_ content-transfer-encodings have been applied.
 */
static unsigned long cum_attachs_size(struct Menu *menu)
{
  size_t s = 0;
  struct Content *info = NULL;
  struct Body *b = NULL;
  struct ComposeRedrawData *rd = menu->mdata;
  struct AttachCtx *actx = rd->actx;
  struct AttachPtr **idx = actx->idx;
  struct ConfigSubset *sub = rd->sub;

  for (unsigned short i = 0; i < actx->idxlen; i++)
  {
    b = idx[i]->body;

    if (!b->content)
      b->content = mutt_get_content_info(b->filename, b, sub);

    info = b->content;
    if (info)
    {
      switch (b->encoding)
      {
        case ENC_QUOTED_PRINTABLE:
          s += 3 * (info->lobin + info->hibin) + info->ascii + info->crlf;
          break;
        case ENC_BASE64:
          s += (4 * (info->lobin + info->hibin + info->ascii + info->crlf)) / 3;
          break;
        default:
          s += info->lobin + info->hibin + info->ascii + info->crlf;
          break;
      }
    }
  }

  return s;
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
  struct Menu *menu = (struct Menu *) data;

  *buf = '\0';
  switch (op)
  {
    case 'a': /* total number of attachments */
      snprintf(fmt, sizeof(fmt), "%%%sd", prec);
      snprintf(buf, buflen, fmt, menu->max);
      break;

    case 'h': /* hostname */
      snprintf(fmt, sizeof(fmt), "%%%ss", prec);
      snprintf(buf, buflen, fmt, NONULL(ShortHostname));
      break;

    case 'l': /* approx length of current message in bytes */
      snprintf(fmt, sizeof(fmt), "%%%ss", prec);
      mutt_str_pretty_size(tmp, sizeof(tmp), menu ? cum_attachs_size(menu) : 0);
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
  struct CBarPrivateData *cbar_data = win->wdata;
  struct ComposeRedrawData *rd = cbar_data->rd;

  char buf[1024] = { 0 };
  const char *const c_compose_format =
      cs_subset_string(rd->sub, "compose_format");
  mutt_expando_format(buf, sizeof(buf), 0, win->state.cols, NONULL(c_compose_format),
                      compose_format_str, (intptr_t) rd->menu, MUTT_FORMAT_NO_FLAGS);

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

  struct CBarPrivateData *cbar_data = win->wdata;

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
static int cbar_color_observer(struct NotifyCallback *nc)
{
  if (!nc->event_data || !nc->global_data)
    return -1;
  if (nc->event_type != NT_COLOR)
    return 0;

  struct EventColor *ev_c = nc->event_data;
  enum ColorId color = ev_c->color;

  if (color != MT_COLOR_STATUS)
    return 0;

  struct MuttWindow *win_cbar = nc->global_data;
  win_cbar->actions |= WA_REPAINT;

  return 0;
}

/**
 * cbar_config_observer - Listen for changes to the Config - Implements ::observer_t
 */
static int cbar_config_observer(struct NotifyCallback *nc)
{
  if (!nc->event_data || !nc->global_data)
    return -1;
  if (nc->event_type != NT_CONFIG)
    return 0;
  if (nc->event_subtype == NT_CONFIG_INITIAL_SET)
    return 0;

  struct EventConfig *ec = nc->event_data;
  if (!mutt_str_equal(ec->name, "compose_format"))
    return 0;

  struct MuttWindow *win_cbar = nc->global_data;
  win_cbar->actions |= WA_RECALC;

  return 0;
}

/**
 * cbar_data_free - Free the private data attached to the MuttWindow - Implements MuttWindow::wdata_free()
 */
static void cbar_data_free(struct MuttWindow *win, void **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct CBarPrivateData *cbar_data = *ptr;

  notify_observer_remove(NeoMutt->notify, cbar_color_observer, win);
  notify_observer_remove(NeoMutt->notify, cbar_config_observer, win);

  FREE(&cbar_data->compose_format);

  FREE(ptr);
}

/**
 * cbar_data_new - Free the private data attached to the MuttWindow
 */
static struct CBarPrivateData *cbar_data_new(struct ComposeRedrawData *rd)
{
  struct CBarPrivateData *cbar_data = mutt_mem_calloc(1, sizeof(struct CBarPrivateData));

  cbar_data->rd = rd;

  return cbar_data;
}

/**
 * cbar_new - Create the Compose Bar (status)
 * @param parent Parent Window
 * @param rd     Redraw data
 */
struct MuttWindow *cbar_new(struct MuttWindow *parent, struct ComposeRedrawData *rd)
{
  struct MuttWindow *win_cbar =
      mutt_window_new(WT_STATUS_BAR, MUTT_WIN_ORIENT_VERTICAL,
                      MUTT_WIN_SIZE_FIXED, MUTT_WIN_SIZE_UNLIMITED, 1);

  win_cbar->wdata = cbar_data_new(rd);
  win_cbar->wdata_free = cbar_data_free;
  win_cbar->recalc = cbar_recalc;
  win_cbar->repaint = cbar_repaint;

  notify_observer_add(NeoMutt->notify, NT_COLOR, cbar_color_observer, win_cbar);
  notify_observer_add(NeoMutt->notify, NT_CONFIG, cbar_config_observer, win_cbar);

  return win_cbar;
}
