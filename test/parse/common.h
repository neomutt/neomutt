/**
 * @file
 * Common test code for parsing
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

#ifndef TEST_PARSE_COMMON_H
#define TEST_PARSE_COMMON_H

#include "config.h"
#include <stddef.h>
#include "core/lib.h"

extern struct Command mutt_commands[];

size_t commands_array(struct Command **first);

#endif /* TEST_PARSE_COMMON_H */
