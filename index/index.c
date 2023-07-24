/**
 * @file
 * Index Window
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
 * @page index_index Index Window
 *
 * The Index Window displays a list of emails to the user.
 *
 * ## Windows
 *
 * | Name         | Type         | See Also           |
 * | :----------- | :----------- | :----------------- |
 * | Index Window | WT_DLG_INDEX | index_window_new() |
 *
 * **Parent**
 * - @ref index_ipanel
 *
 * **Children**
 *
 * None.
 *
 * ## Data
 * - #IndexPrivateData
 *
 * The Index Window stores its data (#IndexPrivateData) in MuttWindow::wdata.
 *
 * ## Events
 *
 * Once constructed, it is controlled by the following events:
 *
 * | Event Type            | Handler                 |
 * | :-------------------- | :---------------------- |
 * | #NT_ALTERN            | index_altern_observer() |
 * | #NT_ATTACH            | index_attach_observer() |
 * | #NT_COLOR             | index_color_observer()  |
 * | #NT_CONFIG            | index_config_observer() |
 * | #NT_MENU              | index_menu_observer()   |
 * | #NT_SCORE             | index_score_observer()  |
 * | #NT_SUBJRX            | index_subjrx_observer() |
 * | #NT_WINDOW            | index_window_observer() |
 * | MuttWindow::recalc()  | index_recalc()          |
 * | MuttWindow::repaint() | index_repaint()         |
 *
 * The Index Window does not implement MuttWindow::recalc() or MuttWindow::repaint().
 *
 * Some other events are handled by the window's children.
 */

#include "config.h"
#include <stddef.h>
#include <stdbool.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "attach/lib.h"
#include "color/lib.h"
#include "menu/lib.h"
#include "postpone/lib.h"
#include "alternates.h"
#include "globals.h" // IWYU pragma: keep
#include "mutt_thread.h"
#include "muttlib.h"
#include "mview.h"
#include "private_data.h"
#include "score.h"
#include "shared_data.h"
#include "subjectrx.h"

/**
 * sort_use_threads_warn - Alert the user to odd $sort settings
 */
static void sort_use_threads_warn(void)
{
  static bool warned = false;
  if (!warned)
  {
    mutt_warning(_("Changing threaded display should prefer $use_threads over $sort"));
    warned = true;
    mutt_sleep(0);
  }
}

/**
 * config_sort - React to changes to "sort"
 * @param sub the config subset that was just updated
 * @retval  0 Successfully handled
 * @retval -1 Error
 */
static int config_sort(const struct ConfigSubset *sub)
{
  const enum SortType c_sort = cs_subset_sort(sub, "sort");
  const unsigned char c_use_threads = cs_subset_enum(sub, "use_threads");

  if (((c_sort & SORT_MASK) != SORT_THREADS) || (c_use_threads == UT_UNSET))
    return 0;

  sort_use_threads_warn();

  /* Note: changing a config variable here kicks off a second round of
   * observers before the first round has completed. Be careful that
   * changes made here do not cause an infinite loop of toggling
   * adjustments - the early exit above when $sort no longer uses
   * SORT_THREADS ends the recursion.
   */
  int rc;
  if ((c_use_threads == UT_FLAT) ||
      (!(c_sort & SORT_REVERSE) == (c_use_threads == UT_REVERSE)))
  {
    /* If we were flat or the user wants to change thread
     * directions, then leave $sort alone for now and change
     * $use_threads to match the desired outcome.  The 2nd-level
     * observer for $use_threads will then adjust $sort, and our
     * 3rd-level observer for $sort will be a no-op.
     */
    rc = cs_subset_str_native_set(sub, "use_threads",
                                  (c_sort & SORT_REVERSE) ? UT_REVERSE : UT_THREADS, NULL);
  }
  else
  {
    /* We were threaded, and the user still wants the same thread
     * direction. Adjust $sort based on $sort_aux, and the 2nd-level
     * observer for $sort will be a no-op.
     */
    enum SortType c_sort_aux = cs_subset_sort(sub, "sort_aux");
    c_sort_aux ^= (c_sort & SORT_REVERSE);
    rc = cs_subset_str_native_set(sub, "sort", c_sort_aux, NULL);
  }
  return (CSR_RESULT(rc) == CSR_SUCCESS) ? 0 : -1;
}

/**
 * config_use_threads - React to changes to "use_threads"
 * @param sub the config subset that was just updated
 * @retval  0 Successfully handled
 * @retval -1 Error
 */
static int config_use_threads(const struct ConfigSubset *sub)
{
  const enum SortType c_sort = cs_subset_sort(sub, "sort");
  const unsigned char c_use_threads = cs_subset_enum(sub, "use_threads");

  if (((c_sort & SORT_MASK) != SORT_THREADS) || (c_use_threads == UT_UNSET))
    return 0;

  sort_use_threads_warn();

  /* Note: changing a config variable here kicks off a second round of
   * observers before the first round has completed. But since we
   * aren't setting $sort to threads, the 2nd-level observer will be a
   * no-op.
   */
  const enum SortType c_sort_aux = cs_subset_sort(sub, "sort_aux");
  int rc = cs_subset_str_native_set(sub, "sort", c_sort_aux, NULL);
  return (CSR_RESULT(rc) == CSR_SUCCESS) ? 0 : -1;
}

/**
 * index_adjust_sort_threads - Adjust use_threads/sort/sort_aux
 * @param sub the config subset that was just updated
 */
void index_adjust_sort_threads(const struct ConfigSubset *sub)
{
  /* For lack of a better way, we fake a "set use_threads" */
  config_use_threads(sub);
}

/**
 * config_reply_regex - React to changes to $reply_regex
 * @param mv Mailbox View
 * @retval  0 Successfully handled
 * @retval -1 Error
 */
static int config_reply_regex(struct MailboxView *mv)
{
  if (!mv || !mv->mailbox)
    return 0;

  struct Mailbox *m = mv->mailbox;

  regmatch_t pmatch[1];

  const struct Regex *c_reply_regex = cs_subset_regex(NeoMutt->sub, "reply_regex");
  for (int i = 0; i < m->msg_count; i++)
  {
    struct Email *e = m->emails[i];
    if (!e)
      break;
    struct Envelope *env = e->env;
    if (!env || !env->subject)
      continue;

    if (mutt_regex_capture(c_reply_regex, env->subject, 1, pmatch))
    {
      env->real_subj = env->subject + pmatch[0].rm_eo;
      if (env->real_subj[0] == '\0')
        env->real_subj = NULL;
      continue;
    }

    env->real_subj = env->subject;
  }

  OptResortInit = true; /* trigger a redraw of the index */
  return 0;
}

/**
 * index_altern_observer - Notification that an 'alternates' command has occurred - Implements ::observer_t - @ingroup observer_api
 */
static int index_altern_observer(struct NotifyCallback *nc)
{
  if (nc->event_type != NT_ALTERN)
    return 0;
  if (!nc->global_data)
    return -1;

  struct MuttWindow *win = nc->global_data;
  struct MuttWindow *dlg = dialog_find(win);
  struct IndexSharedData *shared = dlg->wdata;

  mutt_alternates_reset(shared->mailbox_view);
  mutt_debug(LL_DEBUG5, "alternates done\n");
  return 0;
}

/**
 * index_attach_observer - Notification that an 'attachments' command has occurred - Implements ::observer_t - @ingroup observer_api
 */
static int index_attach_observer(struct NotifyCallback *nc)
{
  if (nc->event_type != NT_ATTACH)
    return 0;
  if (!nc->global_data)
    return -1;

  struct MuttWindow *win = nc->global_data;
  struct MuttWindow *dlg = dialog_find(win);
  struct IndexSharedData *shared = dlg->wdata;

  mutt_attachments_reset(shared->mailbox_view);
  mutt_debug(LL_DEBUG5, "attachments done\n");
  return 0;
}

/**
 * index_color_observer - Notification that a Color has changed - Implements ::observer_t - @ingroup observer_api
 */
static int index_color_observer(struct NotifyCallback *nc)
{
  if (nc->event_type != NT_COLOR)
    return 0;
  if (!nc->global_data || !nc->event_data)
    return -1;

  struct EventColor *ev_c = nc->event_data;

  const int cid = ev_c->cid;

  // MT_COLOR_MAX is sent on `uncolor *`
  if (!((cid == MT_COLOR_INDEX) || (cid == MT_COLOR_INDEX_AUTHOR) ||
        (cid == MT_COLOR_INDEX_COLLAPSED) || (cid == MT_COLOR_INDEX_DATE) ||
        (cid == MT_COLOR_INDEX_FLAGS) || (cid == MT_COLOR_INDEX_LABEL) ||
        (cid == MT_COLOR_INDEX_NUMBER) || (cid == MT_COLOR_INDEX_SIZE) ||
        (cid == MT_COLOR_INDEX_SUBJECT) || (cid == MT_COLOR_INDEX_TAG) ||
        (cid == MT_COLOR_INDEX_TAGS) || (cid == MT_COLOR_MAX) ||
        (cid == MT_COLOR_NORMAL) || (cid == MT_COLOR_TREE)))
  {
    // The changes aren't relevant to the index menu
    return 0;
  }

  struct MuttWindow *win = nc->global_data;
  struct MuttWindow *dlg = dialog_find(win);
  struct IndexSharedData *shared = dlg->wdata;

  struct Mailbox *m = shared->mailbox;
  if (!m)
    return 0;

  // Force re-caching of index colours
  for (int i = 0; i < m->msg_count; i++)
  {
    struct Email *e = m->emails[i];
    if (!e)
      break;
    e->attr_color = NULL;
  }

  struct MuttWindow *panel_index = window_find_child(dlg, WT_INDEX);
  struct IndexPrivateData *priv = panel_index->wdata;
  struct Menu *menu = priv->menu;
  menu->redraw = MENU_REDRAW_FULL;
  win->actions |= WA_REPAINT;
  mutt_debug(LL_DEBUG5, "color done, request MENU_REDRAW_FULL\n");

  return 0;
}

/**
 * index_config_observer - Notification that a Config Variable has changed - Implements ::observer_t - @ingroup observer_api
 */
static int index_config_observer(struct NotifyCallback *nc)
{
  if (nc->event_type != NT_CONFIG)
    return 0;
  if (!nc->global_data || !nc->event_data)
    return -1;

  struct EventConfig *ev_c = nc->event_data;
  if (!ev_c->name || !ev_c->he)
    return 0;

  struct MuttWindow *win = nc->global_data;

  const struct ConfigDef *cdef = ev_c->he->data;
  ConfigRedrawFlags flags = cdef->type & R_REDRAW_MASK;

  if (flags & R_RESORT_SUB)
    OptSortSubthreads = true;
  if (flags & R_RESORT)
    OptNeedResort = true;
  if (flags & R_RESORT_INIT)
    OptResortInit = true;
  if (!(flags & R_INDEX))
    return 0;

  if (mutt_str_equal(ev_c->name, "reply_regex"))
  {
    struct MuttWindow *dlg = dialog_find(win);
    struct IndexSharedData *shared = dlg->wdata;
    config_reply_regex(shared->mailbox_view);
    mutt_debug(LL_DEBUG5, "config done\n");
  }
  else if (mutt_str_equal(ev_c->name, "sort"))
  {
    config_sort(ev_c->sub);
    mutt_debug(LL_DEBUG5, "config done\n");
  }
  else if (mutt_str_equal(ev_c->name, "use_threads"))
  {
    config_use_threads(ev_c->sub);
    mutt_debug(LL_DEBUG5, "config done\n");
  }

  menu_queue_redraw(win->wdata, MENU_REDRAW_INDEX);
  return 0;
}

/**
 * index_global_observer - Notification that a Global event occurred - Implements ::observer_t - @ingroup observer_api
 */
static int index_global_observer(struct NotifyCallback *nc)
{
  if (nc->event_type != NT_GLOBAL)
    return 0;
  if (!nc->global_data)
    return -1;
  if (nc->event_subtype != NT_GLOBAL_COMMAND)
    return 0;

  struct MuttWindow *win = nc->global_data;
  struct MuttWindow *dlg = dialog_find(win);
  if (!dlg)
    return 0;

  struct IndexSharedData *shared = dlg->wdata;
  mutt_check_rescore(shared->mailbox);

  return 0;
}

/**
 * index_index_observer - Notification that the Index has changed - Implements ::observer_t - @ingroup observer_api
 */
static int index_index_observer(struct NotifyCallback *nc)
{
  if (!nc->global_data)
    return -1;

  struct MuttWindow *win = nc->global_data;
  win->actions |= WA_RECALC;

  struct Menu *menu = win->wdata;
  menu_queue_redraw(menu, MENU_REDRAW_INDEX);
  mutt_debug(LL_DEBUG5, "index done, request WA_RECALC\n");

  struct IndexPrivateData *priv = menu->mdata;
  struct IndexSharedData *shared = priv->shared;
  if (shared && shared->mailbox)
    menu->max = shared->mailbox->vcount;
  else
    menu->max = 0;

  return 0;
}

/**
 * index_menu_observer - Notification that the Menu has changed - Implements ::observer_t - @ingroup observer_api
 */
static int index_menu_observer(struct NotifyCallback *nc)
{
  if (nc->event_type != NT_MENU)
    return 0;
  if (!nc->global_data)
    return -1;

  struct MuttWindow *win = nc->global_data;
  struct MuttWindow *dlg = dialog_find(win);
  struct IndexSharedData *shared = dlg->wdata;
  struct Menu *menu = win->wdata;

  const int index = menu_get_index(menu);
  struct Email *e = mutt_get_virt_email(shared->mailbox, index);
  index_shared_data_set_email(shared, e);

  return 0;
}

/**
 * index_score_observer - Notification that a 'score' command has occurred - Implements ::observer_t - @ingroup observer_api
 */
static int index_score_observer(struct NotifyCallback *nc)
{
  if (nc->event_type != NT_SCORE)
    return 0;
  if (!nc->global_data)
    return -1;

  struct MuttWindow *win = nc->global_data;
  struct MuttWindow *dlg = dialog_find(win);
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
    e->attr_color = NULL; // Force recalc of colour
  }

  mutt_debug(LL_DEBUG5, "score done\n");
  return 0;
}

/**
 * index_subjrx_observer - Notification that a 'subjectrx' command has occurred - Implements ::observer_t - @ingroup observer_api
 */
static int index_subjrx_observer(struct NotifyCallback *nc)
{
  if (nc->event_type != NT_SUBJRX)
    return 0;
  if (!nc->global_data)
    return -1;

  struct MuttWindow *win = nc->global_data;
  struct MuttWindow *dlg = dialog_find(win);
  struct IndexSharedData *shared = dlg->wdata;

  subjrx_clear_mods(shared->mailbox_view);
  mutt_debug(LL_DEBUG5, "subjectrx done\n");
  return 0;
}

/**
 * index_window_observer - Notification that a Window has changed - Implements ::observer_t - @ingroup observer_api
 */
static int index_window_observer(struct NotifyCallback *nc)
{
  if (nc->event_type != NT_WINDOW)
    return 0;
  if (!nc->global_data || !nc->event_data)
    return -1;

  struct MuttWindow *win = nc->global_data;
  struct Menu *menu = win->wdata;
  if (nc->event_subtype != NT_WINDOW_DELETE)
  {
    if (nc->event_subtype != NT_WINDOW_FOCUS)
      menu_queue_redraw(menu, MENU_REDRAW_FULL | MENU_REDRAW_INDEX);
    return 0;
  }

  struct EventWindow *ev_w = nc->event_data;
  if (ev_w->win != win)
    return 0;

  struct IndexPrivateData *priv = menu->mdata;

  mutt_color_observer_remove(index_color_observer, win);
  notify_observer_remove(NeoMutt->notify, index_altern_observer, win);
  notify_observer_remove(NeoMutt->notify, index_attach_observer, win);
  notify_observer_remove(NeoMutt->sub->notify, index_config_observer, win);
  notify_observer_remove(NeoMutt->notify, index_global_observer, win);
  notify_observer_remove(priv->shared->notify, index_index_observer, win);
  notify_observer_remove(menu->notify, index_menu_observer, win);
  notify_observer_remove(NeoMutt->notify, index_score_observer, win);
  notify_observer_remove(NeoMutt->notify, index_subjrx_observer, win);
  notify_observer_remove(win->notify, index_window_observer, win);

  mutt_debug(LL_DEBUG5, "window delete done\n");
  return 0;
}

/**
 * index_recalc - Recalculate the Index display - Implements MuttWindow::recalc() - @ingroup window_recalc
 */
static int index_recalc(struct MuttWindow *win)
{
  win->actions |= WA_REPAINT;
  mutt_debug(LL_DEBUG5, "recalc done, request WA_REPAINT\n");
  return 0;
}

/**
 * index_repaint - Repaint the Index display - Implements MuttWindow::repaint() - @ingroup window_repaint
 */
static int index_repaint(struct MuttWindow *win)
{
  struct Menu *menu = win->wdata;

  if (menu->redraw & MENU_REDRAW_FULL)
    menu_redraw_full(menu);

  // If we don't have the focus, then this is a mini-Index ($pager_index_lines)
  if (!window_is_focused(menu->win))
  {
    int indicator = menu->page_len / 3;

    /* some fudge to work out whereabouts the indicator should go */
    const int index = menu_get_index(menu);
    if ((index - indicator) < 0)
      menu->top = 0;
    else if ((menu->max - index) < (menu->page_len - indicator))
      menu->top = menu->max - menu->page_len;
    else
      menu->top = index - indicator;
  }
  menu_adjust(menu);
  menu->redraw = MENU_REDRAW_INDEX;

  struct IndexPrivateData *priv = menu->mdata;
  struct IndexSharedData *shared = priv->shared;
  struct Mailbox *m = shared->mailbox;
  const int index = menu_get_index(menu);
  if (m && m->emails && (index < m->vcount))
  {
    if (menu->redraw & MENU_REDRAW_INDEX)
    {
      menu_redraw_index(menu);
    }
    else if (menu->redraw & MENU_REDRAW_MOTION)
    {
      menu_redraw_motion(menu);
    }
    else if (menu->redraw & MENU_REDRAW_CURRENT)
    {
      menu_redraw_current(menu);
    }
  }

  menu->redraw = MENU_REDRAW_NO_FLAGS;
  mutt_debug(LL_DEBUG5, "repaint done\n");
  return 0;
}

/**
 * index_window_new - Create a new Index Window (list of Emails)
 * @param priv Private Index data
 * @retval ptr New Window
 */
struct MuttWindow *index_window_new(struct IndexPrivateData *priv)
{
  struct MuttWindow *win = menu_window_new(MENU_INDEX, NeoMutt->sub);
  win->recalc = index_recalc;
  win->repaint = index_repaint;

  struct Menu *menu = win->wdata;
  menu->mdata = priv;
  menu->mdata_free = NULL; // Menu doesn't own the data
  priv->menu = menu;

  mutt_color_observer_add(index_color_observer, win);
  notify_observer_add(NeoMutt->notify, NT_ALTERN, index_altern_observer, win);
  notify_observer_add(NeoMutt->notify, NT_ATTACH, index_attach_observer, win);
  notify_observer_add(NeoMutt->sub->notify, NT_CONFIG, index_config_observer, win);
  notify_observer_add(NeoMutt->notify, NT_GLOBAL, index_global_observer, win);
  notify_observer_add(priv->shared->notify, NT_ALL, index_index_observer, win);
  notify_observer_add(menu->notify, NT_MENU, index_menu_observer, win);
  notify_observer_add(NeoMutt->notify, NT_SCORE, index_score_observer, win);
  notify_observer_add(NeoMutt->notify, NT_SUBJRX, index_subjrx_observer, win);
  notify_observer_add(win->notify, NT_WINDOW, index_window_observer, win);

  return win;
}

/**
 * get_current_mailbox_view - Get the current Mailbox view
 * @retval ptr Current Mailbox view
 *
 * Search for the last (most recent) dialog that has an Index.
 * Then return the Mailbox from its shared data.
 */
struct MailboxView *get_current_mailbox_view(void)
{
  if (!AllDialogsWindow)
    return NULL;

  struct MuttWindow *np = NULL;
  TAILQ_FOREACH_REVERSE(np, &AllDialogsWindow->children, MuttWindowList, entries)
  {
    struct MuttWindow *win = window_find_child(np, WT_DLG_INDEX);
    if (win)
    {
      struct IndexSharedData *shared = win->wdata;
      return shared->mailbox_view;
    }

    win = window_find_child(np, WT_DLG_POSTPONE);
    if (win)
    {
      return postponed_get_mailbox_view(win);
    }
  }

  return NULL;
}

/**
 * get_current_mailbox - Get the current Mailbox
 * @retval ptr Current Mailbox
 *
 * Search for the last (most recent) dialog that has an Index.
 * Then return the Mailbox from its shared data.
 */
struct Mailbox *get_current_mailbox(void)
{
  struct MailboxView *mv = get_current_mailbox_view();
  if (mv)
    return mv->mailbox;

  return NULL;
}

/**
 * get_current_menu - Get the current Menu
 * @retval ptr Current Menu
 *
 * Search for the last (most recent) dialog that has an Index.
 * Then return the Menu from its private data.
 */
struct Menu *get_current_menu(void)
{
  if (!AllDialogsWindow)
    return NULL;

  struct MuttWindow *np = NULL;
  TAILQ_FOREACH_REVERSE(np, &AllDialogsWindow->children, MuttWindowList, entries)
  {
    struct MuttWindow *dlg = window_find_child(np, WT_DLG_INDEX);
    if (dlg)
    {
      struct MuttWindow *panel_index = window_find_child(dlg, WT_INDEX);
      struct IndexPrivateData *priv = panel_index->wdata;
      return priv->menu;
    }
  }

  return NULL;
}
