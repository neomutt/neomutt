/**
 * @file
 * Read/write command history from/to a file
 *
 * @authors
 * Copyright (C) 1996-2000 Michael R. Elkins <me@mutt.org>
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

#include "config.h"
#include <stdio.h>
#include "mutt/mutt.h"
#include "mutt.h"
#include "curs_lib.h"
#include "format_flags.h"
#include "keymap.h"
#include "menu.h"
#include "mutt_window.h"
#include "muttlib.h"
#include "opcodes.h"

static const struct Mapping HistoryHelp[] = {
  { N_("Exit"), OP_EXIT },
  { N_("Select"), OP_GENERIC_SELECT_ENTRY },
  { N_("Search"), OP_SEARCH },
  { N_("Help"), OP_HELP },
  { NULL, 0 },
};

/**
 * history_format_str - Format a string for the history list
 * @param[out] buf      Buffer in which to save string
 * @param[in]  buflen   Buffer length
 * @param[in]  col      Starting column
 * @param[in]  cols     Number of screen columns
 * @param[in]  op       printf-like operator, e.g. 't'
 * @param[in]  src      printf-like format string
 * @param[in]  prec     Field precision, e.g. "-3.4"
 * @param[in]  if_str   If condition is met, display this string
 * @param[in]  else_str Otherwise, display this string
 * @param[in]  data     Pointer to the mailbox Context
 * @param[in]  flags    Format flags
 * @retval src (unchanged)
 *
 * history_format_str() is a callback function for mutt_expando_format().
 *
 * | Expando | Description
 * |:--------|:--------------
 * | \%s     | History match
 */
static const char *history_format_str(char *buf, size_t buflen, size_t col, int cols,
                                      char op, const char *src, const char *prec,
                                      const char *if_str, const char *else_str,
                                      unsigned long data, enum FormatFlag flags)
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
 * history_entry - Format a menu item for the history list
 * @param[out] buf    Buffer in which to save string
 * @param[in]  buflen Buffer length
 * @param[in]  menu   Menu containing aliases
 * @param[in]  num    Index into the menu
 */
static void history_entry(char *buf, size_t buflen, struct Menu *menu, int num)
{
  char *entry = ((char **) menu->data)[num];

  mutt_expando_format(buf, buflen, 0, MuttIndexWindow->cols, "%s", history_format_str,
                      (unsigned long) entry, MUTT_FORMAT_ARROWCURSOR);
}

/**
 * history_menu - Select an item from a history list
 * @param buf         Buffer in which to save string
 * @param buflen      Buffer length
 * @param matches     Items to choose from
 * @param match_count Number of items
 */
static void history_menu(char *buf, size_t buflen, char **matches, int match_count)
{
  struct Menu *menu = NULL;
  int done = 0;
  char helpstr[LONG_STRING];
  char title[STRING];

  snprintf(title, sizeof(title), _("History '%s'"), buf);

  menu = mutt_menu_new(MENU_GENERIC);
  menu->make_entry = history_entry;
  menu->title = title;
  menu->help = mutt_compile_help(helpstr, sizeof(helpstr), MENU_GENERIC, HistoryHelp);
  mutt_menu_push_current(menu);

  menu->max = match_count;
  menu->data = matches;

  while (!done)
  {
    switch (mutt_menu_loop(menu))
    {
      case OP_GENERIC_SELECT_ENTRY:
        mutt_str_strfcpy(buf, matches[menu->current], buflen);
        /* fall through */

      case OP_EXIT:
        done = 1;
        break;
    }
  }

  mutt_menu_pop_current(menu);
  mutt_menu_destroy(&menu);
}

/**
 * mutt_hist_complete - Complete a string from a history list
 * @param buf    Buffer in which to save string
 * @param buflen Buffer length
 * @param hclass History list to use
 */
void mutt_hist_complete(char *buf, size_t buflen, enum HistoryClass hclass)
{
  char **matches = mutt_mem_calloc(History, sizeof(char *));
  int match_count = mutt_hist_search(buf, hclass, matches);
  if (match_count)
  {
    if (match_count == 1)
      mutt_str_strfcpy(buf, matches[0], buflen);
    else
      history_menu(buf, buflen, matches, match_count);
  }
  FREE(&matches);
}

/**
 * mutt_hist_listener - Listen for config changes affecting the history
 * @param cs   Config items
 * @param he   HashElem representing config item
 * @param name Name of the config item
 * @param ev   Event type, e.g. #CE_SET
 * @retval true Continue notifying
 */
bool mutt_hist_listener(const struct ConfigSet *cs, struct HashElem *he,
                        const char *name, enum ConfigEvent ev)
{
  if (mutt_str_strcmp(name, "history") != 0)
    return true;

  mutt_hist_init();
  return true;
}
