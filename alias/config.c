/**
 * @file
 * Config used by libaddress
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
 * @page alias_config Config used by libaddress
 *
 * Config used by libaddress
 */

#include "config.h"
#include <config/lib.h>
#include <stdbool.h>

/**
 * SortAliasMethods - Sort methods for email aliases
 */
const struct Mapping SortAliasMethods[] = {
  // clang-format off
  { "address",  SORT_ADDRESS },
  { "alias",    SORT_ALIAS },
  { "unsorted", SORT_ORDER },
  { NULL,       0 },
  // clang-format on
};

char *C_AliasFile = NULL; ///< Config: Save new aliases to this file
char *C_AliasFormat = NULL; ///< Config: printf-like format string for the alias menu
char *C_QueryCommand = NULL; ///< Config: External command to query and external address book
char *C_QueryFormat = NULL; ///< Config: printf-like format string for the query menu (address book)
short C_SortAlias = 0; ///< Config: Sort method for the alias menu

struct ConfigDef AliasVars[] = {
  // clang-format off
  { "alias_file", DT_PATH|DT_PATH_FILE, &C_AliasFile, IP "~/.neomuttrc", 0, NULL,
    "Save new aliases to this file"
  },
  { "alias_format", DT_STRING|DT_NOT_EMPTY, &C_AliasFormat, IP "%3n %f%t %-15a %-56r | %c", 0, NULL,
    "printf-like format string for the alias menu"
  },
  { "sort_alias", DT_SORT|DT_SORT_REVERSE, &C_SortAlias, SORT_ALIAS, IP SortAliasMethods, NULL,
    "Sort method for the alias menu"
  },
  { "query_command", DT_STRING|DT_COMMAND, &C_QueryCommand, 0, 0, NULL,
    "External command to query and external address book"
  },
  { "query_format", DT_STRING|DT_NOT_EMPTY, &C_QueryFormat, IP "%3c %t %-25.25n %-25.25a | %e", 0, NULL,
    "printf-like format string for the query menu (address book)"
  },
  { NULL, 0, NULL, 0, 0, NULL, NULL },
  // clang-format on
};

/**
 * config_init_alias - Register alias config variables - Implements ::module_init_config_t
 */
bool config_init_alias(struct ConfigSet *cs)
{
  return cs_register_variables(cs, AliasVars, 0);
}
