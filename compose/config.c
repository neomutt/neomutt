/**
 * @file
 * Config used by libcompose
 *
 * @authors
 * Copyright (C) 2020 Matthew Hughes <matthewhughes934@gmail.com>
 * Copyright (C) 2020-2024 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2024 Dennis Schön <mail@dennis-schoen.de>
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
#include <stdbool.h>
#include <stddef.h>
#include "mutt/lib.h"
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
  { "*", "padding-soft", ED_GLOBAL,  ED_GLO_PADDING_SOFT, node_padding_parse },
  { ">", "padding-hard", ED_GLOBAL,  ED_GLO_PADDING_HARD, node_padding_parse },
  { "|", "padding-eol",  ED_GLOBAL,  ED_GLO_PADDING_EOL,  node_padding_parse },
  { "a", "attach-count", ED_COMPOSE, ED_COM_ATTACH_COUNT, NULL },
  { "h", "hostname",     ED_GLOBAL,  ED_GLO_HOSTNAME,     NULL },
  { "l", "attach-size",  ED_COMPOSE, ED_COM_ATTACH_SIZE,  NULL },
  { "v", "version",      ED_GLOBAL,  ED_GLO_VERSION,      NULL },
  { NULL, NULL, 0, -1, NULL }
  // clang-format on
};

/**
 * ComposeVars - Config definitions for compose
 */
struct ConfigDef ComposeVars[] = {
  // clang-format off
  { "compose_confirm_detach_first", DT_BOOL, true, 0, NULL,
    "Prevent the accidental deletion of the composed message"
  },
  // L10N: $compose_format default format
  { "compose_format", DT_EXPANDO|D_L10N_STRING, IP N_("-- NeoMutt: Compose  [Approx. msg size: %l   Atts: %a]%>-"), IP &ComposeFormatDef, NULL,
    "printf-like format string for the Compose panel's status bar"
  },
  { "compose_show_preview", DT_BOOL, false, 0, NULL,
    "Display a preview of the message body in the Compose window"
  },
  { "compose_show_user_headers", DT_BOOL, true, 0, NULL,
    "Controls whether or not custom headers are shown in the compose envelope"
  },
  { "compose_preview_min_rows", DT_NUMBER|D_INTEGER_NOT_NEGATIVE, 5, 0, NULL,
    "Hide the preview if it has fewer than this number of rows"
  },
  { "compose_preview_above_attachments", DT_BOOL, false, 0, NULL,
    "Show the message preview above the attachments list. By default it is shown below it."
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

  { "edit_hdrs", DT_SYNONYM, IP "edit_headers", IP "2021-03-21" },
  { NULL },
  // clang-format on
};
