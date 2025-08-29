/**
 * @file
 * Pager Window
 *
 * @authors
 * Copyright (C) 2021-2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2023 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2023 Tóth János <gomba007@gmail.com>
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
 * @page pager_pager Pager Window
 *
 * The Pager Window displays an email to the user.
 *
 * ## Windows
 *
 * | Name         | Type      | See Also           |
 * | :----------- | :-------- | :----------------- |
 * | Pager Window | WT_CUSTOM | pager_window_new() |
 *
 * **Parent**
 * - @ref pager_ppanel
 *
 * **Children**
 *
 * None.
 *
 * ## Data
 * - #PagerPrivateData
 *
 * The Pager Window stores its data (#PagerPrivateData) in MuttWindow::wdata.
 *
 * ## Events
 *
 * Once constructed, it is controlled by the following events:
 *
 * | Event Type            | Handler                 |
 * | :-------------------- | :---------------------- |
 * | #NT_COLOR             | pager_color_observer()  |
 * | #NT_CONFIG            | pager_config_observer() |
 * | #NT_INDEX             | pager_index_observer()  |
 * | #NT_PAGER             | pager_pager_observer()  |
 * | #NT_WINDOW            | pager_window_observer() |
 * | MuttWindow::recalc()  | pager_recalc()          |
 * | MuttWindow::repaint() | pager_repaint()         |
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include <sys/stat.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "lib.h"
#include "color/lib.h"
#include "index/lib.h"
#include "display.h"
#include "private_data.h"

/**
 * config_pager_index_lines - React to changes to $pager_index_lines
 * @param win Pager Window
 * @retval  0 Successfully handled
 * @retval -1 Error
 */
static int config_pager_index_lines(struct MuttWindow *win)
{
  if (!mutt_window_is_visible(win))
    return 0;

  struct MuttWindow *dlg = dialog_find(win);
  struct MuttWindow *panel_index = window_find_child(dlg, WT_INDEX);
  struct MuttWindow *win_index = window_find_child(panel_index, WT_MENU);
  if (!win_index)
    return -1;

  const short c_pager_index_lines = cs_subset_number(NeoMutt->sub, "pager_index_lines");

  if (c_pager_index_lines > 0)
  {
    win_index->req_rows = c_pager_index_lines;
    win_index->size = MUTT_WIN_SIZE_FIXED;

    panel_index->size = MUTT_WIN_SIZE_MINIMISE;
    panel_index->state.visible = true;
  }
  else
  {
    win_index->req_rows = MUTT_WIN_SIZE_UNLIMITED;
    win_index->size = MUTT_WIN_SIZE_MAXIMISE;

    panel_index->size = MUTT_WIN_SIZE_MAXIMISE;
    panel_index->state.visible = false;
  }

  mutt_window_reflow(dlg);
  mutt_debug(LL_DEBUG5, "config, request WA_REFLOW\n");
  return 0;
}

/**
 * pager_recalc - Recalculate the Pager display - Implements MuttWindow::recalc() - @ingroup window_recalc
 */
static int pager_recalc(struct MuttWindow *win)
{
  win->actions |= WA_REPAINT;
  mutt_debug(LL_DEBUG5, "recalc done, request WA_REPAINT\n");
  return 0;
}

/**
 * pager_repaint - Repaint the Pager display - Implements MuttWindow::repaint() - @ingroup window_repaint
 */
static int pager_repaint(struct MuttWindow *win)
{
  struct PagerPrivateData *priv = win->wdata;
  if (!priv || !priv->pview || !priv->pview->pdata)
    return 0;

  dump_pager(priv);

  // We need to populate more lines, but not change position
  const bool repopulate = (priv->cur_line > priv->lines_used);
  if ((priv->redraw & PAGER_REDRAW_FLOW) || repopulate)
  {
    if (!(priv->pview->flags & MUTT_PAGER_RETWINCH))
    {
      priv->win_height = -1;
      for (int i = 0; i <= priv->top_line; i++)
        if (!priv->lines[i].cont_line)
          priv->win_height++;
      for (int i = 0; i < priv->lines_max; i++)
      {
        priv->lines[i].offset = 0;
        priv->lines[i].cid = -1;
        priv->lines[i].cont_line = false;
        priv->lines[i].syntax_arr_size = 0;
        priv->lines[i].search_arr_size = -1;
        priv->lines[i].quote = NULL;

        MUTT_MEM_REALLOC(&(priv->lines[i].syntax), 1, struct TextSyntax);
        if (priv->search_compiled && priv->lines[i].search)
          FREE(&(priv->lines[i].search));
      }

      if (!repopulate)
      {
        priv->lines_used = 0;
        priv->top_line = 0;
      }
    }
    int i = -1;
    int j = -1;
    const PagerFlags flags = priv->has_types | priv->search_flag |
                             (priv->pview->flags & MUTT_PAGER_NOWRAP) |
                             (priv->pview->flags & MUTT_PAGER_STRIPES);

    while (display_line(priv->fp, &priv->bytes_read, &priv->lines, ++i,
                        &priv->lines_used, &priv->lines_max, flags, &priv->quote_list,
                        &priv->q_level, &priv->force_redraw, &priv->search_re,
                        priv->pview->win_pager, &priv->ansi_list) == 0)
    {
      if (!priv->lines[i].cont_line && (++j == priv->win_height))
      {
        if (!repopulate)
          priv->top_line = i;
        if (!priv->search_flag)
          break;
      }
    }
  }

  if ((priv->redraw & PAGER_REDRAW_PAGER) || (priv->top_line != priv->old_top_line))
  {
    do
    {
      mutt_window_move(priv->pview->win_pager, 0, 0);
      priv->cur_line = priv->top_line;
      priv->old_top_line = priv->top_line;
      priv->win_height = 0;
      priv->force_redraw = false;

      while ((priv->win_height < priv->pview->win_pager->state.rows) &&
             (priv->lines[priv->cur_line].offset <= priv->st.st_size - 1))
      {
        const PagerFlags flags = (priv->pview->flags & MUTT_DISPLAYFLAGS) |
                                 priv->hide_quoted | priv->search_flag |
                                 (priv->pview->flags & MUTT_PAGER_NOWRAP) |
                                 (priv->pview->flags & MUTT_PAGER_STRIPES);

        if (display_line(priv->fp, &priv->bytes_read, &priv->lines, priv->cur_line,
                         &priv->lines_used, &priv->lines_max, flags, &priv->quote_list,
                         &priv->q_level, &priv->force_redraw, &priv->search_re,
                         priv->pview->win_pager, &priv->ansi_list) > 0)
        {
          priv->win_height++;
        }
        priv->cur_line++;
        mutt_window_move(priv->pview->win_pager, 0, priv->win_height);
      }
    } while (priv->force_redraw);

    const bool c_tilde = cs_subset_bool(NeoMutt->sub, "tilde");
    mutt_curses_set_normal_backed_color_by_id(MT_COLOR_TILDE);
    while (priv->win_height < priv->pview->win_pager->state.rows)
    {
      mutt_window_clrtoeol(priv->pview->win_pager);
      if (c_tilde)
        mutt_window_addch(priv->pview->win_pager, '~');
      priv->win_height++;
      mutt_window_move(priv->pview->win_pager, 0, priv->win_height);
    }
    mutt_curses_set_color_by_id(MT_COLOR_NORMAL);
  }

  priv->redraw = PAGER_REDRAW_NO_FLAGS;
  mutt_debug(LL_DEBUG5, "repaint done\n");
  return 0;
}

/**
 * pager_color_observer - Notification that a Color has changed - Implements ::observer_t - @ingroup observer_api
 */
static int pager_color_observer(struct NotifyCallback *nc)
{
  if (nc->event_type != NT_COLOR)
    return 0;
  if (!nc->global_data || !nc->event_data)
    return -1;

  struct EventColor *ev_c = nc->event_data;
  struct MuttWindow *win_pager = nc->global_data;
  struct PagerPrivateData *priv = win_pager->wdata;
  if (!priv)
    return 0;

  // MT_COLOR_MAX is sent on `uncolor *`
  if ((ev_c->cid == MT_COLOR_QUOTED) || (ev_c->cid == MT_COLOR_MAX))
  {
    // rework quoted colours
    qstyle_recolor(priv->quote_list);
  }

  if (ev_c->cid == MT_COLOR_MAX)
  {
    for (size_t i = 0; i < priv->lines_max; i++)
    {
      FREE(&(priv->lines[i].syntax));
    }
    priv->lines_used = 0;
  }

  mutt_debug(LL_DEBUG5, "color done\n");
  return 0;
}

/**
 * pager_config_observer - Notification that a Config Variable has changed - Implements ::observer_t - @ingroup observer_api
 */
static int pager_config_observer(struct NotifyCallback *nc)
{
  if (nc->event_type != NT_CONFIG)
    return 0;
  if (!nc->global_data || !nc->event_data)
    return -1;

  struct EventConfig *ev_c = nc->event_data;
  struct MuttWindow *win_pager = nc->global_data;

  if (mutt_str_equal(ev_c->name, "pager_index_lines"))
  {
    config_pager_index_lines(win_pager);
    mutt_debug(LL_DEBUG5, "config done\n");
  }

  return 0;
}

/**
 * pager_global_observer - Notification that a Global Event occurred - Implements ::observer_t - @ingroup observer_api
 */
static int pager_global_observer(struct NotifyCallback *nc)
{
  if (nc->event_type != NT_GLOBAL)
    return 0;
  if (!nc->global_data)
    return -1;
  if (nc->event_subtype != NT_GLOBAL_COMMAND)
    return 0;

  struct MuttWindow *win_pager = nc->global_data;

  struct PagerPrivateData *priv = win_pager->wdata;
  const struct PagerView *pview = priv ? priv->pview : NULL;
  if (priv && pview && (priv->redraw & PAGER_REDRAW_FLOW) && (pview->flags & MUTT_PAGER_RETWINCH))
  {
    priv->rc = OP_REFORMAT_WINCH;
  }

  return 0;
}

/**
 * pager_index_observer - Notification that the Index has changed - Implements ::observer_t - @ingroup observer_api
 */
static int pager_index_observer(struct NotifyCallback *nc)
{
  if (nc->event_type != NT_INDEX)
    return 0;
  if (!nc->global_data)
    return -1;

  struct MuttWindow *win_pager = nc->global_data;

  struct PagerPrivateData *priv = win_pager->wdata;
  if (!priv)
    return 0;

  struct IndexSharedData *shared = nc->event_data;

  if (nc->event_subtype & NT_INDEX_MAILBOX)
  {
    win_pager->actions |= WA_RECALC;
    mutt_debug(LL_DEBUG5, "index done, request WA_RECALC\n");
    priv->loop = PAGER_LOOP_QUIT;
  }
  else if (nc->event_subtype & NT_INDEX_EMAIL)
  {
    win_pager->actions |= WA_RECALC;
    mutt_debug(LL_DEBUG5, "index done, request WA_RECALC\n");
    priv->pager_redraw = true;
    if (shared && shared->email && (priv->loop != PAGER_LOOP_QUIT))
    {
      priv->loop = PAGER_LOOP_RELOAD;
    }
    else
    {
      priv->loop = PAGER_LOOP_QUIT;
      priv->rc = 0;
    }
  }

  return 0;
}

/**
 * pager_pager_observer - Notification that the Pager has changed - Implements ::observer_t - @ingroup observer_api
 */
static int pager_pager_observer(struct NotifyCallback *nc)
{
  if (nc->event_type != NT_PAGER)
    return 0;
  if (!nc->global_data || !nc->event_data)
    return -1;

  mutt_debug(LL_DEBUG5, "pager done\n");
  return 0;
}

/**
 * pager_window_observer - Notification that a Window has changed - Implements ::observer_t - @ingroup observer_api
 */
static int pager_window_observer(struct NotifyCallback *nc)
{
  if (nc->event_type != NT_WINDOW)
    return 0;
  if (!nc->global_data || !nc->event_data)
    return -1;
  if (nc->event_subtype != NT_WINDOW_DELETE)
    return 0;

  struct MuttWindow *win_pager = nc->global_data;
  struct EventWindow *ev_w = nc->event_data;
  if (ev_w->win != win_pager)
    return 0;

  struct MuttWindow *dlg = window_find_parent(win_pager, WT_DLG_INDEX);
  if (!dlg)
    dlg = window_find_parent(win_pager, WT_DLG_PAGER);

  struct IndexSharedData *shared = dlg->wdata;

  mutt_color_observer_remove(pager_color_observer, win_pager);
  notify_observer_remove(NeoMutt->sub->notify, pager_config_observer, win_pager);
  notify_observer_remove(NeoMutt->notify, pager_global_observer, win_pager);
  notify_observer_remove(shared->notify, pager_index_observer, win_pager);
  notify_observer_remove(shared->notify, pager_pager_observer, win_pager);
  notify_observer_remove(win_pager->notify, pager_window_observer, win_pager);

  mutt_debug(LL_DEBUG5, "window delete done\n");

  return 0;
}

/**
 * pager_window_new - Create a new Pager Window (list of Emails)
 * @param shared Shared Index Data
 * @param priv   Private Pager Data
 * @retval ptr New Window
 */
struct MuttWindow *pager_window_new(struct IndexSharedData *shared,
                                    struct PagerPrivateData *priv)
{
  struct MuttWindow *win = mutt_window_new(WT_CUSTOM, MUTT_WIN_ORIENT_VERTICAL,
                                           MUTT_WIN_SIZE_MAXIMISE, MUTT_WIN_SIZE_UNLIMITED,
                                           MUTT_WIN_SIZE_UNLIMITED);
  win->wdata = priv;
  win->recalc = pager_recalc;
  win->repaint = pager_repaint;

  mutt_color_observer_add(pager_color_observer, win);
  notify_observer_add(NeoMutt->sub->notify, NT_CONFIG, pager_config_observer, win);
  notify_observer_add(NeoMutt->notify, NT_GLOBAL, pager_global_observer, win);
  notify_observer_add(shared->notify, NT_INDEX, pager_index_observer, win);
  notify_observer_add(shared->notify, NT_PAGER, pager_pager_observer, win);
  notify_observer_add(win->notify, NT_WINDOW, pager_window_observer, win);

  return win;
}
