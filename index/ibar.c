/**
 * @file
 * Index Bar (status)
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
 * @page index_ibar Index Bar (status)
 *
 * Index Bar (status)
 */

#include "config.h"
#include <stdbool.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "lib.h"
#include "private_data.h"
#include "shared_data.h"
#include "status.h"

/**
 * struct IBarPrivateData - Data to draw the Index Bar
 */
struct IBarPrivateData
{
  struct IndexSharedData *shared; ///< Shared Index data
  struct IndexPrivateData *priv;  ///< Private Index data
  char *status_format;            ///< Cached screen status string
  char *ts_status_format;         ///< Cached terminal status string
  char *ts_icon_format;           ///< Cached terminal icon string
};

/**
 * ibar_recalc - Recalculate the Window data - Implements MuttWindow::recalc()
 */
static int ibar_recalc(struct MuttWindow *win)
{
  char buf[1024] = { 0 };

  struct IBarPrivateData *ibar_data = win->wdata;
  struct IndexSharedData *shared = ibar_data->shared;
  struct IndexPrivateData *priv = ibar_data->priv;

  const char *c_status_format = cs_subset_string(shared->sub, "status_format");
  menu_status_line(buf, sizeof(buf), shared, priv->menu, win->state.cols,
                   NONULL(c_status_format));

  if (!mutt_str_equal(buf, ibar_data->status_format))
  {
    mutt_str_replace(&ibar_data->status_format, buf);
    win->actions |= WA_REPAINT;
  }

  const bool c_ts_enabled = cs_subset_bool(shared->sub, "ts_enabled");
  if (c_ts_enabled && TsSupported)
  {
    const char *c_ts_status_format =
        cs_subset_string(shared->sub, "ts_status_format");
    menu_status_line(buf, sizeof(buf), shared, priv->menu, sizeof(buf),
                     NONULL(c_ts_status_format));
    if (!mutt_str_equal(buf, ibar_data->ts_status_format))
    {
      mutt_str_replace(&ibar_data->ts_status_format, buf);
      win->actions |= WA_REPAINT;
    }

    const char *c_ts_icon_format =
        cs_subset_string(shared->sub, "ts_icon_format");
    menu_status_line(buf, sizeof(buf), shared, priv->menu, sizeof(buf),
                     NONULL(c_ts_icon_format));
    if (!mutt_str_equal(buf, ibar_data->ts_icon_format))
    {
      mutt_str_replace(&ibar_data->ts_icon_format, buf);
      win->actions |= WA_REPAINT;
    }
  }

  return 0;
}

/**
 * ibar_repaint - Repaint the Window - Implements MuttWindow::repaint()
 */
static int ibar_repaint(struct MuttWindow *win)
{
  if (!mutt_window_is_visible(win))
    return 0;

  struct IBarPrivateData *ibar_data = win->wdata;
  struct IndexSharedData *shared = ibar_data->shared;

  mutt_window_move(win, 0, 0);
  mutt_curses_set_color(MT_COLOR_STATUS);
  mutt_window_clrtoeol(win);

  mutt_window_move(win, 0, 0);
  mutt_draw_statusline(win, win->state.cols, ibar_data->status_format,
                       mutt_str_len(ibar_data->status_format));
  mutt_curses_set_color(MT_COLOR_NORMAL);

  const bool c_ts_enabled = cs_subset_bool(shared->sub, "ts_enabled");
  if (c_ts_enabled && TsSupported)
  {
    mutt_ts_status(ibar_data->ts_status_format);
    mutt_ts_icon(ibar_data->ts_icon_format);
  }

  return 0;
}

/**
 * ibar_color_observer - Listen for changes to the Colours - Implements ::observer_t
 */
static int ibar_color_observer(struct NotifyCallback *nc)
{
  if (!nc->event_data || !nc->global_data)
    return -1;
  if (nc->event_type != NT_COLOR)
    return 0;

  struct EventColor *ev_c = nc->event_data;
  enum ColorId color = ev_c->color;

  if (color != MT_COLOR_STATUS)
    return 0;

  struct MuttWindow *win_ibar = nc->global_data;
  win_ibar->actions |= WA_REPAINT;

  return 0;
}

/**
 * ibar_config_observer - Listen for changes to the Config - Implements ::observer_t
 */
static int ibar_config_observer(struct NotifyCallback *nc)
{
  if (!nc->event_data || !nc->global_data)
    return -1;
  if (nc->event_type != NT_CONFIG)
    return 0;
  if (nc->event_subtype == NT_CONFIG_INITIAL_SET)
    return 0;

  struct EventConfig *ev_c = nc->event_data;
  if ((ev_c->name[0] != 's') && (ev_c->name[0] != 't'))
    return 0;

  if (!mutt_str_equal(ev_c->name, "status_format") &&
      !mutt_str_equal(ev_c->name, "ts_enabled") &&
      !mutt_str_equal(ev_c->name, "ts_icon_format") &&
      !mutt_str_equal(ev_c->name, "ts_status_format"))
  {
    return 0;
  }

  struct MuttWindow *win_ibar = nc->global_data;
  win_ibar->actions |= WA_RECALC;

  return 0;
}

/**
 * ibar_mailbox_observer - Listen for changes to the Mailbox - Implements ::observer_t
 */
static int ibar_mailbox_observer(struct NotifyCallback *nc)
{
  if (!nc->global_data)
    return -1;
  if (nc->event_type != NT_MAILBOX)
    return 0;

  struct MuttWindow *win_ibar = nc->global_data;
  win_ibar->actions |= WA_RECALC;

  return 0;
}

/**
 * ibar_index_observer - Listen for changes to the Index - Implements ::observer_t
 */
static int ibar_index_observer(struct NotifyCallback *nc)
{
  if (!nc->global_data)
    return -1;
  if (nc->event_type != NT_INDEX)
    return 0;

  struct MuttWindow *win_ibar = nc->global_data;
  if (!win_ibar)
    return 0;

  struct IndexSharedData *old_shared = nc->event_data;
  if (!old_shared)
    return 0;

  struct IBarPrivateData *ibar_data = win_ibar->wdata;
  struct IndexSharedData *new_shared = ibar_data->shared;

  if (nc->event_subtype & NT_INDEX_MAILBOX)
  {
    if (old_shared->mailbox)
      notify_observer_remove(old_shared->mailbox->notify, ibar_mailbox_observer, win_ibar);
    if (new_shared->mailbox)
      notify_observer_add(new_shared->mailbox->notify, NT_MAILBOX,
                          ibar_mailbox_observer, win_ibar);
    win_ibar->actions |= WA_RECALC;
  }

  if (nc->event_subtype & NT_INDEX_EMAIL)
  {
    win_ibar->actions |= WA_RECALC;
  }

  return 0;
}

/**
 * ibar_menu_observer - Listen for changes to the Menu - Implements ::observer_t
 */
static int ibar_menu_observer(struct NotifyCallback *nc)
{
  if (!nc->global_data)
    return -1;
  if (nc->event_type != NT_MENU)
    return 0;

  struct MuttWindow *win_ibar = nc->global_data;
  win_ibar->actions |= WA_RECALC;

  return 0;
}

/**
 * ibar_window_observer - Listen for changes to the Window - Implements ::observer_t
 */
static int ibar_window_observer(struct NotifyCallback *nc)
{
  if (!nc->global_data)
    return -1;
  if (nc->event_type != NT_WINDOW)
    return 0;

  struct MuttWindow *win_ibar = nc->global_data;
  win_ibar->actions |= WA_REPAINT;

  return 0;
}

/**
 * ibar_data_free - Free the private data attached to the MuttWindow - Implements MuttWindow::wdata_free()
 */
static void ibar_data_free(struct MuttWindow *win, void **ptr)
{
  struct IBarPrivateData *ibar_data = *ptr;
  struct IndexSharedData *shared = ibar_data->shared;
  struct IndexPrivateData *priv = ibar_data->priv;

  notify_observer_remove(NeoMutt->notify, ibar_color_observer, win);
  notify_observer_remove(NeoMutt->notify, ibar_config_observer, win);
  notify_observer_remove(shared->notify, ibar_index_observer, win);
  notify_observer_remove(priv->win_ibar->parent->notify, ibar_menu_observer, win);
  notify_observer_remove(win->notify, ibar_window_observer, win);

  if (shared->mailbox)
    notify_observer_remove(shared->mailbox->notify, ibar_mailbox_observer, win);

  FREE(&ibar_data->status_format);
  FREE(&ibar_data->ts_status_format);
  FREE(&ibar_data->ts_icon_format);

  FREE(ptr);
}

/**
 * ibar_data_new - Free the private data attached to the MuttWindow
 */
static struct IBarPrivateData *ibar_data_new(struct IndexSharedData *shared,
                                             struct IndexPrivateData *priv)
{
  struct IBarPrivateData *ibar_data = mutt_mem_calloc(1, sizeof(struct IBarPrivateData));

  ibar_data->shared = shared;
  ibar_data->priv = priv;

  return ibar_data;
}

/**
 * ibar_new - Create the Index Bar (status)
 * @param parent Parent Window
 * @param shared Shared Index data
 * @param priv   Private Index data
 * @retval ptr New Index Bar
 */
struct MuttWindow *ibar_new(struct MuttWindow *parent, struct IndexSharedData *shared,
                            struct IndexPrivateData *priv)
{
  struct MuttWindow *win_ibar =
      mutt_window_new(WT_STATUS_BAR, MUTT_WIN_ORIENT_VERTICAL,
                      MUTT_WIN_SIZE_FIXED, MUTT_WIN_SIZE_UNLIMITED, 1);

  win_ibar->wdata = ibar_data_new(shared, priv);
  win_ibar->wdata_free = ibar_data_free;
  win_ibar->recalc = ibar_recalc;
  win_ibar->repaint = ibar_repaint;

  notify_observer_add(NeoMutt->notify, NT_COLOR, ibar_color_observer, win_ibar);
  notify_observer_add(NeoMutt->notify, NT_CONFIG, ibar_config_observer, win_ibar);
  notify_observer_add(shared->notify, NT_INDEX, ibar_index_observer, win_ibar);
  notify_observer_add(parent->notify, NT_MENU, ibar_menu_observer, win_ibar);
  notify_observer_add(win_ibar->notify, NT_WINDOW, ibar_window_observer, win_ibar);

  if (shared->mailbox)
    notify_observer_add(shared->mailbox->notify, NT_MAILBOX, ibar_mailbox_observer, win_ibar);

  return win_ibar;
}
