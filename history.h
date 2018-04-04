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

#ifndef _MUTT_HISTORY_H
#define _MUTT_HISTORY_H

#include <stdbool.h>

extern short History;
extern char *HistoryFile;
extern bool  HistoryRemoveDups;
extern short SaveHistory;

/**
 * enum HistoryClass - Type to differentiate different histories
 */
enum HistoryClass
{
  HC_CMD,
  HC_ALIAS,
  HC_COMMAND,
  HC_FILE,
  HC_PATTERN,
  HC_OTHER,
  HC_MBOX,
  /* insert new items here to keep history file working */
  HC_LAST
};

void  mutt_hist_add(enum HistoryClass hclass, const char *str, bool save);
bool  mutt_hist_at_scratch(enum HistoryClass hclass);
void  mutt_hist_free(void);
void  mutt_hist_init(void);
char *mutt_hist_next(enum HistoryClass hclass);
char *mutt_hist_prev(enum HistoryClass hclass);
void  mutt_hist_read_file(void);
void  mutt_hist_reset_state(enum HistoryClass hclass);
void  mutt_hist_save_scratch(enum HistoryClass hclass, const char *str);
void  mutt_history_complete(char *buf, size_t buflen, enum HistoryClass hclass);

#endif /* _MUTT_HISTORY_H */
