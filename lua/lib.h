/**
 * @file
 * Integrated Lua scripting
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

/**
 * @page lib_lua Integrated Lua scripting
 *
 * Integrated Lua scripting
 *
 * | File                | Description                |
 * | :------------------ | :------------------------- |
 * | lua/commands.c      | @subpage lua_commands      |
 * | lua/lua.c           | @subpage lua_lua           |
 */

#ifndef MUTT_LUA_LIB_H
#define MUTT_LUA_LIB_H

#include "config.h"

#ifdef USE_LUA

void lua_init   (void);
void lua_cleanup(void);

#else

static inline void lua_init   (void) {}
static inline void lua_cleanup(void) {}

#endif

#endif /* MUTT_LUA_LIB_H */
