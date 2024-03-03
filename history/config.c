/**
 * @file
 * Config used by libhistory
 *
 * @authors
 * Copyright (C) 2020-2024 Richard Russon <rich@flatcap.org>
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
 * @page history_config Config used by the history
 *
 * Config used by libhistory
 */

#include "config.h"
#include <stddef.h>
#include <stdbool.h>
#include "config/lib.h"
#include "lib.h"
#include "expando/lib.h"
#include "menu/lib.h"

/**
 * HistoryFormatDef - Expando definitions
 *
 * Config:
 * - $history_format
 */
static const struct ExpandoDefinition HistoryFormatDef[] = {
  // clang-format off
  { "^", "arrow",        ED_GLOBAL,  ED_MEN_ARROW,        E_TYPE_STRING, NULL },
  { "*", "padding-soft", ED_GLOBAL,  ED_GLO_PADDING_SOFT, E_TYPE_STRING, node_padding_parse },
  { ">", "padding-hard", ED_GLOBAL,  ED_GLO_PADDING_HARD, E_TYPE_STRING, node_padding_parse },
  { "|", "padding-eol",  ED_GLOBAL,  ED_GLO_PADDING_EOL,  E_TYPE_STRING, node_padding_parse },
  { "C", "number",       ED_HISTORY, ED_HIS_NUMBER,       E_TYPE_NUMBER, NULL },
  { "s", "match",        ED_HISTORY, ED_HIS_MATCH,        E_TYPE_STRING, NULL },
  { NULL, NULL, 0, -1, -1, NULL }
  // clang-format on
};

/**
 * HistoryVars - Config definitions for the command history
 */
static struct ConfigDef HistoryVars[] = {
  // clang-format off
  { "history", DT_NUMBER|D_INTEGER_NOT_NEGATIVE, 10, 0, NULL,
    "Number of history entries to keep in memory per category"
  },
  { "history_file", DT_PATH|D_PATH_FILE, IP "~/.mutthistory", 0, NULL,
    "File to save history in"
  },
  { "history_format", DT_EXPANDO, IP "%^%s", IP &HistoryFormatDef, NULL,
    "printf-like format string for the history menu"
  },
  { "history_remove_dups", DT_BOOL, false, 0, NULL,
    "Remove duplicate entries from the history"
  },
  { "save_history", DT_NUMBER|D_INTEGER_NOT_NEGATIVE, 0, 0, NULL,
    "Number of history entries to save per category"
  },
  { NULL },
  // clang-format on
};

/**
 * config_init_history - Register history config variables - Implements ::module_init_config_t - @ingroup cfg_module_api
 */
bool config_init_history(struct ConfigSet *cs)
{
  return cs_register_variables(cs, HistoryVars);
}
