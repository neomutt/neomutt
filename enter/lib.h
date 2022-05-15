/**
 * @file
 * Enter a string
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

/**
 * @page lib_enter Mailbox Enter
 *
 * Select a Mailbox from a list
 *
 * | File                | Description                |
 * | :------------------ | :------------------------- |
 * | enter/enter.c       | @subpage enter_enter       |
 * | enter/functions.c   | @subpage enter_functions   |
 * | enter/state.c       | @subpage enter_state       |
 */

#ifndef MUTT_ENTER_LIB_H
#define MUTT_ENTER_LIB_H

#include <stddef.h>
#include <stdbool.h>
#include "mutt.h"
// IWYU pragma: begin_exports
#include "state.h"
// IWYU pragma: end_exports

struct Mailbox;

int mutt_enter_string_full(char *buf, size_t buflen, int col, CompletionFlags flags,
                           bool multiple, struct Mailbox *m, char ***files,
                           int *numfiles, struct EnterState *state);

#endif /* MUTT_ENTER_LIB_H */
