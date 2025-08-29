/**
 * @file
 * Alternate address handling
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

#ifndef MUTT_ALTERNATES_H
#define MUTT_ALTERNATES_H

#include <stdbool.h>
#include <stdint.h>
#include "core/lib.h"

struct Buffer;
struct MailboxView;

/**
 * enum NotifyAlternates - Alternates command notification types
 *
 * Observers of #NT_ALTERN will not be passed any Event data.
 *
 * @note Notifications are sent **after** the event.
 */
enum NotifyAlternates
{
  NT_ALTERN_ADD = 1,    ///< Alternate address has been added
  NT_ALTERN_DELETE,     ///< Alternate address has been deleted
  NT_ALTERN_DELETE_ALL, ///< All Alternate addresses have been deleted
};

void alternates_init(void);
void alternates_cleanup(void);

enum CommandResult parse_alternates  (struct Buffer *buf, struct Buffer *s, intptr_t data, struct Buffer *err);
enum CommandResult parse_unalternates(struct Buffer *buf, struct Buffer *s, intptr_t data, struct Buffer *err);

bool mutt_alternates_match(const char *addr);
void mutt_alternates_reset(struct MailboxView *mv);

#endif /* MUTT_ALTERNATES_H */
