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

/**
 * @page lib_history History
 *
 * Read/write command history from/to a file
 *
 * | File                 | Description                 |
 * | :------------------- | :-------------------------- |
 * | history/config.c     | @subpage history_config     |
 * | history/dlghistory.c | @subpage history_dlghistory |
 * | history/history.c    | @subpage history_history    |
 */

#ifndef MUTT_HISTORY_LIB_H
#define MUTT_HISTORY_LIB_H

#include <stdbool.h>
#include <stdlib.h>

/**
 * enum HistoryClass - Type to differentiate different histories
 *
 * Saved lists of recently-used:
 */
enum HistoryClass
{
  HC_CMD,     ///< External commands
  HC_ALIAS,   ///< Aliases
  HC_COMMAND, ///< NeoMutt commands
  HC_FILE,    ///< Files
  HC_PATTERN, ///< Patterns
  HC_OTHER,   ///< Miscellaneous strings
  HC_MBOX,    ///< Mailboxes
  HC_MAX,
};

void  mutt_hist_add         (enum HistoryClass hclass, const char *str, bool save);
bool  mutt_hist_at_scratch  (enum HistoryClass hclass);
void  mutt_hist_free        (void);
void  mutt_hist_init        (void);
char *mutt_hist_next        (enum HistoryClass hclass);
char *mutt_hist_prev        (enum HistoryClass hclass);
void  mutt_hist_read_file   (void);
void  mutt_hist_reset_state (enum HistoryClass hclass);
void  mutt_hist_save_scratch(enum HistoryClass hclass, const char *str);
int   mutt_hist_search      (const char *search_buf, enum HistoryClass hclass, char **matches);

void dlg_select_history(char *buf, size_t buflen, char **matches, int match_count);

#endif /* MUTT_HISTORY_LIB_H */
