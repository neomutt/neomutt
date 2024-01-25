/**
 * @file
 * Config used by libaddress
 *
 * @authors
 * Copyright (C) 2020 Romeu Vieira <romeu.bizz@gmail.com>
 * Copyright (C) 2020-2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2023 наб <nabijaczleweli@nabijaczleweli.xyz>
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
 * @page alias_config Alias Config
 *
 * Config used by libalias
 */

#include "config.h"
#include <stddef.h>
#include <stdbool.h>
#include "mutt/lib.h"
#include "config/lib.h"

/**
 * SortAliasMethods - Sort methods for email aliases
 */
static const struct Mapping SortAliasMethods[] = {
  // clang-format off
  { "address",  SORT_ADDRESS },
  { "alias",    SORT_ALIAS },
  { "unsorted", SORT_ORDER },
  { NULL, 0 },
  // clang-format on
};

/**
 * AliasVars - Config definitions for the alias library
 */
static struct ConfigDef AliasVars[] = {
  // clang-format off
  { "alias_file", DT_PATH|D_PATH_FILE, IP "~/.neomuttrc", 0, NULL,
    "Save new aliases to this file"
  },
  { "alias_format", DT_STRING|D_NOT_EMPTY, IP "%3n %f%t %-15a %-56r | %c", 0, NULL,
    "printf-like format string for the alias menu"
  },
  { "sort_alias", DT_SORT|D_SORT_REVERSE, SORT_ALIAS, IP SortAliasMethods, NULL,
    "Sort method for the alias menu"
  },
  { "query_command", DT_STRING|D_STRING_COMMAND, 0, 0, NULL,
    "External command to query and external address book"
  },
  { "query_format", DT_STRING|D_NOT_EMPTY, IP "%3c %t %-25.25n %-25.25a | %e", 0, NULL,
    "printf-like format string for the query menu (address book)"
  },
  { NULL },
  // clang-format on
};

/**
 * config_init_alias - Register alias config variables - Implements ::module_init_config_t - @ingroup cfg_module_api
 */
bool config_init_alias(struct ConfigSet *cs)
{
  return cs_register_variables(cs, AliasVars);
}
