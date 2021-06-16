/**
 * @file
 * List of Emails Window
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
 * @page index_index List of Emails Window
 *
 * List of Emails Window
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
 * index_altern_observer - Notification that an 'alternates' command has occurred - Implements ::observer_t
 */
static int index_altern_observer(struct NotifyCallback *nc)
{
  if ((nc->event_type != NT_ALTERN) || !nc->global_data)
    return -1;

  struct MuttWindow *dlg = nc->global_data;
  struct IndexSharedData *shared = dlg->wdata;

  mutt_alternates_reset(shared->mailbox);
  mutt_debug(LL_DEBUG5, "alternates done\n");
  return 0;
}

/**
 * index_attach_observer - Notification that an 'attachments' command has occurred - Implements ::observer_t
 */
static int index_attach_observer(struct NotifyCallback *nc)
{
  if ((nc->event_type != NT_ATTACH) || !nc->global_data)
    return -1;

  struct MuttWindow *dlg = nc->global_data;
  struct IndexSharedData *shared = dlg->wdata;

  mutt_attachments_reset(shared->mailbox);
  mutt_debug(LL_DEBUG5, "attachments done\n");
  return 0;
}

/**
 * index_color_observer - Notification that a Color has changed - Implements ::observer_t
 */
static int index_color_observer(struct NotifyCallback *nc)
{
  if ((nc->event_type != NT_COLOR) || !nc->global_data || !nc->event_data)
    return -1;

  struct EventColor *ev_c = nc->event_data;

  const int c = ev_c->color;

  bool simple = (c == MT_COLOR_INDEX_COLLAPSED) || (c == MT_COLOR_INDEX_DATE) ||
                (c == MT_COLOR_INDEX_LABEL) || (c == MT_COLOR_INDEX_NUMBER) ||
                (c == MT_COLOR_INDEX_SIZE) || (c == MT_COLOR_INDEX_TAGS);

  bool lists = (c == MT_COLOR_INDEX) || (c == MT_COLOR_INDEX_AUTHOR) ||
               (c == MT_COLOR_INDEX_FLAGS) || (c == MT_COLOR_INDEX_SUBJECT) ||
               (c == MT_COLOR_INDEX_TAG);

  // The changes aren't relevant to the index menu
  if (!simple && !lists)
    return 0;

  struct MuttWindow *win_index = nc->global_data;
  struct MuttWindow *dlg = dialog_find(win_index);
  struct IndexSharedData *shared = dlg->wdata;

  struct Mailbox *m = shared->mailbox;
  if (!m)
    return 0;

  // Colour deleted from a list
  if ((nc->event_subtype == NT_COLOR_RESET) && lists)
  {
    // Force re-caching of index colours
    for (int i = 0; i < m->msg_count; i++)
    {
      struct Email *e = m->emails[i];
      if (!e)
        break;
      e->pair = 0;
    }
  }

  struct MuttWindow *panel_index = window_find_child(dlg, WT_INDEX);
  struct IndexPrivateData *priv = panel_index->wdata;
  struct Menu *menu = priv->menu;
  menu->redraw = MENU_REDRAW_FULL;
  mutt_debug(LL_DEBUG5, "color done, request MENU_REDRAW_FULL\n");

  return 0;
}

/**
 * index_config_observer - Notification that a Config Variable has changed - Implements ::observer_t
 */
static int index_config_observer(struct NotifyCallback *nc)
{
  if ((nc->event_type != NT_CONFIG) || !nc->global_data || !nc->event_data)
    return -1;

  if (nc->event_subtype == NT_CONFIG_INITIAL_SET)
    return 0;

  struct EventConfig *ev_c = nc->event_data;
  struct MuttWindow *dlg = nc->global_data;
  const struct ConfigDef *cdef = ev_c->he->data;
  ConfigRedrawFlags flags = cdef->type & R_REDRAW_MASK;

  if (flags & R_RESORT_SUB)
    OptSortSubthreads = true;
  if (flags & R_RESORT)
    OptNeedResort = true;
  if (flags & R_RESORT_INIT)
    OptResortInit = true;

  if (mutt_str_equal(ev_c->name, "reply_regex"))
  {
    config_reply_regex(dlg);
    mutt_debug(LL_DEBUG5, "config done\n");
  }

  return 0;
}

/**
 * index_score_observer - Notification that a 'score' command has occurred - Implements ::observer_t
 */
static int index_score_observer(struct NotifyCallback *nc)
{
  if ((nc->event_type != NT_SCORE) || !nc->global_data)
    return -1;

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
 * index_subjrx_observer - Notification that a 'subjectrx' command has occurred - Implements ::observer_t
 */
static int index_subjrx_observer(struct NotifyCallback *nc)
{
  if ((nc->event_type != NT_SUBJRX) || !nc->global_data)
    return -1;

  struct MuttWindow *dlg = nc->global_data;
  struct IndexSharedData *shared = dlg->wdata;

  subjrx_clear_mods(shared->mailbox);
  mutt_debug(LL_DEBUG5, "subjectrx done\n");
  return 0;
}

/**
 * index_window_observer - Notification that a Window has changed - Implements ::observer_t
 */
static int index_window_observer(struct NotifyCallback *nc)
{
  if ((nc->event_type != NT_WINDOW) || !nc->global_data || !nc->event_data)
    return -1;

  if (nc->event_subtype != NT_WINDOW_DELETE)
    return 0;

  struct MuttWindow *win = nc->global_data;
  struct EventWindow *ev_w = nc->event_data;
  if (ev_w->win != win)
    return 0;

  notify_observer_remove(NeoMutt->notify, index_altern_observer, win);
  notify_observer_remove(NeoMutt->notify, index_attach_observer, win);
  notify_observer_remove(NeoMutt->notify, index_color_observer, win);
  notify_observer_remove(NeoMutt->notify, index_config_observer, win);
  notify_observer_remove(NeoMutt->notify, index_score_observer, win);
  notify_observer_remove(NeoMutt->notify, index_subjrx_observer, win);
  notify_observer_remove(win->notify, index_window_observer, win);

  mutt_debug(LL_DEBUG5, "window delete done\n");
  return 0;
}

/**
 * index_window_new - Create a new Index Window (list of Emails)
 * @retval ptr New Window
 */
struct MuttWindow *index_window_new(struct IndexSharedData *shared,
                                    struct IndexPrivateData *priv)
{
  struct MuttWindow *win = menu_new_window(MENU_MAIN, NeoMutt->sub);

  struct Menu *menu = win->wdata;
  menu->mdata = priv;
  priv->menu = menu;

  notify_observer_add(NeoMutt->notify, NT_ALTERN, index_altern_observer, win);
  notify_observer_add(NeoMutt->notify, NT_ATTACH, index_attach_observer, win);
  notify_observer_add(NeoMutt->notify, NT_COLOR, index_color_observer, win);
  notify_observer_add(NeoMutt->notify, NT_CONFIG, index_config_observer, win);
  notify_observer_add(NeoMutt->notify, NT_SCORE, index_score_observer, win);
  notify_observer_add(NeoMutt->notify, NT_SUBJRX, index_subjrx_observer, win);
  notify_observer_add(win->notify, NT_WINDOW, index_window_observer, win);

  return win;
}
