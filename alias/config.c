/**
 * @file
 * Config used by libaddress
 *
 * @authors
 * Copyright (C) 2020 Romeu Vieira <romeu.bizz@gmail.com>
 * Copyright (C) 2020-2024 Richard Russon <rich@flatcap.org>
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
#include <stdbool.h>
#include <stddef.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "expando/lib.h"
#include "gui.h"

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
 * AliasFormatDef - Expando definitions
 *
 * Config:
 * - $alias_format
 */
static const struct ExpandoDefinition AliasFormatDef[] = {
  // clang-format off
  { "*", "padding-soft", ED_GLOBAL, ED_GLO_PADDING_SOFT, node_padding_parse },
  { ">", "padding-hard", ED_GLOBAL, ED_GLO_PADDING_HARD, node_padding_parse },
  { "|", "padding-eol",  ED_GLOBAL, ED_GLO_PADDING_EOL,  node_padding_parse },
  { "a", "name",         ED_ALIAS,  ED_ALI_NAME,         NULL },
  { "c", "comment",      ED_ALIAS,  ED_ALI_COMMENT,      NULL },
  { "f", "flags",        ED_ALIAS,  ED_ALI_FLAGS,        NULL },
  { "n", "number",       ED_ALIAS,  ED_ALI_NUMBER,       NULL },
  { "r", "address",      ED_ALIAS,  ED_ALI_ADDRESS,      NULL },
  { "t", "tagged",       ED_ALIAS,  ED_ALI_TAGGED,       NULL },
  { "Y", "tags",         ED_ALIAS,  ED_ALI_TAGS,         NULL },
  { NULL, NULL, 0, -1, NULL }
  // clang-format on
};

/**
 * QueryFormatDef - Expando definitions
 *
 * Config:
 * - $query_format
 */
static const struct ExpandoDefinition QueryFormatDef[] = {
  // clang-format off
  { "*", "padding-soft", ED_GLOBAL, ED_GLO_PADDING_SOFT, node_padding_parse },
  { ">", "padding-hard", ED_GLOBAL, ED_GLO_PADDING_HARD, node_padding_parse },
  { "|", "padding-eol",  ED_GLOBAL, ED_GLO_PADDING_EOL,  node_padding_parse },
  { "a", "address",      ED_ALIAS,  ED_ALI_ADDRESS,      NULL },
  { "c", "number",       ED_ALIAS,  ED_ALI_NUMBER,       NULL },
  { "e", "comment",      ED_ALIAS,  ED_ALI_COMMENT,      NULL },
  { "n", "name",         ED_ALIAS,  ED_ALI_NAME,         NULL },
  { "t", "tagged",       ED_ALIAS,  ED_ALI_TAGGED,       NULL },
  { "Y", "tags",         ED_ALIAS,  ED_ALI_TAGS,         NULL },
  { NULL, NULL, 0, -1, NULL }
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
  { "alias_format", DT_EXPANDO|D_NOT_EMPTY, IP "%3n %f%t %-15a %-56r | %c", IP &AliasFormatDef, NULL,
    "printf-like format string for the alias menu"
  },
  { "sort_alias", DT_SORT|D_SORT_REVERSE, SORT_ALIAS, IP SortAliasMethods, NULL,
    "Sort method for the alias menu"
  },
  { "query_command", DT_STRING|D_STRING_COMMAND, 0, 0, NULL,
    "External command to query and external address book"
  },
  { "query_format", DT_EXPANDO|D_NOT_EMPTY, IP "%3c %t %-25.25n %-25.25a | %e", IP &QueryFormatDef, NULL,
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
