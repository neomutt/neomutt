/**
 * @file
 * Parse colour commands
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

#ifndef MUTT_COLOR_PARSE_COLOR_H
#define MUTT_COLOR_PARSE_COLOR_H

#include "core/lib.h"
#include "mutt/lib.h"

struct AttrColor;

extern const struct Mapping ColorNames[];

enum CommandResult parse_attr_spec (struct Buffer *buf, struct Buffer *s, struct AttrColor *ac, struct Buffer *err);
enum CommandResult parse_color_pair(struct Buffer *buf, struct Buffer *s, struct AttrColor *ac, struct Buffer *err);

#endif /* MUTT_COLOR_PARSE_COLOR_H */
