/**
 * @file
 * History Selection Dialog
 *
 * @authors
 * Copyright (C) 2020 Richard Russon <rich@flatcap.org>
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
 * | Name                     | Type           | See Also             |
 * | :----------------------- | :------------- | :------------------- |
 * | History Selection Dialog | WT_DLG_HISTORY | dlg_select_history() |
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
#include <stdint.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "gui/lib.h"
#include "lib.h"
#include "index/lib.h"
#include "menu/lib.h"
#include "format_flags.h"
#include "mutt_logging.h"
#include "muttlib.h"
#include "opcodes.h"

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
 * history_format_str - Format a string for the history list - Implements ::format_t - @ingroup expando_api
 *
 * | Expando | Description
 * | :------ | :-------------
 * | \%s     | History match
 */
static const char *history_format_str(char *buf, size_t buflen, size_t col, int cols,
                                      char op, const char *src, const char *prec,
                                      const char *if_str, const char *else_str,
                                      intptr_t data, MuttFormatFlags flags)
{
  char *match = (char *) data;

  switch (op)
  {
    case 's':
      mutt_format_s(buf, buflen, prec, match);
      break;
  }

  return src;
}

/**
 * history_make_entry - Format a menu item for the history list - Implements Menu::make_entry() - @ingroup menu_make_entry
 *
 * @sa history_format_str()
 */
static void history_make_entry(struct Menu *menu, char *buf, size_t buflen, int line)
{
  char *entry = ((char **) menu->mdata)[line];

  mutt_expando_format(buf, buflen, 0, menu->win->state.cols, "%s", history_format_str,
                      (intptr_t) entry, MUTT_FORMAT_ARROWCURSOR);
}

/**
 * dlg_select_history - Select an item from a history list
 * @param[in]  buf         Buffer in which to save string
 * @param[in]  buflen      Buffer length
 * @param[out] matches     Items to choose from
 * @param[in]  match_count Number of items
 */
void dlg_select_history(char *buf, size_t buflen, char **matches, int match_count)
{
  struct MuttWindow *dlg = simple_dialog_new(MENU_GENERIC, WT_DLG_HISTORY, HistoryHelp);

  struct MuttWindow *sbar = window_find_child(dlg, WT_STATUS_BAR);
  char title[256];
  snprintf(title, sizeof(title), _("History '%s'"), buf);
  sbar_set_title(sbar, title);

  struct Menu *menu = dlg->wdata;
  menu->make_entry = history_make_entry;
  menu->max = match_count;
  menu->mdata = matches;
  menu->mdata_free = NULL; // Menu doesn't own the data

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
      case OP_GENERIC_SELECT_ENTRY:
      {
        const int index = menu_get_index(menu);
        mutt_str_copy(buf, matches[index], buflen);
        rc = FR_DONE;
        break;
      }

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

  simple_dialog_free(&dlg);
}
