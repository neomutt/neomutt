/**
 * @file
 * Handle Email commands
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
 * @page email_commands Email commands
 *
 * Email commands
 */

#include "config.h"
#include <stdio.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "group.h"
#include "ignore.h"
#include "score.h"
#include "spam.h"

/**
 * EmailCommands - Email Commands
 */
const struct Command EmailCommands[] = {
  // clang-format off
  { "group", CMD_GROUP, parse_group, CMD_NO_DATA,
        N_("Add addresses to an address group"),
        N_("group [ -group <name> ... ] { -rx <regex> ... | -addr <address> ... }"),
        "configuration.html#addrgroup" },
  { "ignore", CMD_IGNORE, parse_ignore, CMD_NO_DATA,
        N_("Hide specified headers when displaying messages"),
        N_("ignore <string> [ <string> ...]"),
        "configuration.html#ignore" },
  { "nospam", CMD_NOSPAM, parse_nospam, CMD_NO_DATA,
        N_("Remove a spam detection rule"),
        N_("nospam { * | <regex> }"),
        "configuration.html#spam" },
  { "spam", CMD_SPAM, parse_spam, CMD_NO_DATA,
        N_("Define rules to parse spam detection headers"),
        N_("spam <regex> [ <format> ]"),
        "configuration.html#spam" },
  { "ungroup", CMD_UNGROUP, parse_group, CMD_NO_DATA,
        N_("Remove addresses from an address `group`"),
        N_("ungroup [ -group <name> ... ] { * | -rx <regex> ... | -addr <address> ... }"),
        "configuration.html#addrgroup" },
  { "unignore", CMD_UNIGNORE, parse_unignore, CMD_NO_DATA,
        N_("Remove a header from the `header-order` list"),
        N_("unignore { * | <string> ... }"),
        "configuration.html#ignore" },
  { "score", CMD_SCORE, parse_score, CMD_NO_DATA,
        N_("Set a score value on emails matching a pattern"),
        N_("score <pattern> <value>"),
        "configuration.html#score-command" },
  { "unscore", CMD_UNSCORE, parse_unscore, CMD_NO_DATA,
        N_("Remove scoring rules for matching patterns"),
        N_("unscore { * | <pattern> ... }"),
        "configuration.html#score-command" },

  { NULL, CMD_NONE, NULL, CMD_NO_DATA, NULL, NULL, NULL, CF_NO_FLAGS },
  // clang-format on
};
