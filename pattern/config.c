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

/**
 * PatternVars - Config definitions for the pattern library
 */
static struct ConfigDef PatternVars[] = {
  // clang-format off
  { "external_search_command", DT_STRING|DT_COMMAND, 0, 0, NULL,
    "External search command"
  },
  { "pattern_format", DT_STRING, IP "%2n %-15e  %d", 0, NULL,
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
  return cs_register_variables(cs, PatternVars, 0);
}
