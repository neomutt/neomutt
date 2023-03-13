/**
 * @file
 * Parse lines from a runtime configuration (rc) file
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

#ifndef PARSE_RC_H
#define PARSE_RC_H

#include "core/lib.h"

struct Buffer;

enum CommandResult parse_rc_buffer(struct Buffer *line, struct Buffer *token, struct Buffer *err);
enum CommandResult parse_rc_line  (const char *line, struct Buffer *err);

#endif /* PARSE_RC_H */
