/**
 * @file
 * Simple Pager Dialog
 *
 * @authors
 * Copyright (C) 1996-2002,2007,2010,2012-2013 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2020 R Primus <rprimus@gmail.com>
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
 * @page pager_dopager Simple Pager Dialog
 *
 * The Simple Pager Dialog displays text to the user that can be paged.
 *
 * ## Windows
 *
 * | Name                | Type             | Constructor     |
 * | :------------------ | :--------------- | :-------------- |
 * | Simple Pager Dialog | #WT_DLG_PAGER    | mutt_do_pager() |
 *
 * **Parent**
 * - @ref gui_dialog
 *
 * **Children**
 * - @ref pager_ppanel
 *
 * ## Data
 *
 * The Simple Pager Dialog has no data.
 *
 * ## Events
 *
 * Once constructed, it is controlled by the following events:
 *
 * | Event Type  | Handler                   |
 * | :---------- | :------------------------ |
 * | #NT_CONFIG  | dopager_config_observer() |
 * | #NT_WINDOW  | dopager_window_observer() |
 *
 * The Simple Pager Dialog does not implement MuttWindow::recalc() or
 * MuttWindow::repaint().
 */

#include "config.h"
#include <stddef.h>
#include <assert.h>
#include <stdbool.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "lib.h"
#include "index/lib.h"
#include "protos.h"

struct Email;

/**
 * dopager_config_observer - Notification that a Config Variable has changed - Implements ::observer_t - @ingroup observer_api
 */
static int dopager_config_observer(struct NotifyCallback *nc)
{
  if (nc->event_type != NT_CONFIG)
    return 0;
  if (!nc->global_data || !nc->event_data)
    return -1;

  struct EventConfig *ev_c = nc->event_data;
  if (!mutt_str_equal(ev_c->name, "status_on_top"))
    return 0;

  struct MuttWindow *dlg = nc->global_data;
  window_status_on_top(dlg, NeoMutt->sub);
  mutt_debug(LL_DEBUG5, "config done, request WA_REFLOW\n");
  return 0;
}

/**
 * dopager_window_observer - Notification that a Window has changed - Implements ::observer_t - @ingroup observer_api
 */
static int dopager_window_observer(struct NotifyCallback *nc)
{
  if (nc->event_type != NT_WINDOW)
    return 0;
  if (!nc->global_data || !nc->event_data)
    return -1;
  if (nc->event_subtype != NT_WINDOW_DELETE)
    return 0;

  struct MuttWindow *dlg = nc->global_data;
  struct EventWindow *ev_w = nc->event_data;
  if (ev_w->win != dlg)
    return 0;

  notify_observer_remove(NeoMutt->sub->notify, dopager_config_observer, dlg);
  notify_observer_remove(dlg->notify, dopager_window_observer, dlg);
  mutt_debug(LL_DEBUG5, "window delete done\n");

  return 0;
}

/**
 * mutt_do_pager - Display some page-able text to the user (help or attachment)
 * @param pview PagerView to construct Pager object
 * @param e     Email to use
 * @retval  0 Success
 * @retval -1 Error
 */
int mutt_do_pager(struct PagerView *pview, struct Email *e)
{
  assert(pview);
  assert(pview->pdata);
  assert(pview->pdata->fname);
  assert((pview->mode == PAGER_MODE_ATTACH) ||
         (pview->mode == PAGER_MODE_HELP) || (pview->mode == PAGER_MODE_OTHER));

  struct MuttWindow *dlg = mutt_window_new(WT_DLG_PAGER, MUTT_WIN_ORIENT_VERTICAL,
                                           MUTT_WIN_SIZE_MAXIMISE, MUTT_WIN_SIZE_UNLIMITED,
                                           MUTT_WIN_SIZE_UNLIMITED);

  struct IndexSharedData *shared = index_shared_data_new();
  shared->email = e;

  notify_set_parent(shared->notify, dlg->notify);

  dlg->wdata = shared;
  dlg->wdata_free = index_shared_data_free;

  const bool c_status_on_top = cs_subset_bool(NeoMutt->sub, "status_on_top");
  struct MuttWindow *panel_pager = ppanel_new(c_status_on_top, shared);
  dlg->focus = panel_pager;
  mutt_window_add_child(dlg, panel_pager);

  notify_observer_add(NeoMutt->sub->notify, NT_CONFIG, dopager_config_observer, dlg);
  notify_observer_add(dlg->notify, NT_WINDOW, dopager_window_observer, dlg);
  dialog_push(dlg);

  pview->win_index = NULL;
  pview->win_pbar = window_find_child(panel_pager, WT_STATUS_BAR);
  pview->win_pager = window_find_child(panel_pager, WT_CUSTOM);

  int rc;

  const char *const c_pager = pager_get_pager(NeoMutt->sub);
  if (c_pager)
  {
    struct Buffer *cmd = buf_pool_get();

    mutt_endwin();
    buf_file_expand_fmt_quote(cmd, c_pager, pview->pdata->fname);
    if (mutt_system(buf_string(cmd)) == -1)
    {
      mutt_error(_("Error running \"%s\""), buf_string(cmd));
      rc = -1;
    }
    else
    {
      rc = 0;
    }
    mutt_file_unlink(pview->pdata->fname);
    buf_pool_release(&cmd);
  }
  else
  {
    rc = dlg_pager(pview);
  }

  dialog_pop();
  mutt_window_free(&dlg);
  return rc;
}
