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
 * Pager Bar
 */

#include "config.h"
#include <inttypes.h> // IWYU pragma: keep
#include <stdio.h>
#include <sys/stat.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "pbar.h"
#include "lib.h"
#include "index/lib.h"
#include "context.h"
#include "format_flags.h"
#include "hdrline.h"
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
 * pbar_recalc - Recalculate the Window data - Implements MuttWindow::recalc()
 */
static int pbar_recalc(struct MuttWindow *win)
{
  if (!mutt_window_is_visible(win))
    return 0;

  char buf[1024] = { 0 };

  struct PBarPrivateData *pbar_data = win->wdata;
  struct IndexSharedData *shared = pbar_data->shared;
  struct PagerPrivateData *priv = pbar_data->priv;

  char pager_progress_str[65]; /* Lots of space for translations */
  if (priv->last_pos < priv->sb.st_size - 1)
  {
    snprintf(pager_progress_str, sizeof(pager_progress_str), OFF_T_FMT "%%",
             (100 * priv->last_offset / priv->sb.st_size));
  }
  else
  {
    const char *msg = (priv->topline == 0) ?
                          /* L10N: Status bar message: the entire email is visible in the pager */
                          _("all") :
                          /* L10N: Status bar message: the end of the email is visible in the pager */
                          _("end");
    mutt_str_copy(pager_progress_str, msg, sizeof(pager_progress_str));
  }

  if ((priv->pview->mode == PAGER_MODE_EMAIL) || (priv->pview->mode == PAGER_MODE_ATTACH_E))
  {
    int msg_in_pager = shared->ctx ? shared->ctx->msg_in_pager : -1;

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
 * pbar_repaint - Repaint the Window - Implements MuttWindow::repaint()
 */
static int pbar_repaint(struct MuttWindow *win)
{
  if (!mutt_window_is_visible(win))
    return 0;

  struct PBarPrivateData *pbar_data = win->wdata;
  // struct IndexSharedData *shared = pbar_data->shared;

  mutt_window_move(win, 0, 0);
  mutt_curses_set_color(MT_COLOR_STATUS);
  mutt_window_clrtoeol(win);

  mutt_window_move(win, 0, 0);
  mutt_draw_statusline(win, win->state.cols, pbar_data->pager_format,
                       mutt_str_len(pbar_data->pager_format));
  mutt_curses_set_color(MT_COLOR_NORMAL);

  return 0;
}

/**
 * pbar_color_observer - Notification that a Color has changed - Implements ::observer_t
 */
static int pbar_color_observer(struct NotifyCallback *nc)
{
  if ((nc->event_type != NT_COLOR) || !nc->global_data || !nc->event_data)
    return -1;

  struct EventColor *ev_c = nc->event_data;
  enum ColorId color = ev_c->color;

  if (color != MT_COLOR_STATUS)
    return 0;

  struct MuttWindow *win_pbar = nc->global_data;
  win_pbar->actions |= WA_REPAINT;
  mutt_debug(LL_DEBUG5, "color done, request WA_REPAINT\n");

  return 0;
}

/**
 * pbar_config_observer - Notification that a Config Variable has changed - Implements ::observer_t
 */
static int pbar_config_observer(struct NotifyCallback *nc)
{
  if ((nc->event_type != NT_CONFIG) || !nc->global_data || !nc->event_data)
    return -1;

  if (nc->event_subtype == NT_CONFIG_INITIAL_SET)
    return 0;

  struct EventConfig *ev_c = nc->event_data;
  if (!mutt_str_equal(ev_c->name, "pager_format"))
    return 0;

  struct MuttWindow *win_pbar = nc->global_data;
  win_pbar->actions |= WA_RECALC;
  mutt_debug(LL_DEBUG5, "config done, request WA_RECALC\n");

  return 0;
}

/**
 * pbar_email_observer - Notification that an Email has changed - Implements ::observer_t
 */
static int pbar_email_observer(struct NotifyCallback *nc)
{
  if ((nc->event_type != NT_EMAIL) || !nc->global_data)
    return -1;

  struct MuttWindow *win_pbar = nc->global_data;
  win_pbar->actions |= WA_RECALC;
  mutt_debug(LL_DEBUG5, "email done, request WA_RECALC\n");

  return 0;
}

/**
 * pbar_mailbox_observer - Notification that a Mailbox has changed - Implements ::observer_t
 */
static int pbar_mailbox_observer(struct NotifyCallback *nc)
{
  if ((nc->event_type != NT_MAILBOX) || !nc->global_data)
    return -1;

  struct MuttWindow *win_pbar = nc->global_data;
  win_pbar->actions |= WA_RECALC;
  mutt_debug(LL_DEBUG5, "mailbox done, request WA_RECALC\n");

  return 0;
}

/**
 * pbar_pager_observer - Notification that the Pager has changed - Implements ::observer_t
 */
static int pbar_pager_observer(struct NotifyCallback *nc)
{
  if ((nc->event_type != NT_PAGER) || !nc->global_data)
    return -1;

  struct MuttWindow *win_pbar = nc->global_data;
  if (!win_pbar)
    return 0;

  struct IndexSharedData *old_shared = nc->event_data;
  if (!old_shared)
    return 0;

  struct PBarPrivateData *pbar_data = win_pbar->wdata;
  struct IndexSharedData *new_shared = pbar_data->shared;

  if (nc->event_subtype & NT_PAGER_MAILBOX)
  {
    if (old_shared->mailbox)
      notify_observer_remove(old_shared->mailbox->notify, pbar_mailbox_observer, win_pbar);
    if (new_shared->mailbox)
      notify_observer_add(new_shared->mailbox->notify, NT_MAILBOX,
                          pbar_mailbox_observer, win_pbar);
    win_pbar->actions |= WA_RECALC;
    mutt_debug(LL_DEBUG5, "pager done, request WA_RECALC\n");
  }

  if (nc->event_subtype & NT_PAGER_EMAIL)
  {
    if (old_shared->email)
      notify_observer_remove(old_shared->email->notify, pbar_email_observer, win_pbar);
    if (new_shared->email)
      notify_observer_add(new_shared->email->notify, NT_EMAIL, pbar_email_observer, win_pbar);
    win_pbar->actions |= WA_RECALC;
    mutt_debug(LL_DEBUG5, "pager done, request WA_RECALC\n");
  }

  return 0;
}

/**
 * pbar_menu_observer - Notification that a Menu has changed - Implements ::observer_t
 */
static int pbar_menu_observer(struct NotifyCallback *nc)
{
  if ((nc->event_type != NT_MENU) || !nc->global_data)
    return -1;

  struct MuttWindow *win_pbar = nc->global_data;
  win_pbar->actions |= WA_RECALC;
  mutt_debug(LL_DEBUG5, "menu done, request WA_RECALC\n");

  return 0;
}

/**
 * pbar_window_observer - Notification that a Window has changed - Implements ::observer_t
 */
static int pbar_window_observer(struct NotifyCallback *nc)
{
  if ((nc->event_type != NT_WINDOW) || !nc->global_data || !nc->event_data)
    return -1;

  struct MuttWindow *win_pbar = nc->global_data;

  if (nc->event_subtype == NT_WINDOW_STATE)
  {
    win_pbar->actions |= WA_RECALC;
    mutt_debug(LL_NOTIFY, "window state done, request WA_RECALC\n");
  }
  else if (nc->event_subtype == NT_WINDOW_DELETE)
  {
    struct PBarPrivateData *pbar_data = win_pbar->wdata;

    struct IndexSharedData *shared = pbar_data->shared;
    struct PagerPrivateData *priv = pbar_data->priv;

    notify_observer_remove(NeoMutt->notify, pbar_color_observer, win_pbar);
    notify_observer_remove(NeoMutt->notify, pbar_config_observer, win_pbar);
    notify_observer_remove(shared->notify, pbar_pager_observer, win_pbar);
    if (priv->win_pbar)
      notify_observer_remove(priv->win_pbar->parent->notify, pbar_menu_observer, win_pbar);

    if (shared->mailbox)
      notify_observer_remove(shared->mailbox->notify, pbar_mailbox_observer, win_pbar);
    if (shared->email)
      notify_observer_remove(shared->email->notify, pbar_email_observer, win_pbar);

    mutt_debug(LL_DEBUG5, "window delete done\n");
  }

  return 0;
}

/**
 * pbar_data_free - Free the private data attached to the MuttWindow - Implements MuttWindow::wdata_free()
 */
static void pbar_data_free(struct MuttWindow *win, void **ptr)
{
  struct PBarPrivateData *pbar_data = *ptr;

  FREE(&pbar_data->pager_format);

  FREE(ptr);
}

/**
 * pbar_data_new - Free the private data attached to the MuttWindow
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
 * pbar_new - Add the Pager Bar
 * @param parent Parent Window
 * @param shared Shared Pager data
 * @param priv   Private Pager data
 * @retval ptr New Pager Bar
 */
struct MuttWindow *pbar_new(struct MuttWindow *parent, struct IndexSharedData *shared,
                            struct PagerPrivateData *priv)
{
  struct MuttWindow *win_pbar =
      mutt_window_new(WT_STATUS_BAR, MUTT_WIN_ORIENT_VERTICAL,
                      MUTT_WIN_SIZE_FIXED, MUTT_WIN_SIZE_UNLIMITED, 1);

  win_pbar->wdata = pbar_data_new(shared, priv);
  win_pbar->wdata_free = pbar_data_free;
  win_pbar->recalc = pbar_recalc;
  win_pbar->repaint = pbar_repaint;

  notify_observer_add(NeoMutt->notify, NT_COLOR, pbar_color_observer, win_pbar);
  notify_observer_add(NeoMutt->notify, NT_CONFIG, pbar_config_observer, win_pbar);
  notify_observer_add(shared->notify, NT_PAGER, pbar_pager_observer, win_pbar);
  notify_observer_add(parent->notify, NT_MENU, pbar_menu_observer, win_pbar);
  notify_observer_add(win_pbar->notify, NT_WINDOW, pbar_window_observer, win_pbar);

  if (shared->mailbox)
    notify_observer_add(shared->mailbox->notify, NT_MAILBOX, pbar_mailbox_observer, win_pbar);
  if (shared->email)
    notify_observer_add(shared->email->notify, NT_EMAIL, pbar_email_observer, win_pbar);

  return win_pbar;
}
