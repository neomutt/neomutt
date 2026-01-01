/**
 * @file
 * Parse My_hdr Commands
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

#ifndef MUTT_COMMANDS_MY_HDR_H
#define MUTT_COMMANDS_MY_HDR_H

#include "config.h"
#include "core/lib.h"

struct Buffer;

enum CommandResult parse_my_hdr          (const struct Command *cmd, struct Buffer *line, struct Buffer *err);
enum CommandResult parse_unmy_hdr        (const struct Command *cmd, struct Buffer *line, struct Buffer *err);

#endif /* MUTT_COMMANDS_MY_HDR_H */
