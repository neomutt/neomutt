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

#define HC_FIRST HC_CMD

void mutt_init_history(void);
void mutt_read_histfile(void);
void mutt_history_add(enum HistoryClass hclass, const char *s, int save);
char *mutt_history_next(enum HistoryClass hclass);
char *mutt_history_prev(enum HistoryClass hclass);
void mutt_reset_history_state(enum HistoryClass hclass);
int mutt_history_at_scratch(enum HistoryClass hclass);
void mutt_history_save_scratch(enum HistoryClass hclass, const char *s);

#endif /* _MUTT_HISTORY_H */
