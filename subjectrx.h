/**
 * @file
 * Subject Regex handling
 *
 * @authors
 * Copyright (C) 2021 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_SUBJECTRX_H
#define MUTT_SUBJECTRX_H

#include <stdbool.h>
#include <stdint.h>
#include "mutt_commands.h"

struct Buffer;
struct Mailbox;

/**
 * enum NotifySubjRx - Subject Regex notification types
 */
enum NotifySubjRx
{
  NT_SUBJRX_ADD = 1,    ///< Subject Regex has been added
  NT_SUBJRX_DELETE,     ///< Subject Regex has been deleted
};

/**
 * struct EventSubjectRx - A Subject Regex event
 */
struct EventSubjectRx
{
  const char *rule;   // Subject regex added or removed
};

void subjrx_init(void);
void subjrx_free(void);

enum CommandResult parse_subjectrx_list  (struct Buffer *buf, struct Buffer *s, intptr_t data, struct Buffer *err);
enum CommandResult parse_unsubjectrx_list(struct Buffer *buf, struct Buffer *s, intptr_t data, struct Buffer *err);

bool subjrx_apply_mods(struct Envelope *env);
void subjrx_clear_mods(struct Mailbox *m);

#endif /* MUTT_SUBJECTRX_H */
