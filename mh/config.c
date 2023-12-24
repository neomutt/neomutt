/**
 * @file
 * Config used by libmh
 *
 * @authors
 * Copyright (C) 2023 Richard Russon <rich@flatcap.org>
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
 * @page mh_config Config used by Mh
 *
 * Config used by libmh
 */

#include "config.h"
#include <stddef.h>
#include <stdbool.h>
#include "config/lib.h"

/**
 * MhVars - Config definitions for the Mh library
 */
static struct ConfigDef MhVars[] = {
  // clang-format off
  { "mh_purge", DT_BOOL, false, 0, NULL,
    "Really delete files in MH mailboxes"
  },
  { "mh_seq_flagged", DT_STRING, IP "flagged", 0, NULL,
    "MH sequence for flagged message"
  },
  { "mh_seq_replied", DT_STRING, IP "replied", 0, NULL,
    "MH sequence to tag replied messages"
  },
  { "mh_seq_unseen", DT_STRING, IP "unseen", 0, NULL,
    "MH sequence for unseen messages"
  },
  { NULL },
  // clang-format on
};

/**
 * config_init_mh - Register mh config variables - Implements ::module_init_config_t - @ingroup cfg_module_api
 */
bool config_init_mh(struct ConfigSet *cs)
{
  return cs_register_variables(cs, MhVars);
}
