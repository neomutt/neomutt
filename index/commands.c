/**
 * @file
 * Index commands
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
 * @page index_commands Index commands
 *
 * Index commands
 */

#include "config.h"
#include <stddef.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "subjectrx.h"

/**
 * IndexCommands - Index Commands
 */
const struct Command IndexCommands[] = {
  // clang-format off
  { "subject-regex", CMD_SUBJECT_REGEX, parse_subjectrx_list,
        N_("Apply regex-based rewriting to message subjects"),
        N_("subject-regex <regex> <replacement>"),
        "advancedusage.html#display-munging" },
  { "unsubject-regex", CMD_UNSUBJECT_REGEX, parse_unsubjectrx_list,
        N_("Remove subject-rewriting rules"),
        N_("unsubject-regex { * | <regex> }"),
        "advancedusage.html#display-munging" },

  // Deprecated
  { "subjectrx",           CMD_NONE, NULL, "subject-regex",       NULL, NULL, CF_SYNONYM },
  { "unsubjectrx",         CMD_NONE, NULL, "unsubject-regex",     NULL, NULL, CF_SYNONYM },

  { NULL, CMD_NONE, NULL, NULL, NULL, NULL, CF_NO_FLAGS },
  // clang-format on
};
