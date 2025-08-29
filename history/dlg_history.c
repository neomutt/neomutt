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
 * None.  The dialog is not affected by any config or colours and doesn't
 * support sorting.  Once constructed, the events are handled by the Menu (part
 * of the @ref gui_simple).
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
#include "functions.h"
#include "mutt_logging.h"

const struct ExpandoRenderData HistoryRenderData[];

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
 * history_C_num - History: Index number - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
long history_C_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct HistoryEntry *entry = data;

  return entry->num + 1;
}

/**
 * history_s - History: History match - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
void history_s(const struct ExpandoNode *node, void *data,
               MuttFormatFlags flags, struct Buffer *buf)
{
  const struct HistoryEntry *entry = data;

  const char *s = entry->history;
  buf_strcpy(buf, s);
}

/**
 * history_make_entry - Format a History Item for the Menu - Implements Menu::make_entry() - @ingroup menu_make_entry
 */
static int history_make_entry(struct Menu *menu, int line, int max_cols, struct Buffer *buf)
{
  char *entry = ((char **) menu->mdata)[line];

  struct HistoryEntry h = { line, entry };

  const bool c_arrow_cursor = cs_subset_bool(menu->sub, "arrow_cursor");
  if (c_arrow_cursor)
  {
    const char *const c_arrow_string = cs_subset_string(menu->sub, "arrow_string");
    max_cols -= (mutt_strwidth(c_arrow_string) + 1);
  }

  const struct Expando *c_history_format = cs_subset_expando(NeoMutt->sub, "history_format");
  return expando_filter(c_history_format, HistoryRenderData, &h,
                        MUTT_FORMAT_ARROWCURSOR, max_cols, buf);
}

/**
 * dlg_history - Select an item from a history list - @ingroup gui_dlg
 * @param[in]  buf         Buffer in which to save string
 * @param[in]  buflen      Buffer length
 * @param[out] matches     Items to choose from
 * @param[in]  match_count Number of items
 *
 * The History Dialog lets the user select from the history of commands,
 * functions or files.
 */
void dlg_history(char *buf, size_t buflen, char **matches, int match_count)
{
  struct MuttWindow *dlg = simple_dialog_new(MENU_GENERIC, WT_DLG_HISTORY, HistoryHelp);

  struct MuttWindow *sbar = window_find_child(dlg, WT_STATUS_BAR);
  char title[256] = { 0 };
  snprintf(title, sizeof(title), _("History '%s'"), buf);
  sbar_set_title(sbar, title);

  struct Menu *menu = dlg->wdata;
  menu->make_entry = history_make_entry;
  menu->max = match_count;
  menu->mdata = matches;
  menu->mdata_free = NULL; // Menu doesn't own the data

  struct HistoryData hd = { false, false,   buf,        buflen,
                            menu,  matches, match_count };
  dlg->wdata = &hd;

  struct MuttWindow *old_focus = window_set_focus(menu->win);
  // ---------------------------------------------------------------------------
  // Event Loop
  int op = OP_NULL;
  do
  {
    menu_tagging_dispatcher(menu->win, op);
    window_redraw(NULL);

    op = km_dokey(MENU_DIALOG, GETCH_NO_FLAGS);
    mutt_debug(LL_DEBUG1, "Got op %s (%d)\n", opcodes_get_name(op), op);
    if (op < 0)
      continue;
    if (op == OP_NULL)
    {
      km_error_key(MENU_GENERIC);
      continue;
    }
    mutt_clear_error();

    int rc = history_function_dispatcher(dlg, op);
    if (rc == FR_UNKNOWN)
      rc = menu_function_dispatcher(menu->win, op);
    if (rc == FR_UNKNOWN)
      rc = global_function_dispatcher(NULL, op);
  } while (!hd.done);
  // ---------------------------------------------------------------------------

  window_set_focus(old_focus);
  simple_dialog_free(&dlg);
}

/**
 * HistoryRenderData - Callbacks for History Expandos
 *
 * @sa HistoryFormatDef, ExpandoDataGlobal, ExpandoDataHistory
 */
const struct ExpandoRenderData HistoryRenderData[] = {
  // clang-format off
  { ED_HISTORY, ED_HIS_NUMBER, NULL,          history_C_num },
  { ED_HISTORY, ED_HIS_MATCH,  history_s,     NULL },
  { -1, -1, NULL, NULL },
  // clang-format on
};
