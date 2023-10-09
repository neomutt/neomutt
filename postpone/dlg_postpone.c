/**
 * @file
 * Postponed Email Selection Dialog
 *
 * @authors
 * Copyright (C) 1996-2002,2012-2013 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 1999-2002,2004 Thomas Roessler <roessler@does-not-exist.org>
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
 * @page postpone_dlg_postpone Postponed Email Selection Dialog
 *
 * The Postponed Email Selection Dialog lets the user set a postponed (draft)
 * email.
 *
 * This is a @ref gui_simple
 *
 * ## Windows
 *
 * | Name                             | Type             | See Also        |
 * | :------------------------------- | :--------------- | :-------------- |
 * | Postponed Email Selection Dialog | WT_DLG_POSTPONED | dlg_postponed() |
 *
 * **Parent**
 * - @ref gui_dialog
 *
 * **Children**
 * - See: @ref gui_simple
 *
 * ## Data
 * - #Menu
 * - #Menu::mdata
 * - #Mailbox
 *
 * The @ref gui_simple holds a Menu.  The Autocrypt Account Dialog stores its
 * data (#Mailbox) in Menu::mdata.
 *
 * ## Events
 *
 * Once constructed, it is controlled by the following events:
 *
 * | Event Type  | Handler                     |
 * | :---------- | :-------------------------- |
 * | #NT_CONFIG  | postponed_config_observer() |
 * | #NT_WINDOW  | postponed_window_observer() |
 *
 * The Postponed Email Selection Dialog doesn't have any specific colours, so
 * it doesn't need to support #NT_COLOR.
 *
 * The Postponed Email Selection Dialog does not implement MuttWindow::recalc()
 * or MuttWindow::repaint().
 *
 * Some other events are handled by the @ref gui_simple.
 */

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "index/lib.h"
#include "key/lib.h"
#include "menu/lib.h"
#include "pattern/lib.h"
#include "format_flags.h"
#include "functions.h"
#include "hdrline.h"
#include "mutt_logging.h"
#include "mview.h"

/// Help Bar for the Postponed email selection dialog
static const struct Mapping PostponedHelp[] = {
  // clang-format off
  { N_("Exit"),  OP_EXIT },
  { N_("Del"),   OP_DELETE },
  { N_("Undel"), OP_UNDELETE },
  { N_("Help"),  OP_HELP },
  { NULL, 0 },
  // clang-format on
};

/**
 * post_make_entry - Format an Email for the Menu - Implements Menu::make_entry() - @ingroup menu_make_entry
 *
 * @sa $index_format, index_format_str()
 */
static void post_make_entry(struct Menu *menu, char *buf, size_t buflen, int line)
{
  struct MailboxView *mv = menu->mdata;
  struct Mailbox *m = mv->mailbox;

  const char *const c_index_format = cs_subset_string(NeoMutt->sub, "index_format");
  mutt_make_string(buf, buflen, menu->win->state.cols, NONULL(c_index_format), m, -1,
                   m->emails[line], MUTT_FORMAT_INDEX | MUTT_FORMAT_ARROWCURSOR, NULL);
}

/**
 * postponed_config_observer - Notification that a Config Variable has changed - Implements ::observer_t - @ingroup observer_api
 *
 * The Address Book Window is affected by changes to `$sort_postponed`.
 */
static int postponed_config_observer(struct NotifyCallback *nc)
{
  if (nc->event_type != NT_CONFIG)
    return 0;
  if (!nc->global_data || !nc->event_data)
    return -1;

  struct EventConfig *ev_c = nc->event_data;

  if (!mutt_str_equal(ev_c->name, "index_format") && !mutt_str_equal(ev_c->name, "sort"))
    return 0;

  struct Menu *menu = nc->global_data;
  menu_queue_redraw(menu, MENU_REDRAW_FULL);
  mutt_debug(LL_DEBUG5, "config done, request WA_RECALC, MENU_REDRAW_FULL\n");

  return 0;
}

/**
 * postponed_window_observer - Notification that a Window has changed - Implements ::observer_t - @ingroup observer_api
 *
 * This function is triggered by changes to the windows.
 *
 * - Delete (this window): clean up the resources held by the Help Bar
 */
static int postponed_window_observer(struct NotifyCallback *nc)
{
  if (nc->event_type != NT_WINDOW)
    return 0;
  if (!nc->global_data || !nc->event_data)
    return -1;
  if (nc->event_subtype != NT_WINDOW_DELETE)
    return 0;

  struct MuttWindow *win_menu = nc->global_data;
  struct EventWindow *ev_w = nc->event_data;
  if (ev_w->win != win_menu)
    return 0;

  struct Menu *menu = win_menu->wdata;

  notify_observer_remove(NeoMutt->sub->notify, postponed_config_observer, menu);
  notify_observer_remove(win_menu->notify, postponed_window_observer, win_menu);

  mutt_debug(LL_DEBUG5, "window delete done\n");
  return 0;
}

/**
 * post_color - Calculate the colour for a line of the postpone index - Implements Menu::color() - @ingroup menu_color
 */
static const struct AttrColor *post_color(struct Menu *menu, int line)
{
  struct MailboxView *mv = menu->mdata;
  if (!mv || (line < 0))
    return NULL;

  struct Mailbox *m = mv->mailbox;
  if (!m)
    return NULL;

  struct Email *e = mutt_get_virt_email(m, line);
  if (!e)
    return NULL;

  if (e->attr_color)
    return e->attr_color;

  mutt_set_header_color(m, e);
  return e->attr_color;
}

/**
 * dlg_postponed - Create a Menu to select a postponed message - @ingroup gui_dlg
 * @param m Mailbox
 * @retval ptr Email
 *
 * The Select Postponed Email Dialog shows the user a list of draft emails.
 * They can select one to use in the Compose Dialog.
 *
 * This dialog is only shown if there are two or more postponed emails.
 */
struct Email *dlg_postponed(struct Mailbox *m)
{
  struct MuttWindow *dlg = simple_dialog_new(MENU_POSTPONED, WT_DLG_POSTPONED, PostponedHelp);
  // Required to number the emails
  struct MailboxView *mv = mview_new(m, NeoMutt->notify);

  struct Menu *menu = dlg->wdata;
  menu->make_entry = post_make_entry;
  menu->color = post_color;
  menu->max = m->msg_count;
  menu->mdata = mv;
  menu->mdata_free = NULL; // Menu doesn't own the data

  struct PostponeData pd = { mv, menu, NULL, false, search_state_new() };
  dlg->wdata = &pd;

  // NT_COLOR is handled by the SimpleDialog
  notify_observer_add(NeoMutt->sub->notify, NT_CONFIG, postponed_config_observer, menu);
  notify_observer_add(menu->win->notify, NT_WINDOW, postponed_window_observer, menu->win);

  struct MuttWindow *sbar = window_find_child(dlg, WT_STATUS_BAR);
  sbar_set_title(sbar, _("Postponed Messages"));

  /* The postponed mailbox is setup to have sorting disabled, but the global
   * `$sort` variable may indicate something different.   Sorting has to be
   * disabled while the postpone menu is being displayed. */
  const enum SortType c_sort = cs_subset_sort(NeoMutt->sub, "sort");
  cs_subset_str_native_set(NeoMutt->sub, "sort", SORT_ORDER, NULL);

  struct MuttWindow *old_focus = window_set_focus(menu->win);
  // ---------------------------------------------------------------------------
  // Event Loop
  int op = OP_NULL;
  do
  {
    menu_tagging_dispatcher(menu->win, op);
    window_redraw(NULL);

    op = km_dokey(MENU_POSTPONED, GETCH_NO_FLAGS);
    mutt_debug(LL_DEBUG1, "Got op %s (%d)\n", opcodes_get_name(op), op);
    if (op < 0)
      continue;
    if (op == OP_NULL)
    {
      km_error_key(MENU_POSTPONED);
      continue;
    }
    mutt_clear_error();

    int rc = postpone_function_dispatcher(dlg, op);

    if (rc == FR_UNKNOWN)
      rc = menu_function_dispatcher(menu->win, op);
    if (rc == FR_UNKNOWN)
      rc = global_function_dispatcher(NULL, op);
  } while (!pd.done);
  // ---------------------------------------------------------------------------

  mview_free(&mv);
  cs_subset_str_native_set(NeoMutt->sub, "sort", c_sort, NULL);
  search_state_free(&pd.search_state);
  window_set_focus(old_focus);
  simple_dialog_free(&dlg);

  return pd.email;
}
