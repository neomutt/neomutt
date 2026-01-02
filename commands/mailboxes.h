/**
 * @file
 * Parse Mailboxes Commands
 *
 * @authors
 * Copyright (C) 2025 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_COMMANDS_MAILBOXES_H
#define MUTT_COMMANDS_MAILBOXES_H

#include "config.h"
#include <stdbool.h>
#include "core/lib.h"

struct Buffer;

/**
 * enum TriBool - Tri-state boolean
 */
enum TriBool
{
  TB_UNSET = -1, ///< Value hasn't been set
  TB_FALSE,      ///< Value is false
  TB_TRUE,       ///< Value is true
};


enum CommandResult parse_mailboxes  (const struct Command *cmd, struct Buffer *line, struct Buffer *err);
enum CommandResult parse_unmailboxes(const struct Command *cmd, struct Buffer *line, struct Buffer *err);

bool mailbox_add_simple(const char *mailbox, struct Buffer *err);
bool mailbox_remove_simple(const char *mailbox);

#endif /* MUTT_COMMANDS_MAILBOXES_H */
