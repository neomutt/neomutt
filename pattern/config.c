/**
 * @file
 * Config used by libpattern
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
 * @page pattern_config Config used by libpattern
 *
 * Config used by libpattern
 */

#include "config.h"
#include <stddef.h>
#include <stdbool.h>
#include "private.h"
#include "config/lib.h"
#include "expando/lib.h"
#include "menu/lib.h"

/**
 * PatternFormatDef - Expando definitions
 *
 * Config:
 * - $pattern_format
 */
static const struct ExpandoDefinition PatternFormatDef[] = {
  // clang-format off
  { "^", "arrow",        ED_GLOBAL,  ED_MEN_ARROW,        E_TYPE_STRING, NULL },
  { "*", "padding-soft", ED_GLOBAL,  ED_GLO_PADDING_SOFT, E_TYPE_STRING, node_padding_parse },
  { ">", "padding-hard", ED_GLOBAL,  ED_GLO_PADDING_HARD, E_TYPE_STRING, node_padding_parse },
  { "|", "padding-eol",  ED_GLOBAL,  ED_GLO_PADDING_EOL,  E_TYPE_STRING, node_padding_parse },
  { "d", "description",  ED_PATTERN, ED_PAT_DESCRIPTION,  E_TYPE_STRING, NULL },
  { "e", "expresion",    ED_PATTERN, ED_PAT_EXPRESION,    E_TYPE_STRING, NULL },
  { "n", "number",       ED_PATTERN, ED_PAT_NUMBER,       E_TYPE_NUMBER, NULL },
  { NULL, NULL, 0, -1, -1, NULL }
  // clang-format on
};

/**
 * PatternVars - Config definitions for the pattern library
 */
static struct ConfigDef PatternVars[] = {
  // clang-format off
  { "external_search_command", DT_STRING|D_STRING_COMMAND, 0, 0, NULL,
    "External search command"
  },
  { "pattern_format", DT_EXPANDO, IP "%^%2n %-15e  %d", IP &PatternFormatDef, NULL,
    "printf-like format string for the pattern completion menu"
  },
  { "thorough_search", DT_BOOL, true, 0, NULL,
    "Decode headers and messages before searching them"
  },
  { NULL },
  // clang-format on
};

/**
 * config_init_pattern - Register pattern config variables - Implements ::module_init_config_t - @ingroup cfg_module_api
 */
bool config_init_pattern(struct ConfigSet *cs)
{
  return cs_register_variables(cs, PatternVars);
}
