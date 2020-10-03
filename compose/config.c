/**
 * @file
 * Config used by libcompose
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
 * @page compose_config Config used by libcompose
 *
 * Config used by libcompose
 */

#include <stdbool.h>
#include "private.h"
#include "config/lib.h"

#ifndef ISPELL
#define ISPELL "ispell"
#endif

// clang-format off
char *        C_ComposeFormat;           ///< Config: printf-like format string for the Compose panel's status bar
bool          C_ComposeShowUserHeaders;  ///< Config: Whether to display user-defined headers
unsigned char C_Copy;                    ///< Config: Save outgoing emails to $record
bool          C_EditHeaders;             ///< Config: Let the user edit the email headers whilst editing an email
char *        C_Ispell;                  ///< Config: External command to perform spell-checking
unsigned char C_Postpone;                ///< Config: Save messages to the `$postponed` folder
// clang-format on

static struct ConfigDef ComposeVars[] = {
  // clang-format off
  { "compose_show_user_headers", DT_BOOL, &C_ComposeShowUserHeaders, true, 0, NULL,
    "Controls whether or not custom headers are shown in the compose envelope"
  },
  { "edit_headers", DT_BOOL, &C_EditHeaders, false, 0, NULL,
    "Let the user edit the email headers whilst editing an email"
  },
  { "compose_format", DT_STRING|R_MENU, &C_ComposeFormat, IP "-- NeoMutt: Compose  [Approx. msg size: %l   Atts: %a]%>-", 0, NULL,
    "printf-like format string for the Compose panel's status bar"
  },
  { "ispell", DT_STRING|DT_COMMAND, &C_Ispell, IP ISPELL, 0, NULL,
    "External command to perform spell-checking"
  },
  { "copy", DT_QUAD, &C_Copy, MUTT_YES, 0, NULL,
    "Save outgoing emails to $record"
  },
  { "postpone", DT_QUAD, &C_Postpone, MUTT_ASKYES, 0, NULL,
    "Save messages to the `$postponed` folder"
  },
  { NULL, 0, NULL, 0, 0, NULL, NULL }
  // clang-format on
};

/**
 * config_init_compose - Register compose config variables - Implements ::module_init_config_t
 */
bool config_init_compose(struct ConfigSet *cs)
{
  return cs_register_variables(cs, ComposeVars, 0);
}
