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
#include <stddef.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "expando/lib.h"
#include "expando.h"
#include "sort.h"

/**
 * AliasSortMethods - Sort methods for email aliases
 */
static const struct Mapping AliasSortMethods[] = {
  // clang-format off
  { "alias",    ALIAS_SORT_ALIAS },
  { "email",    ALIAS_SORT_EMAIL },
  { "name",     ALIAS_SORT_NAME },
  { "unsorted", ALIAS_SORT_UNSORTED },
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
  { "a", "alias",        ED_ALIAS,  ED_ALI_ALIAS,        NULL },
  { "A", "address",      ED_ALIAS,  ED_ALI_ADDRESS,      NULL },
  { "C", "comment",      ED_ALIAS,  ED_ALI_COMMENT,      NULL },
  { "E", "email",        ED_ALIAS,  ED_ALI_EMAIL,        NULL },
  { "f", "flags",        ED_ALIAS,  ED_ALI_FLAGS,        NULL },
  { "i", "number",       ED_ALIAS,  ED_ALI_NUMBER,       NULL },
  { "N", "name",         ED_ALIAS,  ED_ALI_NAME,         NULL },
  { "t", "tagged",       ED_ALIAS,  ED_ALI_TAGGED,       NULL },
  { "Y", "tags",         ED_ALIAS,  ED_ALI_TAGS,         NULL },
  // Deprecated
  { "c", NULL,           ED_ALIAS,  ED_ALI_COMMENT,      NULL },
  { "n", NULL,           ED_ALIAS,  ED_ALI_NUMBER,       NULL },
  { "r", NULL,           ED_ALIAS,  ED_ALI_ADDRESS,      NULL },
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
  { "A", "address",      ED_ALIAS,  ED_ALI_ADDRESS,      NULL },
  { "C", "comment",      ED_ALIAS,  ED_ALI_COMMENT,      NULL },
  { "E", "email",        ED_ALIAS,  ED_ALI_EMAIL,        NULL },
  { "i", "number",       ED_ALIAS,  ED_ALI_NUMBER,       NULL },
  { "N", "name",         ED_ALIAS,  ED_ALI_NAME,         NULL },
  { "t", "tagged",       ED_ALIAS,  ED_ALI_TAGGED,       NULL },
  { "Y", "tags",         ED_ALIAS,  ED_ALI_TAGS,         NULL },
  // Deprecated
  { "a", NULL,           ED_ALIAS,  ED_ALI_EMAIL,        NULL },
  { "c", NULL,           ED_ALIAS,  ED_ALI_NUMBER,       NULL },
  { "e", NULL,           ED_ALIAS,  ED_ALI_COMMENT,      NULL },
  { "n", NULL,           ED_ALIAS,  ED_ALI_NAME,         NULL },
  { NULL, NULL, 0, -1, NULL }
  // clang-format on
};

/**
 * AliasVars - Config definitions for the alias library
 */
struct ConfigDef AliasVars[] = {
  // clang-format off
  { "alias_file", DT_PATH|D_PATH_FILE, IP "~/.neomuttrc", 0, NULL,
    "Save new aliases to this file"
  },
  { "alias_format", DT_EXPANDO|D_NOT_EMPTY, IP "%3i %f%t %-15a %-56A | %C%> %Y", IP &AliasFormatDef, NULL,
    "printf-like format string for the alias menu"
  },
  { "alias_sort", DT_SORT|D_SORT_REVERSE, ALIAS_SORT_ALIAS, IP AliasSortMethods, NULL,
    "Sort method for the alias menu"
  },
  { "query_command", DT_STRING|D_STRING_COMMAND, 0, 0, NULL,
    "External command to query and external address book"
  },
  { "query_format", DT_EXPANDO|D_NOT_EMPTY, IP "%3i %t %-25N %-25E | %C%> %Y", IP &QueryFormatDef, NULL,
    "printf-like format string for the query menu (address book)"
  },

  { "sort_alias", DT_SYNONYM, IP "alias_sort", IP "2024-11-19" },

  { NULL },
  // clang-format on
};
