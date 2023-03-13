/**
 * @file
 * Parse the 'set' command
 *
 * @authors
 * Copyright (C) 2023 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_PARSE_SET_H
#define MUTT_PARSE_SET_H

#include <stdint.h>
#include "core/lib.h"

struct Buffer;

/**
 * enum MuttSetCommand - Flags for parse_set()
 */
enum MuttSetCommand
{
  MUTT_SET_SET,   ///< default is to set all vars
  MUTT_SET_INV,   ///< default is to invert all vars
  MUTT_SET_UNSET, ///< default is to unset all vars
  MUTT_SET_RESET, ///< default is to reset all vars to default
};

enum CommandResult parse_set(struct Buffer *buf, struct Buffer *s, intptr_t data, struct Buffer *err);

#endif /* MUTT_PARSE_SET_H */
