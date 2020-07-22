/**
 * @file
 * History selection dialog
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
 * @page history_dlghistory History selection dialog
 *
 * History selection dialog
 */

#include "config.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "format_flags.h"
#include "keymap.h"
#include "mutt_menu.h"
#include "muttlib.h"
#include "opcodes.h"
#include "history/lib.h"

static const struct Mapping HistoryHelp[] = {
  { N_("Exit"), OP_EXIT },
  { N_("Select"), OP_GENERIC_SELECT_ENTRY },
  { N_("Search"), OP_SEARCH },
  { N_("Help"), OP_HELP },
  { NULL, 0 },
};

/**
 * history_format_str - Format a string for the history list - Implements ::format_t
 *
 * | Expando | Description
 * |:--------|:--------------
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
 * history_make_entry - Format a menu item for the history list - Implements Menu::make_entry()
 */
static void history_make_entry(char *buf, size_t buflen, struct Menu *menu, int line)
{
  char *entry = ((char **) menu->mdata)[line];

  mutt_expando_format(buf, buflen, 0, menu->win_index->state.cols, "%s",
                      history_format_str, (intptr_t) entry, MUTT_FORMAT_ARROWCURSOR);
}

/**
 * history_menu - Select an item from a history list
 * @param[in]  buf         Buffer in which to save string
 * @param[in]  buflen      Buffer length
 * @param[out] matches     Items to choose from
 * @param[in]  match_count Number of items
 */
void history_menu(char *buf, size_t buflen, char **matches, int match_count)
{
  bool done = false;
  char helpstr[1024];
  char title[256];

  snprintf(title, sizeof(title), _("History '%s'"), buf);

  struct Menu *menu = mutt_menu_new(MENU_GENERIC);
  struct MuttWindow *dlg = dialog_create_simple_index(menu, WT_DLG_HISTORY);

  menu->make_entry = history_make_entry;
  menu->title = title;
  menu->help = mutt_compile_help(helpstr, sizeof(helpstr), MENU_GENERIC, HistoryHelp);
  mutt_menu_push_current(menu);

  menu->max = match_count;
  menu->mdata = matches;

  while (!done)
  {
    switch (mutt_menu_loop(menu))
    {
      case OP_GENERIC_SELECT_ENTRY:
        mutt_str_copy(buf, matches[menu->current], buflen);
        /* fall through */

      case OP_EXIT:
        done = true;
        break;
    }
  }

  mutt_menu_pop_current(menu);
  mutt_menu_free(&menu);
  dialog_destroy_simple_index(&dlg);
}
