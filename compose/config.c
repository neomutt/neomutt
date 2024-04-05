/**
 * @file
 * Config used by libcompose
 *
 * @authors
 * Copyright (C) 2020 Matthew Hughes <matthewhughes934@gmail.com>
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
 * @page compose_config Config used by Compose
 *
 * Config used by libcompose
 */

#include "config.h"
#include <stddef.h>
#include <stdbool.h>
#include "config/lib.h"
#include "expando/lib.h"
#include "shared_data.h"

#ifndef ISPELL
#define ISPELL "ispell"
#endif

/**
 * ComposeFormatDef - Expando definitions
 *
 * Config:
 * - $compose_format
 */
static const struct ExpandoDefinition ComposeFormatDef[] = {
  // clang-format off
  { "*", "padding-soft", ED_GLOBAL,  ED_GLO_PADDING_SOFT, E_TYPE_STRING, node_padding_parse },
  { ">", "padding-hard", ED_GLOBAL,  ED_GLO_PADDING_HARD, E_TYPE_STRING, node_padding_parse },
  { "|", "padding-eol",  ED_GLOBAL,  ED_GLO_PADDING_EOL,  E_TYPE_STRING, node_padding_parse },
  { "a", "attach-count", ED_COMPOSE, ED_COM_ATTACH_COUNT, E_TYPE_STRING, NULL },
  { "h", "hostname",     ED_GLOBAL,  ED_GLO_HOSTNAME,     E_TYPE_STRING, NULL },
  { "l", "attach-size",  ED_COMPOSE, ED_COM_ATTACH_SIZE,  E_TYPE_STRING, NULL },
  { "v", "version",      ED_GLOBAL,  ED_GLO_VERSION,      E_TYPE_STRING, NULL },
  { NULL, NULL, 0, -1, -1, NULL }
  // clang-format on
};

/**
 * ComposeVars - Config definitions for compose
 */
static struct ConfigDef ComposeVars[] = {
  // clang-format off
  { "compose_confirm_detach_first", DT_BOOL, true, 0, NULL,
    "Prevent the accidental deletion of the composed message"
  },
  // L10N: $compose_format default format
  { "compose_format", DT_EXPANDO|D_L10N_STRING, IP N_("-- NeoMutt: Compose  [Approx. msg size: %l   Atts: %a]%>-"), IP &ComposeFormatDef, NULL,
    "printf-like format string for the Compose panel's status bar"
  },
  { "compose_show_user_headers", DT_BOOL, true, 0, NULL,
    "Controls whether or not custom headers are shown in the compose envelope"
  },
  { "copy", DT_QUAD, MUTT_YES, 0, NULL,
    "Save outgoing emails to $record"
  },
  { "edit_headers", DT_BOOL, false, 0, NULL,
    "Let the user edit the email headers whilst editing an email"
  },
  { "ispell", DT_STRING|D_STRING_COMMAND, IP ISPELL, 0, NULL,
    "External command to perform spell-checking"
  },
  { "postpone", DT_QUAD, MUTT_ASKYES, 0, NULL,
    "Save messages to the `$postponed` folder"
  },
  { NULL },
  // clang-format on
};

/**
 * config_init_compose - Register compose config variables - Implements ::module_init_config_t - @ingroup cfg_module_api
 */
bool config_init_compose(struct ConfigSet *cs)
{
  return cs_register_variables(cs, ComposeVars);
}
