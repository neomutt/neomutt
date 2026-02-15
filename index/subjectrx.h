/**
 * @file
 * Parse Subject-regex Commands
 *
 * @authors
 * Copyright (C) 2021-2026 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_INDEX_SUBJECTRX_H
#define MUTT_INDEX_SUBJECTRX_H

#include <stdbool.h>
#include "core/lib.h"

struct Buffer;
struct Envelope;
struct IndexModuleData;
struct MailboxView;
struct ParseContext;
struct ParseError;

/**
 * enum NotifySubjectRx - Subject Regex notification types
 *
 * Observers of #NT_SUBJECTRX will not be passed any Event data.
 *
 * @note Notifications are sent **after** the event.
 */
enum NotifySubjectRx
{
  NT_SUBJECTRX_ADD = 1,    ///< Subject Regex has been added
  NT_SUBJECTRX_DELETE,     ///< Subject Regex has been deleted
  NT_SUBJECTRX_DELETE_ALL, ///< All Subject Regexes have been deleted
};

void subjectrx_init   (struct NeoMutt *n, struct IndexModuleData *md);
void subjectrx_cleanup(struct IndexModuleData *md);

enum CommandResult parse_subjectrx_list  (const struct Command *cmd, struct Buffer *line, const struct ParseContext *pc, struct ParseError *pe);
enum CommandResult parse_unsubjectrx_list(const struct Command *cmd, struct Buffer *line, const struct ParseContext *pc, struct ParseError *pe);

bool subjectrx_apply_mods(struct Envelope *env);
void subjectrx_clear_mods(struct MailboxView *mv);

#endif /* MUTT_INDEX_SUBJECTRX_H */
