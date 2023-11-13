/**
 * @file
 * Pager Bar
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
 * @page pager_pbar Pager Bar
 *
 * The Pager Bar Window displays status info about the email.
 *
 * ## Windows
 *
 * | Name             | Type          | See Also   |
 * | :--------------- | :------------ | :--------- |
 * | Pager Bar Window | WT_STATUS_BAR | pbar_new() |
 *
 * **Parent**
 * - @ref pager_ppanel
 *
 * **Children**
 *
 * None.
 *
 * ## Data
 * - #PBarPrivateData
 *
 * The Pager Bar Window stores its data (#PBarPrivateData) in
 * MuttWindow::wdata.
 *
 * ## Events
 *
 * Once constructed, it is controlled by the following events:
 *
 * | Event Type            | Handler                 |
 * | :-------------------- | :---------------------- |
 * | #NT_COLOR             | pbar_color_observer()   |
 * | #NT_CONFIG            | pbar_config_observer()  |
 * | #NT_PAGER             | pbar_pager_observer()   |
 * | #NT_INDEX             | pbar_index_observer()   |
 * | #NT_WINDOW            | pbar_window_observer()  |
 * | MuttWindow::recalc()  | pbar_recalc()           |
 * | MuttWindow::repaint() | pbar_repaint()          |
 */

#include "config.h"
#include <stdio.h>
#include <sys/stat.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "pbar.h"
#include "lib.h"
#include "color/lib.h"
#include "index/lib.h"
#include "display.h"
#include "format_flags.h"
#include "hdrline.h"
#include "mview.h"
#include "private_data.h"

/**
 * struct PBarPrivateData - Data to draw the Pager Bar
 */
struct PBarPrivateData
{
  struct IndexSharedData *shared; ///< Shared Index data
  struct PagerPrivateData *priv;  ///< Private Pager data
  char *pager_format;             ///< Cached status string
};

/**
 * pbar_recalc - Recalculate the Window data - Implements MuttWindow::recalc() - @ingroup window_recalc
 */
static int pbar_recalc(struct MuttWindow *win)
{
  char buf[1024] = { 0 };

  struct PBarPrivateData *pbar_data = win->wdata;
  struct IndexSharedData *shared = pbar_data->shared;
  struct PagerPrivateData *priv = pbar_data->priv;
  if (!priv || !priv->pview)
    return 0;

  char pager_progress_str[65] = { 0 }; /* Lots of space for translations */

  long offset;
  if (priv->lines && (priv->cur_line <= priv->lines_used))
    offset = priv->lines[priv->cur_line].offset;
  else
    offset = priv->bytes_read;

  if (offset < (priv->st.st_size - 1))
  {
    const long percent = (100 * offset) / priv->st.st_size;
    /* L10N: Pager position percentage.
       `%ld` is the number, `%%` is the percent symbol.
       They may be reordered, or space inserted, if you wish. */
    snprintf(pager_progress_str, sizeof(pager_progress_str), _("%ld%%"), percent);
  }
  else
  {
    const char *msg = (priv->top_line == 0) ?
                          /* L10N: Status bar message: the entire email is visible in the pager */
                          _("all") :
                          /* L10N: Status bar message: the end of the email is visible in the pager */
                          _("end");
    mutt_str_copy(pager_progress_str, msg, sizeof(pager_progress_str));
  }

  if ((priv->pview->mode == PAGER_MODE_EMAIL) || (priv->pview->mode == PAGER_MODE_ATTACH_E))
  {
    int msg_in_pager = shared->mailbox_view ? shared->mailbox_view->msg_in_pager : -1;

    const char *c_pager_format = cs_subset_string(shared->sub, "pager_format");
    mutt_make_string(buf, sizeof(buf), win->state.cols, NONULL(c_pager_format),
                     shared->mailbox, msg_in_pager, shared->email,
                     MUTT_FORMAT_NO_FLAGS, pager_progress_str);
  }
  else
  {
    snprintf(buf, sizeof(buf), "%s (%s)", priv->pview->banner, pager_progress_str);
  }

  if (!mutt_str_equal(buf, pbar_data->pager_format))
  {
    mutt_str_replace(&pbar_data->pager_format, buf);
    win->actions |= WA_REPAINT;
  }

  return 0;
}

/**
 * pbar_repaint - Repaint the Window - Implements MuttWindow::repaint() - @ingroup window_repaint
 */
static int pbar_repaint(struct MuttWindow *win)
{
  struct PBarPrivateData *pbar_data = win->wdata;

  mutt_window_move(win, 0, 0);
  mutt_curses_set_normal_backed_color_by_id(MT_COLOR_STATUS);
  mutt_window_clrtoeol(win);

  mutt_window_move(win, 0, 0);
  mutt_draw_statusline(win, win->state.cols, pbar_data->pager_format,
                       mutt_str_len(pbar_data->pager_format));
  mutt_curses_set_color_by_id(MT_COLOR_NORMAL);

  mutt_debug(LL_DEBUG5, "repaint done\n");
  return 0;
}

/**
 * pbar_color_observer - Notification that a Color has changed - Implements ::observer_t - @ingroup observer_api
 */
static int pbar_color_observer(struct NotifyCallback *nc)
{
  if (nc->event_type != NT_COLOR)
    return 0;
  if (!nc->global_data || !nc->event_data)
    return -1;

  struct EventColor *ev_c = nc->event_data;
  enum ColorId cid = ev_c->cid;

  if ((cid != MT_COLOR_STATUS) && (cid != MT_COLOR_NORMAL) && (cid != MT_COLOR_MAX))
    return 0;

  struct MuttWindow *win_pbar = nc->global_data;
  win_pbar->actions |= WA_REPAINT;
  mutt_debug(LL_DEBUG5, "color done, request WA_REPAINT\n");

  return 0;
}

/**
 * pbar_config_observer - Notification that a Config Variable has changed - Implements ::observer_t - @ingroup observer_api
 */
static int pbar_config_observer(struct NotifyCallback *nc)
{
  if (nc->event_type != NT_CONFIG)
    return 0;
  if (!nc->global_data || !nc->event_data)
    return -1;

  struct EventConfig *ev_c = nc->event_data;
  if (!mutt_str_equal(ev_c->name, "pager_format"))
    return 0;

  struct MuttWindow *win_pbar = nc->global_data;
  win_pbar->actions |= WA_RECALC;
  mutt_debug(LL_DEBUG5, "config done, request WA_RECALC\n");

  return 0;
}

/**
 * pbar_index_observer - Notification that the Index has changed - Implements ::observer_t - @ingroup observer_api
 *
 * This function receives two sorts of notification:
 * - NT_INDEX:
 *   User has changed to a different Mailbox/Email
 * - NT_CONTEXT/NT_ACCOUNT/NT_MAILBOX/NT_EMAIL:
 *   The state of an object has changed
 */
static int pbar_index_observer(struct NotifyCallback *nc)
{
  if (!nc->global_data)
    return -1;

  struct MuttWindow *win_pbar = nc->global_data;
  win_pbar->actions |= WA_RECALC;
  mutt_debug(LL_DEBUG5, "index done, request WA_RECALC\n");

  return 0;
}

/**
 * pbar_pager_observer - Notification that the Pager has changed - Implements ::observer_t - @ingroup observer_api
 */
static int pbar_pager_observer(struct NotifyCallback *nc)
{
  if (nc->event_type != NT_PAGER)
    return 0;
  if (!nc->global_data)
    return -1;

  struct MuttWindow *win_pbar = nc->global_data;

  if (nc->event_subtype & NT_PAGER_VIEW)
  {
    win_pbar->actions |= WA_RECALC;
    mutt_debug(LL_DEBUG5, "pager done, request WA_RECALC\n");
  }

  return 0;
}

/**
 * pbar_window_observer - Notification that a Window has changed - Implements ::observer_t - @ingroup observer_api
 */
static int pbar_window_observer(struct NotifyCallback *nc)
{
  if (nc->event_type != NT_WINDOW)
    return 0;
  if (!nc->global_data || !nc->event_data)
    return -1;

  struct MuttWindow *win_pbar = nc->global_data;
  struct EventWindow *ev_w = nc->event_data;
  if (ev_w->win != win_pbar)
    return 0;

  if (nc->event_subtype == NT_WINDOW_STATE)
  {
    win_pbar->actions |= WA_RECALC | WA_REPAINT;
    mutt_debug(LL_NOTIFY, "window state done, request WA_RECALC\n");
  }
  else if (nc->event_subtype == NT_WINDOW_DELETE)
  {
    struct PBarPrivateData *pbar_data = win_pbar->wdata;
    struct IndexSharedData *shared = pbar_data->shared;

    mutt_color_observer_remove(pbar_color_observer, win_pbar);
    notify_observer_remove(NeoMutt->sub->notify, pbar_config_observer, win_pbar);
    notify_observer_remove(shared->notify, pbar_index_observer, win_pbar);
    notify_observer_remove(pbar_data->priv->notify, pbar_pager_observer, win_pbar);
    notify_observer_remove(win_pbar->notify, pbar_window_observer, win_pbar);

    mutt_debug(LL_DEBUG5, "window delete done\n");
  }

  return 0;
}

/**
 * pbar_data_free - Free the private data - Implements MuttWindow::wdata_free() - @ingroup window_wdata_free
 */
static void pbar_data_free(struct MuttWindow *win, void **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct PBarPrivateData *pbar_data = *ptr;

  FREE(&pbar_data->pager_format);

  FREE(ptr);
}

/**
 * pbar_data_new - Create new private data
 * @param shared Shared Index data
 * @param priv   Private Index data
 * @retval ptr New PBar
 */
static struct PBarPrivateData *pbar_data_new(struct IndexSharedData *shared,
                                             struct PagerPrivateData *priv)
{
  struct PBarPrivateData *pbar_data = mutt_mem_calloc(1, sizeof(struct PBarPrivateData));

  pbar_data->shared = shared;
  pbar_data->priv = priv;

  return pbar_data;
}

/**
 * pbar_new - Create the Pager Bar
 * @param shared Shared Pager data
 * @param priv   Private Pager data
 * @retval ptr New Pager Bar
 */
struct MuttWindow *pbar_new(struct IndexSharedData *shared, struct PagerPrivateData *priv)
{
  struct MuttWindow *win_pbar = mutt_window_new(WT_STATUS_BAR, MUTT_WIN_ORIENT_VERTICAL,
                                                MUTT_WIN_SIZE_FIXED,
                                                MUTT_WIN_SIZE_UNLIMITED, 1);

  win_pbar->wdata = pbar_data_new(shared, priv);
  win_pbar->wdata_free = pbar_data_free;
  win_pbar->recalc = pbar_recalc;
  win_pbar->repaint = pbar_repaint;

  mutt_color_observer_add(pbar_color_observer, win_pbar);
  notify_observer_add(NeoMutt->sub->notify, NT_CONFIG, pbar_config_observer, win_pbar);
  notify_observer_add(shared->notify, NT_ALL, pbar_index_observer, win_pbar);
  notify_observer_add(priv->notify, NT_PAGER, pbar_pager_observer, win_pbar);
  notify_observer_add(win_pbar->notify, NT_WINDOW, pbar_window_observer, win_pbar);

  return win_pbar;
}
