/**
 * @file
 * Config used by libmlist
 *
 * @authors
 * Copyright (C) 2026 Richard Russon <rich@flatcap.org>
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
 * @page mlist_config Config used by libmlist
 *
 * Config used by libmlist
 */

#include "config.h"
#include <stddef.h>
#include "config/lib.h"
#include "expando/lib.h"
#include "expando.h"

/**
 * MlistFormatDef - Expando definitions
 *
 * Config:
 * - $url_open_command
 */
static const struct ExpandoDefinition MlistFormatDef[] = {
  // clang-format off
  { "u", "url", ED_MLIST, ED_MLS_URL, NULL },
  { NULL, NULL, 0, -1, NULL }
  // clang-format on
};

/**
 * MlistVars - Config definitions for the Mlist library
 */
struct ConfigDef MlistVars[] = {
  // clang-format off
  { "url_open_command", DT_EXPANDO, 0, IP &MlistFormatDef, NULL,
    "External command to open a URL from a List-* header"
  },
  { NULL },
  // clang-format on
};
