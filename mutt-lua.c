/* Copyright (C) 2016 Richard Russon <rich@flatcap.org>
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 *
 *     You should have received a copy of the GNU General Public License
 *     along with this program; if not, write to the Free Software
 *     Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "mutt.h"
#include "globals.h"

/* The function we'll call from the lua script */
int
average (lua_State *l)
{
  /* get number of arguments */
  int n = lua_gettop (l);
  int sum = 0;
  int i;

  /* loop through each argument */
  for (i = 1; i <= n; i++)
  {
#if LUA_VERSION_NUM >= 503
    if (!lua_isinteger (l, i))
    {
      lua_pushstring (l, "Incorrect argument to 'average'");
      lua_error (l);
    }
#endif

    /* total the arguments */
    sum += lua_tointeger (l, i);
  }

  /* push the average */
  lua_pushinteger (l, sum / n);

  /* return the number of results */
  return 1;
}

int
get_lua_integer (lua_State *l, const char *name)
{
  if (!name)
    return -1;

  /* lookup the name and put it on the stack */
  lua_getglobal (l, name);
  if (!lua_isnumber (l, -1))
    return -1;

  int ret = lua_tonumber (l, -1);
  lua_pop (l, 1);

  return ret;
}

int
lua_test (void)
{
  lua_State *l;

  if (!LuaScript)
    return 0;

  /* initialise Lua */
  l = luaL_newstate();
  if (!l)
    return 0;

  /* load Lua base libraries */
  luaL_openlibs (l);

  /* register our function */
  lua_register (l, "average", average);

  /* set some default values */
  lua_pushinteger (l, 15); lua_setglobal (l, "apple");
  lua_pushinteger (l, 27); lua_setglobal (l, "banana");
  lua_pushinteger (l, 39); lua_setglobal (l, "cherry");

  if (luaL_dofile (l, LuaScript) == 0)
  {
    /* retrieve the values */
    mutt_message ("apple  = %d", get_lua_integer (l, "apple"));  mutt_sleep(1);
    mutt_message ("banana = %d", get_lua_integer (l, "banana")); mutt_sleep(1);
    mutt_message ("cherry = %d", get_lua_integer (l, "cherry")); mutt_sleep(1);

    /* one value on the stack */
    if (lua_gettop (l) == 1)
    {
      mutt_message ("lua returned: %lld", lua_tointeger (l, 1));
    }
    else
    {
      mutt_message ("lua returned");
    }
  }
  else
  {
    mutt_error ("error running lua script");
  }

  /* cleanup Lua */
  lua_close (l);

  return 1;
}

