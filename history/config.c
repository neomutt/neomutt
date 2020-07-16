/**
 * @file
 * Config used by libhistory
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
 * @page history_config Config used by libhistory
 *
 * Config used by libhistory
 */

#include "config.h"
#include <stddef.h>
#include <config/lib.h>
#include <stdbool.h>
#include "mutt/lib.h"

// clang-format off
short C_History;           ///< Config: Number of history entries to keep in memory per category
char *C_HistoryFile;       ///< Config: File to save history in
bool  C_HistoryRemoveDups; ///< Config: Remove duplicate entries from the history
short C_SaveHistory;       ///< Config: Number of history entries to save per category
// clang-format on

// clang-format off
struct ConfigDef HistoryVars[] = {
  { "history",             DT_NUMBER|DT_NOT_NEGATIVE, &C_History,           10 },
  { "history_file",        DT_PATH|DT_PATH_FILE,      &C_HistoryFile,       IP "~/.mutthistory" },
  { "history_remove_dups", DT_BOOL,                   &C_HistoryRemoveDups, false },
  { "save_history",        DT_NUMBER|DT_NOT_NEGATIVE, &C_SaveHistory,       0 },
  { NULL, 0, NULL, 0, 0, NULL },
};
// clang-format on

/**
 * config_init_history - Register history config variables
 */
bool config_init_history(struct ConfigSet *cs)
{
  return cs_register_variables(cs, HistoryVars, 0);
}
