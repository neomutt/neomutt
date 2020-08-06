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
 * @page mutt_history Read/write command history from/to a file
 *
 * Read/write command history from/to a file
 */

#include "config.h"
#include <stdio.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "mutt_history.h"
#include "history/lib.h"

/**
 * mutt_hist_complete - Complete a string from a history list
 * @param buf    Buffer in which to save string
 * @param buflen Buffer length
 * @param hclass History list to use
 */
void mutt_hist_complete(char *buf, size_t buflen, enum HistoryClass hclass)
{
  char **matches = mutt_mem_calloc(C_History, sizeof(char *));
  int match_count = mutt_hist_search(buf, hclass, matches);
  if (match_count)
  {
    if (match_count == 1)
      mutt_str_copy(buf, matches[0], buflen);
    else
      dlg_select_history(buf, buflen, matches, match_count);
  }
  FREE(&matches);
}

/**
 * mutt_hist_observer - Listen for config changes affecting the history - Implements ::observer_t
 */
int mutt_hist_observer(struct NotifyCallback *nc)
{
  if (!nc->event_data)
    return -1;
  if (nc->event_type != NT_CONFIG)
    return 0;

  struct EventConfig *ec = nc->event_data;

  if (!mutt_str_equal(ec->name, "history"))
    return 0;

  mutt_hist_init();
  return 0;
}
