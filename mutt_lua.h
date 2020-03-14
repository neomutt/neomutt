/**
 * @file
 * Integrated Lua scripting
 *
 * @authors
 * Copyright (C) 2016 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_MUTT_LUA_H
#define MUTT_MUTT_LUA_H

#include "mutt_commands.h"

struct Buffer;

enum CommandResult mutt_lua_parse(struct Buffer *tmp, struct Buffer *s, intptr_t data, struct Buffer *err);
enum CommandResult mutt_lua_source_file(struct Buffer *tmp, struct Buffer *s, intptr_t data, struct Buffer *err);

#endif /* MUTT_MUTT_LUA_H */
