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

#include <stdbool.h>

#include "mutt.h"
#include "globals.h"

lua_State *Lua = NULL;

static int _lua_mutt_message(lua_State *l) {
  mutt_debug(2, "—→ _lua_mutt_message()\n");
  const char *msg = lua_tostring(l, -1);
  if (msg != NULL)
    mutt_message(msg);
  return 0;
}

static int _lua_mutt_error(lua_State *l) {
  mutt_debug(2, "—→ _lua_mutt_error()\n");
  const char *msg = lua_tostring(l, -1);
  if (msg != NULL)
    mutt_error(msg);
  return 0;
}

static int _lua_mutt_error_handler(lua_State *l)
{
  mutt_error("Oops...");
  return 0;
}


static const luaL_Reg luaMuttDecl[] =
{
    { "message", _lua_mutt_message },
    { "error",   _lua_mutt_error   },
    { NULL, NULL }
};


static int luaopen_mutt_decl(lua_State *l)
{
  mutt_debug(2, "—→ luaopen_mutt()\n");
  luaL_newlib(l, luaMuttDecl);
  return 1;
}
#define luaopen_mutt(L) do { luaL_requiref(L, "mutt", luaopen_mutt_decl, 1); } while (0);

static bool _lua_init(void)
{
  if (Lua == NULL) {
    mutt_debug(2, "—→ lua_init()\n");
    Lua = luaL_newstate();

    if (Lua == NULL) {
      mutt_error("Error: Couldn't load the lua interpreter.");
      return false;
    }

    lua_atpanic (Lua, _lua_mutt_error_handler);

    /* load various Lua libraries */
    luaL_openlibs(Lua);
    luaopen_mutt(Lua);
  }

  return true;
}

// static void _lua_close(void)
// {
//   printf("—→ lua_close()");
//   lua_close(Lua);
// }

int mutt_lua_parse (BUFFER *tmp, BUFFER *s, unsigned long data, BUFFER *err)
{
  int error;
  _lua_init();
  mutt_debug(2, "—→ mutt_lua_parse(%s)\n", tmp->data);
  // printf("—→ mutt_lua_parse(%s)\n", s->dptr);

  if ((error = luaL_dostring(Lua, s->dptr))) {
    mutt_debug(2, "—→ mutt_lua_parse(%s) → failure\n", s->dptr);
    // printf("—→ mutt_lua_parse(%s) → failure\n", s->dptr);
    mutt_error("Lua error: %s\n", lua_tostring(Lua, -1));
    lua_pop(Lua, 1);
/* pop error message from the stack */
    return -1;
  }
  mutt_debug(2, "—→ mutt_lua_parse(%s) → success\n", s->dptr);
  // printf("—→ mutt_lua_parse(%s) → success\n", tmp->data);
  return 1;
}

int mutt_lua_source_file (BUFFER *tmp, BUFFER *s, unsigned long data, BUFFER *err)
{
  mutt_debug(2, "—→ mutt_lua_source()\n");

  _lua_init();

  char path[_POSIX_PATH_MAX];

  if (mutt_extract_token (tmp, s, 0) != 0)
  {
    snprintf (err->data, err->dsize, _("source: error at %s"), s->dptr);
    return (-1);
  }
  if (MoreArgs (s))
  {
    strfcpy (err->data, _("source: too many arguments"), err->dsize);
    return (-1);
  }
  strfcpy (path, tmp->data, sizeof (path));
  mutt_expand_path (path, sizeof (path));

  if (luaL_dofile(Lua, path)) {
      mutt_error("Couldn't source lua source: %s", lua_tostring(Lua, -1));
      lua_pop(Lua, 1);
      return -1;
  }
  return 1;
}

/* The function we'll call from the lua script */
int average (lua_State *l)
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

int get_lua_integer (lua_State *l, const char *name)
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

// int lua_test (void)
// {
//   if (!LuaScript)
//     return 0;

//   /* initialise Lua */
//   l = luaL_newstate();
//   if (!l)
//     return 0;

//   /* load Lua base libraries */
//   luaL_openlibs (l);

//   /* register our function */
//   lua_register (l, "average", average);

//   /* set some default values */
//   lua_pushinteger (l, 15); lua_setglobal (l, "apple");
//   lua_pushinteger (l, 27); lua_setglobal (l, "banana");
//   lua_pushinteger (l, 39); lua_setglobal (l, "cherry");

//   if (luaL_dofile (l, LuaScript) == 0)
//   {
//     /* retrieve the values */
//     mutt_message ("apple  = %d", get_lua_integer (l, "apple"));  mutt_sleep(1);
//     mutt_message ("banana = %d", get_lua_integer (l, "banana")); mutt_sleep(1);
//     mutt_message ("cherry = %d", get_lua_integer (l, "cherry")); mutt_sleep(1);

//     /* one value on the stack */
//     if (lua_gettop (l) == 1)
//     {
//       mutt_message ("lua returned: %lld", lua_tointeger (l, 1));
//     }
//     else
//     {
//       mutt_message ("lua returned");
//     }
//   }
//   else
//   {
//     mutt_error ("error running lua script");
//   }

//   /* cleanup Lua */
//   lua_close (l);

//   return 1;
// }

