/**
 * @file
 * Lua GUI Wrapper
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
 * @page lua_gui Lua GUI Wrapper
 *
 * Lua GUI Wrapper
 */

#include <lauxlib.h>
#include <lua.h>
#include <stddef.h>
#include "gui/lib.h"
#include "gui.h"

/**
 * lua_gui_cb_get_focus - Callback for Gui::get_focus() - Implements ::lua_callback_t
 *
 * XXX: Usage:
 * XXX: Retval:
 * XXX: Error:
 */
static int lua_gui_cb_get_focus(lua_State *l)
{
  struct MuttWindow *win = window_get_focus();
  if (!win || (win->type != WT_MENU))
  {
    lua_pushnil(l);
    lua_pushstring(l, "NOT INDEX");
    return 2;
  }

  struct MuttWindow **pptr = lua_newuserdata(l, sizeof(struct MuttWindow *));
  *pptr = win;
  luaL_getmetatable(l, "Index");
  lua_setmetatable(l, -2);

  return 1;
}

/**
 * GuiMethods - Gui Class Methods
 */
const struct luaL_Reg GuiMethods[] = {
  // clang-format off
  { "get_focus", lua_gui_cb_get_focus },
  { NULL, NULL },
  // clang-format on
};

/**
 * lua_gui_init - Initialise the Lua `gui` object
 */
void lua_gui_init(lua_State *l)
{
  lua_newtable(l);
  luaL_setfuncs(l, GuiMethods, 0);

  luaL_getmetatable(l, "Gui");
  lua_setmetatable(l, -2);

  lua_setglobal(l, "gui");
}
