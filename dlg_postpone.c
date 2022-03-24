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
 * @page neo_dlg_postpone Postponed Email Selection Dialog
 *
 * The Postponed Email Selection Dialog lets the user set a postponed (draft)
 * email.
 *
 * This is a @ref gui_simple
 *
 * ## Windows
 *
 * | Name                             | Type            | See Also                     |
 * | :------------------------------- | :-------------- | :--------------------------- |
 * | Postponed Email Selection Dialog | WT_DLG_POSTPONE | dlg_select_postponed_email() |
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
#include "core/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "menu/lib.h"
#include "pattern/lib.h"
#include "format_flags.h"
#include "hdrline.h"
#include "keymap.h"
#include "mutt_logging.h"
#include "opcodes.h"
#include "protos.h"

struct Email;

/// Help Bar for the Postponed email selection dialog
static const struct Mapping PostponeHelp[] = {
  // clang-format off
  { N_("Exit"),  OP_EXIT },
  { N_("Del"),   OP_DELETE },
  { N_("Undel"), OP_UNDELETE },
  { N_("Help"),  OP_HELP },
  { NULL, 0 },
  // clang-format on
};

/**
 * post_make_entry - Format a menu item for the email list - Implements Menu::make_entry() - @ingroup menu_make_entry
 */
static void post_make_entry(struct Menu *menu, char *buf, size_t buflen, int line)
{
  struct Mailbox *m = menu->mdata;

  const char *const c_index_format = cs_subset_string(NeoMutt->sub, "index_format");
  mutt_make_string(buf, buflen, menu->win->state.cols, NONULL(c_index_format),
                   m, -1, m->emails[line], MUTT_FORMAT_ARROWCURSOR, NULL);
}

/**
 * postponed_config_observer - Notification that a Config Variable has changed - Implements ::observer_t - @ingroup observer_api
 *
 * The Address Book Window is affected by changes to `$sort_postponed`.
 */
static int postponed_config_observer(struct NotifyCallback *nc)
{
  if ((nc->event_type != NT_CONFIG) || !nc->global_data || !nc->event_data)
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
  if ((nc->event_type != NT_WINDOW) || !nc->global_data || !nc->event_data)
    return -1;

  if (nc->event_subtype != NT_WINDOW_DELETE)
    return 0;

  struct MuttWindow *win_menu = nc->global_data;
  struct EventWindow *ev_w = nc->event_data;
  if (ev_w->win != win_menu)
    return 0;

  struct Menu *menu = win_menu->wdata;

  notify_observer_remove(NeoMutt->notify, postponed_config_observer, menu);
  notify_observer_remove(win_menu->notify, postponed_window_observer, win_menu);

  mutt_debug(LL_DEBUG5, "window delete done\n");
  return 0;
}

/**
 * dlg_select_postponed_email - Create a Menu to select a postponed message
 * @param m Mailbox
 * @retval ptr Email
 */
struct Email *dlg_select_postponed_email(struct Mailbox *m)
{
  int r = -1;

  struct MuttWindow *dlg = simple_dialog_new(MENU_POSTPONE, WT_DLG_POSTPONE, PostponeHelp);

  struct Menu *menu = dlg->wdata;
  menu->make_entry = post_make_entry;
  menu->max = m->msg_count;
  menu->mdata = m;
  menu->mdata_free = NULL; // Menu doesn't own the data
  menu->custom_search = true;

  struct MuttWindow *win_menu = menu->win;

  // NT_COLOR is handled by the SimpleDialog
  notify_observer_add(NeoMutt->notify, NT_CONFIG, postponed_config_observer, menu);
  notify_observer_add(win_menu->notify, NT_WINDOW, postponed_window_observer, win_menu);

  struct MuttWindow *sbar = window_find_child(dlg, WT_STATUS_BAR);
  sbar_set_title(sbar, _("Postponed Messages"));

  /* The postponed mailbox is setup to have sorting disabled, but the global
   * `$sort` variable may indicate something different.   Sorting has to be
   * disabled while the postpone menu is being displayed. */
  const short c_sort = cs_subset_sort(NeoMutt->sub, "sort");
  cs_subset_str_native_set(NeoMutt->sub, "sort", SORT_ORDER, NULL);

  // ---------------------------------------------------------------------------
  // Event Loop
  int op = OP_NULL;
  int rc;
  do
  {
    rc = FR_UNKNOWN;
    menu_tagging_dispatcher(menu->win, op);
    window_redraw(NULL);

    op = km_dokey(menu->type);
    mutt_debug(LL_DEBUG1, "Got op %s (%d)\n", opcodes_get_name(op), op);
    if (op < 0)
      continue;
    if (op == OP_NULL)
    {
      km_error_key(menu->type);
      continue;
    }
    mutt_clear_error();

    switch (op)
    {
      case OP_DELETE:
      case OP_UNDELETE:
      {
        const int index = menu_get_index(menu);
        /* should deleted draft messages be saved in the trash folder? */
        mutt_set_flag(m, m->emails[index], MUTT_DELETE, (op == OP_DELETE));
        PostCount = m->msg_count - m->msg_deleted;
        const bool c_resolve = cs_subset_bool(NeoMutt->sub, "resolve");
        if (c_resolve && (index < (menu->max - 1)))
        {
          menu_set_index(menu, index + 1);
          if (index >= (menu->top + menu->page_len))
          {
            menu->top = index;
            menu_queue_redraw(menu, MENU_REDRAW_INDEX);
          }
        }
        else
          menu_queue_redraw(menu, MENU_REDRAW_CURRENT);
        continue;
      }

      // All search operations must exist to show the menu
      case OP_SEARCH_REVERSE:
      case OP_SEARCH_NEXT:
      case OP_SEARCH_OPPOSITE:
      case OP_SEARCH:
      {
        int index = menu_get_index(menu);
        index = mutt_search_command(m, menu, index, op);
        if (index != -1)
          menu_set_index(menu, index);
        continue;
      }

      case OP_GENERIC_SELECT_ENTRY:
        r = menu_get_index(menu);
        rc = FR_DONE;
        break;

      case OP_EXIT:
        rc = FR_DONE;
        break;
    }

    if (rc == FR_UNKNOWN)
      rc = menu_function_dispatcher(menu->win, op);
    if (rc == FR_UNKNOWN)
      rc = global_function_dispatcher(menu->win, op);
  } while (rc != FR_DONE);
  // ---------------------------------------------------------------------------

  cs_subset_str_native_set(NeoMutt->sub, "sort", c_sort, NULL);
  simple_dialog_free(&dlg);

  return (r > -1) ? m->emails[r] : NULL;
}
