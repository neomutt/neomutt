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
#include "mutt/lib.h"
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

/**
 * struct ParseMailbox - Parsed data for a single mailbox
 */
struct ParseMailbox
{
  char *path;          ///< Mailbox path
  char *label;         ///< Descriptive label (strdup'd, may be NULL)
  enum TriBool poll;   ///< Enable mailbox polling?
  enum TriBool notify; ///< Enable mailbox notification?
};
ARRAY_HEAD(ParseMailboxArray, struct ParseMailbox);

void               parse_mailbox_free (struct ParseMailbox *pm);
void               parse_mailbox_array_free(struct ParseMailboxArray *pma);
bool               parse_mailboxes_args(const struct Command *cmd, struct Buffer *line, struct Buffer *err, struct ParseMailboxArray *args);
enum CommandResult parse_mailboxes_exec(const struct Command *cmd, struct ParseMailboxArray *args, struct Buffer *err);
enum CommandResult parse_mailboxes(const struct Command *cmd, struct Buffer *line, struct ParseContext *pctx, struct ConfigParseError *perr);
enum CommandResult parse_unmailboxes(const struct Command *cmd, struct Buffer *line, struct ParseContext *pctx, struct ConfigParseError *perr);

bool mailbox_add_simple(const char *mailbox, struct Buffer *err);
bool mailbox_remove_simple(const char *mailbox);

#endif /* MUTT_COMMANDS_MAILBOXES_H */
