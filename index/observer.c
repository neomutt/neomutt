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
#include "alternates.h"
#include "attachments.h"
#include "context.h"
#include "mutt_globals.h"
#include "options.h"
#include "subjectrx.h"

/**
 * config_pager_index_lines - React to changes to $pager_index_lines
 * @param dlg Index Dialog
 * @retval  0 Successfully handled
 * @retval -1 Error
 */
static int config_pager_index_lines(struct MuttWindow *dlg)
{
  struct MuttWindow *win_index = mutt_window_find(dlg, WT_INDEX);
  struct MuttWindow *win_pager = mutt_window_find(dlg, WT_PAGER);
  if (!win_index || !win_pager)
    return -1;

  struct MuttWindow *parent = win_pager->parent;
  if (parent->state.visible)
  {
    const short c_pager_index_lines =
        cs_subset_number(NeoMutt->sub, "pager_index_lines");
    int vcount = ctx_mailbox(Context) ? Context->mailbox->vcount : 0;
    win_index->req_rows = MIN(c_pager_index_lines, vcount);
    win_index->size = MUTT_WIN_SIZE_FIXED;

    win_index->parent->size = MUTT_WIN_SIZE_MINIMISE;
    win_index->parent->state.visible = (c_pager_index_lines != 0);
  }
  else
  {
    win_index->req_rows = MUTT_WIN_SIZE_UNLIMITED;
    win_index->size = MUTT_WIN_SIZE_MAXIMISE;

    win_index->parent->size = MUTT_WIN_SIZE_MAXIMISE;
    win_index->parent->state.visible = true;
  }

  mutt_window_reflow(dlg);
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
  struct Mailbox *m = ctx_mailbox(Context);
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

  struct MuttWindow *parent = win_index->parent;
  if (!parent)
    return -1;
  struct MuttWindow *first = TAILQ_FIRST(&parent->children);
  if (!first)
    return -1;

  const bool c_status_on_top = cs_subset_bool(NeoMutt->sub, "status_on_top");
  if ((c_status_on_top && (first == win_index)) || (!c_status_on_top && (first != win_index)))
  {
    // Swap the Index and the Index Bar Windows
    TAILQ_REMOVE(&parent->children, first, entries);
    TAILQ_INSERT_TAIL(&parent->children, first, entries);
  }

  parent = win_pager->parent;
  first = TAILQ_FIRST(&parent->children);

  if ((c_status_on_top && (first == win_pager)) || (!c_status_on_top && (first != win_pager)))
  {
    // Swap the Pager and Pager Bar Windows
    TAILQ_REMOVE(&parent->children, first, entries);
    TAILQ_INSERT_TAIL(&parent->children, first, entries);
  }

  mutt_window_reflow(dlg);
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

  struct EventConfig *ec = nc->event_data;
  struct MuttWindow *dlg = nc->global_data;

  if (mutt_str_equal(ec->name, "pager_index_lines"))
    return config_pager_index_lines(dlg);

  if (mutt_str_equal(ec->name, "reply_regex"))
    return config_reply_regex(dlg);

  if (mutt_str_equal(ec->name, "status_on_top"))
    return config_status_on_top(dlg);

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

  // struct MuttWindow *dlg = nc->global_data;

  subjrx_clear_mods(ctx_mailbox(Context));
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

  // struct MuttWindow *dlg = nc->global_data;

  mutt_attachments_reset(ctx_mailbox(Context));
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

  // struct MuttWindow *dlg = nc->global_data;

  mutt_alternates_reset(ctx_mailbox(Context));
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

  notify_observer_add(NeoMutt->notify, NT_CONFIG, index_config_observer, dlg);
  notify_observer_add(NeoMutt->notify, NT_SUBJRX, index_subjrx_observer, dlg);
  notify_observer_add(NeoMutt->notify, NT_ATTACH, index_attach_observer, dlg);
  notify_observer_add(NeoMutt->notify, NT_ALTERN, index_altern_observer, dlg);
}

/**
 * index_remove_observers - Remove Observers from the Index Dialog
 * @param dlg Index Dialog
 */
void index_remove_observers(struct MuttWindow *dlg)
{
  if (!dlg || !NeoMutt)
    return;

  notify_observer_remove(NeoMutt->notify, index_config_observer, dlg);
  notify_observer_remove(NeoMutt->notify, index_subjrx_observer, dlg);
  notify_observer_remove(NeoMutt->notify, index_attach_observer, dlg);
  notify_observer_remove(NeoMutt->notify, index_altern_observer, dlg);
}
