/**
 * @file
 * Index Observers
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
 * @page index_observer Index Observers
 *
 * Index Observers
 */

#include "config.h"
#include <stdbool.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "menu/lib.h"
#include "alternates.h"
#include "attachments.h"
#include "options.h"
#include "private_data.h"
#include "score.h"
#include "shared_data.h"
#include "subjectrx.h"

/**
 * config_pager_index_lines - React to changes to $pager_index_lines
 * @param dlg Index Dialog
 * @retval  0 Successfully handled
 * @retval -1 Error
 */
static int config_pager_index_lines(struct MuttWindow *dlg)
{
  struct MuttWindow *panel_index = mutt_window_find(dlg, WT_INDEX);
  struct MuttWindow *panel_pager = mutt_window_find(dlg, WT_PAGER);
  struct MuttWindow *win_index = mutt_window_find(panel_index, WT_MENU);
  struct MuttWindow *win_pager = mutt_window_find(panel_pager, WT_MENU);
  if (!win_index || !win_pager)
    return -1;

  struct MuttWindow *parent = win_pager->parent;
  if (parent->state.visible)
  {
    struct IndexSharedData *shared = dlg->wdata;
    const short c_pager_index_lines =
        cs_subset_number(NeoMutt->sub, "pager_index_lines");
    int vcount = shared->mailbox ? shared->mailbox->vcount : 0;
    win_index->req_rows = MIN(c_pager_index_lines, vcount);
    win_index->size = MUTT_WIN_SIZE_FIXED;

    panel_index->size = MUTT_WIN_SIZE_MINIMISE;
    panel_index->state.visible = (c_pager_index_lines != 0);
  }
  else
  {
    win_index->req_rows = MUTT_WIN_SIZE_UNLIMITED;
    win_index->size = MUTT_WIN_SIZE_MAXIMISE;

    panel_index->size = MUTT_WIN_SIZE_MAXIMISE;
    panel_index->state.visible = true;
  }

  mutt_window_reflow(dlg);
  mutt_debug(LL_DEBUG5, "config, request WA_REFLOW\n");
  return 0;
}

/**
 * config_reply_regex - React to changes to $reply_regex
 * @param dlg Index Dialog
 * @retval  0 Successfully handled
 * @retval -1 Error
 */
static int config_reply_regex(struct MuttWindow *dlg)
{
  struct IndexSharedData *shared = dlg->wdata;
  struct Mailbox *m = shared->mailbox;
  if (!m)
    return 0;

  regmatch_t pmatch[1];

  for (int i = 0; i < m->msg_count; i++)
  {
    struct Email *e = m->emails[i];
    if (!e)
      break;
    struct Envelope *env = e->env;
    if (!env || !env->subject)
      continue;

    const struct Regex *c_reply_regex =
        cs_subset_regex(NeoMutt->sub, "reply_regex");
    if (mutt_regex_capture(c_reply_regex, env->subject, 1, pmatch))
    {
      env->real_subj = env->subject + pmatch[0].rm_eo;
      continue;
    }

    env->real_subj = env->subject;
  }

  OptResortInit = true; /* trigger a redraw of the index */
  return 0;
}

/**
 * config_status_on_top - React to changes to $status_on_top
 * @param dlg Index Dialog
 * @retval  0 Successfully handled
 * @retval -1 Error
 */
static int config_status_on_top(struct MuttWindow *dlg)
{
  struct MuttWindow *win_index = mutt_window_find(dlg, WT_INDEX);
  struct MuttWindow *win_pager = mutt_window_find(dlg, WT_PAGER);
  if (!win_index || !win_pager)
    return -1;

  const bool c_status_on_top = cs_subset_bool(NeoMutt->sub, "status_on_top");

  struct MuttWindow *first = TAILQ_FIRST(&win_index->children);
  if ((c_status_on_top && (first->type == WT_MENU)) ||
      (!c_status_on_top && (first->type != WT_MENU)))
  {
    // Swap the Index and the Index Bar Windows
    TAILQ_REMOVE(&win_index->children, first, entries);
    TAILQ_INSERT_TAIL(&win_index->children, first, entries);
  }

  first = TAILQ_FIRST(&win_pager->children);
  if ((c_status_on_top && (first->type == WT_MENU)) ||
      (!c_status_on_top && (first->type != WT_MENU)))
  {
    // Swap the Pager and Pager Bar Windows
    TAILQ_REMOVE(&win_pager->children, first, entries);
    TAILQ_INSERT_TAIL(&win_pager->children, first, entries);
  }

  mutt_window_reflow(dlg);
  mutt_debug(LL_DEBUG5, "config, request WA_REFLOW\n");
  return 0;
}

/**
 * index_color_observer - Listen for color changes affecting the Index - Implements ::observer_t
 */
static int index_color_observer(struct NotifyCallback *nc)
{
  if (!nc->event_data || !nc->global_data)
    return -1;
  if (nc->event_type != NT_COLOR)
    return 0;

  struct EventColor *ev_c = nc->event_data;

  int c = ev_c->color;

  // MT_COLOR_MAX is sent on `uncolor *`
  bool simple = (c == MT_COLOR_INDEX_COLLAPSED) || (c == MT_COLOR_INDEX_DATE) ||
                (c == MT_COLOR_INDEX_LABEL) || (c == MT_COLOR_INDEX_NUMBER) ||
                (c == MT_COLOR_INDEX_SIZE) || (c == MT_COLOR_INDEX_TAGS) ||
                (c == MT_COLOR_MAX);
  bool lists = (c == MT_COLOR_ATTACH_HEADERS) || (c == MT_COLOR_BODY) ||
               (c == MT_COLOR_HEADER) || (c == MT_COLOR_INDEX) ||
               (c == MT_COLOR_INDEX_AUTHOR) || (c == MT_COLOR_INDEX_FLAGS) ||
               (c == MT_COLOR_INDEX_SUBJECT) || (c == MT_COLOR_INDEX_TAG) ||
               (c == MT_COLOR_MAX);

  // The changes aren't relevant to the index menu
  if (!simple && !lists)
    return 0;

  struct MuttWindow *dlg = nc->global_data;
  struct IndexSharedData *shared = dlg->wdata;

  struct Mailbox *m = shared->mailbox;
  if (!m)
    return 0;

  // Colour deleted from a list
  if ((nc->event_subtype == NT_COLOR_RESET) && lists)
  {
    // Force re-caching of index colors
    for (int i = 0; i < m->msg_count; i++)
    {
      struct Email *e = m->emails[i];
      if (!e)
        break;
      e->pair = 0;
    }
  }

  struct MuttWindow *panel_index = mutt_window_find(dlg, WT_INDEX);
  struct IndexPrivateData *priv = panel_index->wdata;
  struct Menu *menu = priv->menu;
  menu->redraw = MENU_REDRAW_FULL;

  return 0;
}

/**
 * index_config_observer - Listen for config changes affecting the Index/Pager - Implements ::observer_t
 */
static int index_config_observer(struct NotifyCallback *nc)
{
  if (!nc->event_data || !nc->global_data)
    return -1;
  if (nc->event_type != NT_CONFIG)
    return 0;

  struct EventConfig *ev_c = nc->event_data;
  struct MuttWindow *dlg = nc->global_data;

  if (mutt_str_equal(ev_c->name, "pager_index_lines"))
    return config_pager_index_lines(dlg);

  if (mutt_str_equal(ev_c->name, "reply_regex"))
    return config_reply_regex(dlg);

  if (mutt_str_equal(ev_c->name, "status_on_top"))
    return config_status_on_top(dlg);

  mutt_debug(LL_DEBUG5, "config done\n");
  return 0;
}

/**
 * index_subjrx_observer - Listen for subject regex changes affecting the Index/Pager - Implements ::observer_t
 */
static int index_subjrx_observer(struct NotifyCallback *nc)
{
  if (!nc->global_data)
    return -1;
  if (nc->event_type != NT_SUBJRX)
    return 0;

  struct MuttWindow *dlg = nc->global_data;
  struct IndexSharedData *shared = dlg->wdata;

  subjrx_clear_mods(shared->mailbox);
  mutt_debug(LL_DEBUG5, "subjectrx done\n");
  return 0;
}

/**
 * index_attach_observer - Listen for attachment command changes affecting the Index/Pager - Implements ::observer_t
 */
static int index_attach_observer(struct NotifyCallback *nc)
{
  if (!nc->global_data)
    return -1;
  if (nc->event_type != NT_ATTACH)
    return 0;

  struct MuttWindow *dlg = nc->global_data;
  struct IndexSharedData *shared = dlg->wdata;

  mutt_attachments_reset(shared->mailbox);
  mutt_debug(LL_DEBUG5, "attachments done\n");
  return 0;
}

/**
 * index_altern_observer - Listen for alternates command changes affecting the Index/Pager - Implements ::observer_t
 */
static int index_altern_observer(struct NotifyCallback *nc)
{
  if (!nc->global_data)
    return -1;
  if (nc->event_type != NT_ALTERN)
    return 0;

  struct MuttWindow *dlg = nc->global_data;
  struct IndexSharedData *shared = dlg->wdata;

  mutt_alternates_reset(shared->mailbox);
  mutt_debug(LL_DEBUG5, "alternates done\n");
  return 0;
}

/**
 * index_score_observer - Listen for Mailbox changes affecting the Index/Pager - Implements ::observer_t
 */
static int index_score_observer(struct NotifyCallback *nc)
{
  if (!nc->global_data)
    return -1;
  if (nc->event_type != NT_SCORE)
    return 0;

  struct MuttWindow *dlg = nc->global_data;
  struct IndexSharedData *shared = dlg->wdata;

  struct Mailbox *m = shared->mailbox;
  if (!m)
    return 0;

  for (int i = 0; i < m->msg_count; i++)
  {
    struct Email *e = m->emails[i];
    if (!e)
      break;

    mutt_score_message(m, e, true);
    e->pair = 0; // Force recalc of colour
  }

  mutt_debug(LL_DEBUG5, "score done\n");
  return 0;
}

/**
 * index_add_observers - Add Observers to the Index Dialog
 * @param dlg Index Dialog
 */
void index_add_observers(struct MuttWindow *dlg)
{
  if (!dlg || !NeoMutt)
    return;

  notify_observer_add(NeoMutt->notify, NT_ALTERN, index_altern_observer, dlg);
  notify_observer_add(NeoMutt->notify, NT_ATTACH, index_attach_observer, dlg);
  notify_observer_add(NeoMutt->notify, NT_COLOR, index_color_observer, dlg);
  notify_observer_add(NeoMutt->notify, NT_CONFIG, index_config_observer, dlg);
  notify_observer_add(NeoMutt->notify, NT_SCORE, index_score_observer, dlg);
  notify_observer_add(NeoMutt->notify, NT_SUBJRX, index_subjrx_observer, dlg);
}

/**
 * index_remove_observers - Remove Observers from the Index Dialog
 * @param dlg Index Dialog
 */
void index_remove_observers(struct MuttWindow *dlg)
{
  if (!dlg || !NeoMutt)
    return;

  notify_observer_remove(NeoMutt->notify, index_altern_observer, dlg);
  notify_observer_remove(NeoMutt->notify, index_attach_observer, dlg);
  notify_observer_remove(NeoMutt->notify, index_color_observer, dlg);
  notify_observer_remove(NeoMutt->notify, index_config_observer, dlg);
  notify_observer_remove(NeoMutt->notify, index_score_observer, dlg);
  notify_observer_remove(NeoMutt->notify, index_subjrx_observer, dlg);
}
