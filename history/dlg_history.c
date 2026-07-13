/**
 * @file
 * History Selection Dialog
 *
 * @authors
 * Copyright (C) 2020-2024 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2023-2024 Tóth János <gomba007@gmail.com>
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
 * @page history_dlg_history History Selection Dialog
 *
 * The History Selection Dialog lets the user choose a string from the history,
 * e.g. a past command.
 *
 * This is a @ref gui_simple
 *
 * ## Windows
 *
 * | Name                     | Type           | See Also      |
 * | :----------------------- | :------------- | :------------ |
 * | History Selection Dialog | WT_DLG_HISTORY | dlg_history() |
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
 * - `char **matches`
 *
 * The @ref gui_simple holds a Menu.  The History Selection Dialog stores its
 * data (`char **matches`) in Menu::mdata.
 *
 * ## Events
 *
 * Once constructed, it is controlled by the following events:
 *
 * | Event Type | Handler                   |
 * | :--------- | :------------------------ |
 * | #NT_CONFIG | history_config_observer() |
 *
 * The dialog is not affected by colours and doesn't support sorting.  Some
 * other events are handled by the Menu (part of the @ref gui_simple).
 */

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "lib.h"
#include "expando/lib.h"
#include "key/lib.h"
#include "menu/lib.h"
#include "expando.h"
#include "functions.h"
#include "mutt_logging.h"

/// Help Bar for the History Selection dialog
static const struct Mapping HistoryHelp[] = {
  // clang-format off
  { N_("Exit"),   OP_EXIT },
  { N_("Select"), OP_GENERIC_SELECT_ENTRY },
  { N_("Search"), OP_SEARCH },
  { N_("Help"),   OP_HELP },
  { NULL, 0 },
  // clang-format on
};

/**
 * history_make_entry - Format a History Item for the Menu - Implements Menu::make_entry() - @ingroup menu_make_entry
 */
static int history_make_entry(struct Menu *menu, int line, int max_cols, struct Buffer *buf)
{
  struct HistoryData *hd = menu->mdata;

  const char **pentry = ARRAY_GET(hd->matches, line);
  if (!pentry)
    return 0;

  struct HistoryEntry h = { line, *pentry };

  const bool c_arrow_cursor = cs_subset_bool(menu->sub, "arrow_cursor");
  if (c_arrow_cursor)
  {
    const char *const c_arrow_string = cs_subset_string(menu->sub, "arrow_string");
    if (max_cols > 0)
      max_cols -= (mutt_strwidth(c_arrow_string) + 1);
  }

  const struct Expando *c_history_format = cs_subset_expando(NeoMutt->sub, "history_format");
  return expando_filter(c_history_format, HistoryRenderCallbacks, &h,
                        MUTT_FORMAT_ARROWCURSOR, max_cols, NeoMutt->env, buf);
}

/**
 * history_config_observer - Notification that a Config Variable has changed - Implements ::observer_t - @ingroup observer_api
 *
 * The History Dialog is affected by changes to `$history_format`.
 */
static int history_config_observer(struct NotifyCallback *nc)
{
  if (nc->event_type != NT_CONFIG)
    return 0;
  if (!nc->global_data || !nc->event_data)
    return -1;

  struct EventConfig *ev_c = nc->event_data;
  if (!mutt_str_equal(ev_c->name, "history_format"))
    return 0;

  struct Menu *menu = nc->global_data;
  menu_queue_redraw(menu, MENU_REDRAW_FULL);
  mutt_debug(LL_DEBUG5, "config done, request WA_RECALC, MENU_REDRAW_FULL\n");

  return 0;
}

/**
 * dlg_history - Select an item from a history list - @ingroup gui_dlg
 * @param[in]  buf     Buffer in which to save string
 * @param[out] matches Items to choose from
 *
 * The History Dialog lets the user select from the history of commands,
 * functions or files.
 */
void dlg_history(struct Buffer *buf, struct StringArray *matches)
{
  const struct MenuDefinition *md_generic = generic_get_menu_definition();
  struct SimpleDialogWindows sdw = simple_dialog_new(md_generic, WT_DLG_HISTORY, HistoryHelp);
  struct Menu *menu = sdw.menu;

  struct HistoryData hd = { false, false, buf, menu, matches };

  char title[256] = { 0 };
  snprintf(title, sizeof(title), _("History '%s'"), buf_string(buf));
  sbar_set_title(sdw.sbar, title);

  menu->make_entry = history_make_entry;
  menu->max = ARRAY_SIZE(matches);
  menu->mdata = &hd;
  menu->mdata_free = NULL; // Menu doesn't own the data

  // NT_COLOR is handled by the SimpleDialog
  notify_observer_add(NeoMutt->sub->notify, NT_CONFIG, history_config_observer, menu);

  struct MuttWindow *old_focus = window_set_focus(menu->win);
  // ---------------------------------------------------------------------------
  // Event Loop
  int op = OP_NULL;
  struct KeyEvent event = { 0, OP_NULL };
  do
  {
    menu_tagging_dispatcher(menu->win, &event);
    window_redraw(NULL);

    event = km_dokey(md_generic, GETCH_NONE);
    op = event.op;
    mutt_debug(LL_DEBUG1, "Got op %s (%d)\n", opcodes_get_name(op), op);
    if (op < 0)
      continue;
    if (op == OP_NULL)
    {
      km_error_key(md_generic);
      continue;
    }
    mutt_clear_error();

    int rc = history_function_dispatcher(sdw.dlg, &event);
    if (rc == FR_UNKNOWN)
      rc = menu_function_dispatcher(menu->win, &event);
    if (rc == FR_UNKNOWN)
      rc = global_function_dispatcher(menu->win, &event);
  } while (!hd.done);
  // ---------------------------------------------------------------------------

  window_set_focus(old_focus);
  notify_observer_remove(NeoMutt->sub->notify, history_config_observer, menu);
  simple_dialog_free(&sdw.dlg);
}
