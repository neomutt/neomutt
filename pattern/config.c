/**
 * @file
 * Config used by libpattern
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
 * @page pattern_config Config used by libpattern
 *
 * Config used by libpattern
 */

#include "config.h"
#include <stddef.h>
#include <config/lib.h>
#include <stdbool.h>

// clang-format off
char *C_ExternalSearchCommand; ///< Config: External search command
char *C_PatternFormat;         ///< Config: printf-like format string for the pattern completion menu
bool  C_ThoroughSearch;        ///< Config: Decode headers and messages before searching them
// clang-format on

// clang-format off
struct ConfigDef PatternVars[] = {
  { "external_search_command", DT_STRING|DT_COMMAND, &C_ExternalSearchCommand, 0 },
  { "pattern_format", DT_STRING, &C_PatternFormat, IP "%2n %-15e  %d" },
  { "thorough_search", DT_BOOL, &C_ThoroughSearch, true },
  { NULL, 0, NULL, 0, 0, NULL },
};
// clang-format on

/**
 * config_init_pattern - Register pattern config variables
 */
bool config_init_pattern(struct ConfigSet *cs)
{
  return cs_register_variables(cs, PatternVars, 0);
}
