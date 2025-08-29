/**
 * @file
 * Read/write command history from/to a file
 *
 * @authors
 * Copyright (C) 2017-2024 Richard Russon <rich@flatcap.org>
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
 * | File                  | Description                  |
 * | :-------------------- | :--------------------------- |
 * | history/config.c      | @subpage history_config      |
 * | history/dlg_history.c | @subpage history_dlg_history |
 * | history/functions.c   | @subpage history_functions   |
 * | history/history.c     | @subpage history_history     |
 */

#ifndef MUTT_HISTORY_LIB_H
#define MUTT_HISTORY_LIB_H

#include <stdbool.h>
#include <stdlib.h>

struct NotifyCallback;

/**
 * enum HistoryClass - Type to differentiate different histories
 *
 * Saved lists of recently-used:
 */
enum HistoryClass
{
  HC_EXT_COMMAND, ///< External commands
  HC_ALIAS,       ///< Aliases
  HC_NEO_COMMAND, ///< NeoMutt commands
  HC_FILE,        ///< Files
  HC_PATTERN,     ///< Patterns
  HC_OTHER,       ///< Miscellaneous strings
  HC_MAILBOX,     ///< Mailboxes
  HC_MAX,
};

/**
 * struct HistoryEntry - A line in the History menu
 */
struct HistoryEntry
{
  int num;             ///< Index number
  const char *history; ///< Description of history
};

/**
 * ExpandoDataHistory - Expando UIDs for History
 *
 * @sa ED_HISTORY, ExpandoDomain
 */
enum ExpandoDataHistory
{
  ED_HIS_MATCH = 1,            ///< HistoryEntry.history
  ED_HIS_NUMBER,               ///< HistoryEntry.num
};

void  mutt_hist_add         (enum HistoryClass hclass, const char *str, bool save);
bool  mutt_hist_at_scratch  (enum HistoryClass hclass);
void  mutt_hist_cleanup     (void);
void  mutt_hist_init        (void);
char *mutt_hist_next        (enum HistoryClass hclass);
char *mutt_hist_prev        (enum HistoryClass hclass);
void  mutt_hist_read_file   (void);
void  mutt_hist_reset_state (enum HistoryClass hclass);
void  mutt_hist_save_scratch(enum HistoryClass hclass, const char *str);
int   mutt_hist_search      (const char *search_buf, enum HistoryClass hclass, char **matches);
void  mutt_hist_complete    (char *buf, size_t buflen, enum HistoryClass hclass);
int   main_hist_observer    (struct NotifyCallback *nc);

void  dlg_history(char *buf, size_t buflen, char **matches, int match_count);

#endif /* MUTT_HISTORY_LIB_H */
