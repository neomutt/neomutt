/**
 * @file
 * Send Commands
 *
 * @authors
 * Copyright (C) 2025-2026 Richard Russon <rich@flatcap.org>
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
 * @page send_commands Send Commands
 *
 * Send Commands
 */

#include "config.h"
#include <stdio.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "my_header.h"

/**
 * SendCommands - Send Commands
 */
const struct Command SendCommands[] = {
  // clang-format off
  { "my-header", CMD_MY_HEADER, parse_my_header,
        N_("Add a custom header to outgoing messages"),
        N_("my-header <string>"),
        "configuration.html#my-header" },
  { "unmy-header", CMD_UNMY_HEADER, parse_unmy_header,
        N_("Remove a header previously added with `my-header`"),
        N_("unmy-header { * | <field> ... }"),
        "configuration.html#my-header" },

  // Deprecated
  { "my_hdr",              CMD_NONE, NULL, "my-header",           NULL, NULL, CF_SYNONYM },
  { "unmy_hdr",            CMD_NONE, NULL, "unmy-header",         NULL, NULL, CF_SYNONYM },

  { NULL, CMD_NONE, NULL, NULL, NULL, NULL, CF_NO_FLAGS },
  // clang-format on
};
