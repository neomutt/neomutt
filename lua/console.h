/**
 * @file
 * Lua Console
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

#ifndef MUTT_LUA_CONSOLE_H
#define MUTT_LUA_CONSOLE_H

/**
 * struct LuaConsoleInfo - Private data for the Lua Console
 */
struct LuaConsoleInfo
{
  struct Menu *menu;    ///< Menu
};

/**
 * enum LuaConsoleVisibilty - XXX
 */
enum LuaConsoleVisibilty
{
  LCV_SHOW,             ///< Make the Lua Console visible
  LCV_HIDE,             ///< Hide the Lua Console
  LCV_TOGGLE,           ///< Toggle the visibility of the Lua Console
};

void lua_console_set_visibility(enum LuaConsoleVisibilty vis);
void lua_console_update(void);

#endif /* MUTT_LUA_CONSOLE_H */
