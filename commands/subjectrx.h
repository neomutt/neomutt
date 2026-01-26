/**
 * @file
 * Parse Subject-regex Commands
 *
 * @authors
 * Copyright (C) 2021-2023 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_COMMANDS_SUBJECTRX_H
#define MUTT_COMMANDS_SUBJECTRX_H

#include <stdbool.h>
#include "core/lib.h"

struct Buffer;
struct Envelope;
struct MailboxView;
struct ParseContext;
struct ParseError;

/**
 * enum NotifySubjRx - Subject Regex notification types
 *
 * Observers of #NT_SUBJRX will not be passed any Event data.
 *
 * @note Notifications are sent **after** the event.
 */
enum NotifySubjRx
{
  NT_SUBJRX_ADD = 1,    ///< Subject Regex has been added
  NT_SUBJRX_DELETE,     ///< Subject Regex has been deleted
  NT_SUBJRX_DELETE_ALL, ///< All Subject Regexes have been deleted
};

void subjrx_init(void);
void subjrx_cleanup(void);

enum CommandResult parse_subjectrx_list  (const struct Command *cmd, struct Buffer *line, const struct ParseContext *pc, struct ParseError *pe);
enum CommandResult parse_unsubjectrx_list(const struct Command *cmd, struct Buffer *line, const struct ParseContext *pc, struct ParseError *pe);

bool subjrx_apply_mods(struct Envelope *env);
void subjrx_clear_mods(struct MailboxView *mv);

#endif /* MUTT_COMMANDS_SUBJECTRX_H */
