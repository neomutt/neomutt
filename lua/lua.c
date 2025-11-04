/**
 * @file
 * Integrated Lua scripting
 *
 * @authors
 * Copyright (C) 2016-2017 Bernard Pratz <z+mutt+pub@m0g.net>
 * Copyright (C) 2017-2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2018 Victor Fernandes <criw@pm.me>
 * Copyright (C) 2019 Ian Zimmerman <itz@no-use.mooo.com>
 * Copyright (C) 2019-2020 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2023 Rayford Shireman
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
 * @page lua_lua Integrated Lua scripting
 *
 * Integrated Lua scripting
 */

#ifndef LUA_COMPAT_ALL
#define LUA_COMPAT_ALL
#endif
#ifndef LUA_COMPAT_5_1
#define LUA_COMPAT_5_1
#endif

#include "config.h"
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "lib.h"
#include "parse/lib.h"
#include "muttlib.h"
#include "version.h"

/// Global Lua State
lua_State *LuaState = NULL;

/**
 * lua_handle_panic - Handle a panic in the Lua interpreter
 * @param l Lua State
 * @retval -1 Always
 */
static int lua_handle_panic(lua_State *l)
{
  mutt_debug(LL_DEBUG1, "lua runtime panic: %s\n", lua_tostring(l, -1));
  mutt_error("Lua runtime panic: %s", lua_tostring(l, -1));
  lua_pop(l, 1);
  return -1;
}

/**
 * lua_handle_error - Handle an error in the Lua interpreter
 * @param l Lua State
 * @retval -1 Always
 */
static int lua_handle_error(lua_State *l)
{
  mutt_debug(LL_DEBUG1, "lua runtime error: %s\n", lua_tostring(l, -1));
  mutt_error("Lua runtime error: %s", lua_tostring(l, -1));
  lua_pop(l, 1);
  return -1;
}

/**
 * lua_cb_global_call - Call a NeoMutt command by name
 * @param l Lua State
 * @retval >=0 Success
 * @retval -1 Error
 */
static int lua_cb_global_call(lua_State *l)
{
  mutt_debug(LL_DEBUG2, "enter\n");
  struct Buffer *err = buf_pool_get();
  struct Buffer *token = buf_pool_get();
  struct Buffer *buf = buf_pool_get();
  const struct Command *cmd = NULL;
  int rc = 0;

  if (lua_gettop(l) == 0)
  {
    luaL_error(l, "Error command argument required");
    return -1;
  }

  cmd = commands_get(&NeoMutt->commands, lua_tostring(l, 1));
  if (!cmd)
  {
    luaL_error(l, "Error command %s not found", lua_tostring(l, 1));
    return -1;
  }

  for (int i = 2; i <= lua_gettop(l); i++)
  {
    buf_addstr(buf, lua_tostring(l, i));
    buf_addch(buf, ' ');
  }
  buf_seek(buf, 0);

  if (cmd->parse(token, buf, cmd->data, err))
  {
    luaL_error(l, "NeoMutt error: %s", buf_string(err));
    rc = -1;
  }
  else
  {
    if (!lua_pushstring(l, buf_string(err)))
      lua_handle_error(l);
    else
      rc++;
  }

  buf_pool_release(&buf);
  buf_pool_release(&token);
  buf_pool_release(&err);
  return rc;
}

/**
 * lua_cb_global_set - Set a NeoMutt variable
 * @param l Lua State
 * @retval  0 Success
 * @retval -1 Error
 */
static int lua_cb_global_set(lua_State *l)
{
  const char *param = lua_tostring(l, -2);
  mutt_debug(LL_DEBUG2, "%s\n", param);

  struct Buffer *err = buf_pool_get();
  struct HashElem *he = cs_subset_lookup(NeoMutt->sub, param);
  if (!he)
  {
    // In case it is a my_var, we have to create it
    if (mutt_str_startswith(param, "my_"))
    {
      struct ConfigDef my_cdef = { 0 };
      my_cdef.name = param;
      my_cdef.type = DT_MYVAR;
      he = cs_create_variable(NeoMutt->sub->cs, &my_cdef, err);
      if (!he)
        return -1;
    }
    else
    {
      luaL_error(l, "NeoMutt parameter not found %s", param);
      return -1;
    }
  }

  struct ConfigDef *cdef = he->data;

  int rc = 0;

  switch (CONFIG_TYPE(cdef->type))
  {
    case DT_ADDRESS:
    case DT_ENUM:
    case DT_EXPANDO:
    case DT_MBTABLE:
    case DT_MYVAR:
    case DT_PATH:
    case DT_REGEX:
    case DT_SLIST:
    case DT_SORT:
    case DT_STRING:
    {
      const char *value = lua_tostring(l, -1);
      size_t val_size = lua_rawlen(l, -1);
      struct Buffer *value_buf = buf_pool_get();
      buf_strcpy_n(value_buf, value, val_size);
      if (CONFIG_TYPE(he->type) == DT_PATH)
        buf_expand_path(value_buf);

      int rv = cs_subset_he_string_set(NeoMutt->sub, he, buf_string(value_buf), err);
      buf_pool_release(&value_buf);
      if (CSR_RESULT(rv) != CSR_SUCCESS)
        rc = -1;
      break;
    }
    case DT_LONG:
    case DT_NUMBER:
    case DT_QUAD:
    {
      const intptr_t value = lua_tointeger(l, -1);
      int rv = cs_subset_he_native_set(NeoMutt->sub, he, value, err);
      if (CSR_RESULT(rv) != CSR_SUCCESS)
        rc = -1;
      break;
    }
    case DT_BOOL:
    {
      const intptr_t value = lua_toboolean(l, -1);
      int rv = cs_subset_he_native_set(NeoMutt->sub, he, value, err);
      if (CSR_RESULT(rv) != CSR_SUCCESS)
        rc = -1;
      break;
    }
    default:
      luaL_error(l, "Unsupported NeoMutt parameter type %d for %s",
                 CONFIG_TYPE(cdef->type), param);
      rc = -1;
      break;
  }

  buf_pool_release(&err);
  return rc;
}

/**
 * lua_cb_global_get - Get a NeoMutt variable
 * @param l Lua State
 * @retval  1 Success
 * @retval -1 Error
 */
static int lua_cb_global_get(lua_State *l)
{
  const char *param = lua_tostring(l, -1);
  mutt_debug(LL_DEBUG2, "%s\n", param);

  struct HashElem *he = cs_subset_lookup(NeoMutt->sub, param);
  if (!he)
  {
    mutt_debug(LL_DEBUG2, "error\n");
    luaL_error(l, "NeoMutt parameter not found %s", param);
    return -1;
  }

  struct ConfigDef *cdef = he->data;

  switch (CONFIG_TYPE(cdef->type))
  {
    case DT_ADDRESS:
    case DT_ENUM:
    case DT_EXPANDO:
    case DT_MBTABLE:
    case DT_MYVAR:
    case DT_PATH:
    case DT_REGEX:
    case DT_SLIST:
    case DT_SORT:
    case DT_STRING:
    {
      struct Buffer *value = buf_pool_get();
      int rc = cs_subset_he_string_get(NeoMutt->sub, he, value);
      if (CSR_RESULT(rc) != CSR_SUCCESS)
      {
        buf_pool_release(&value);
        return -1;
      }

      struct Buffer *escaped = buf_pool_get();
      escape_string(escaped, buf_string(value));
      lua_pushstring(l, buf_string(escaped));
      buf_pool_release(&value);
      buf_pool_release(&escaped);
      return 1;
    }
    case DT_QUAD:
      lua_pushinteger(l, (unsigned char) cdef->var);
      return 1;
    case DT_LONG:
      lua_pushinteger(l, (signed long) cdef->var);
      return 1;
    case DT_NUMBER:
      lua_pushinteger(l, (signed short) cdef->var);
      return 1;
    case DT_BOOL:
      lua_pushboolean(l, (bool) cdef->var);
      return 1;
    default:
      luaL_error(l, "NeoMutt parameter type %d unknown for %s", cdef->type, param);
      return -1;
  }
}

/**
 * lua_cb_global_enter - Execute NeoMutt config from Lua
 * @param l Lua State
 * @retval >=0 Success
 * @retval -1  Error
 */
static int lua_cb_global_enter(lua_State *l)
{
  mutt_debug(LL_DEBUG2, "enter\n");
  struct Buffer *err = buf_pool_get();
  char *buf = mutt_str_dup(lua_tostring(l, -1));
  int rc = 0;

  if (parse_rc_line(buf, err))
  {
    luaL_error(l, "NeoMutt error: %s", buf_string(err));
    rc = -1;
  }
  else
  {
    if (!lua_pushstring(l, buf_string(err)))
      lua_handle_error(l);
    else
      rc++;
  }

  FREE(&buf);
  buf_pool_release(&err);

  return rc;
}

/**
 * lua_cb_global_message - Display a message in NeoMutt
 * @param l Lua State
 * @retval 0 Always
 */
static int lua_cb_global_message(lua_State *l)
{
  mutt_debug(LL_DEBUG2, "enter\n");
  const char *msg = lua_tostring(l, -1);
  if (msg)
    mutt_message("%s", msg);
  return 0;
}

/**
 * lua_cb_global_error - Display an error in NeoMutt
 * @param l Lua State
 * @retval 0 Always
 */
static int lua_cb_global_error(lua_State *l)
{
  mutt_debug(LL_DEBUG2, "enter\n");
  const char *msg = lua_tostring(l, -1);
  if (msg)
    mutt_error("%s", msg);
  return 0;
}

/**
 * lua_expose_command - Expose a NeoMutt command to the Lua interpreter
 * @param l   Lua state
 * @param cmd NeoMutt Command
 */
static void lua_expose_command(lua_State *l, const struct Command *cmd)
{
  char buf[1024] = { 0 };
  snprintf(buf, sizeof(buf), "mutt.command.%s = function (...); mutt.call('%s', ...); end",
           cmd->name, cmd->name);
  (void) luaL_dostring(l, buf);
}

/**
 * LuaMuttCommands - List of Lua commands to register
 *
 * In NeoMutt, run:
 *
 * `:lua mutt.message('hello')`
 *
 * and it will call lua_cb_global_message()
 */
static const luaL_Reg LuaMuttCommands[] = {
  // clang-format off
  { "set",     lua_cb_global_set },
  { "get",     lua_cb_global_get },
  { "call",    lua_cb_global_call },
  { "enter",   lua_cb_global_enter },
  { "print",   lua_cb_global_message },
  { "message", lua_cb_global_message },
  { "error",   lua_cb_global_error },
  { NULL, NULL },
  // clang-format on
};

/**
 * lua_expose_commands - Declare some NeoMutt types to the Lua interpreter
 * @param l Lua State
 * @retval 1 Always
 */
static int lua_expose_commands(lua_State *l)
{
  mutt_debug(LL_DEBUG2, "enter\n");
  luaL_newlib(l, LuaMuttCommands);
  int lib_idx = lua_gettop(l);

  // clang-format off
  lua_pushstring(l, "VERSION");     lua_pushstring(l, mutt_make_version()); lua_settable(l, lib_idx);
  lua_pushstring(l, "QUAD_YES");    lua_pushinteger(l, MUTT_YES);           lua_settable(l, lib_idx);
  lua_pushstring(l, "QUAD_NO");     lua_pushinteger(l, MUTT_NO);            lua_settable(l, lib_idx);
  lua_pushstring(l, "QUAD_ASKYES"); lua_pushinteger(l, MUTT_ASKYES);        lua_settable(l, lib_idx);
  lua_pushstring(l, "QUAD_ASKNO");  lua_pushinteger(l, MUTT_ASKNO);         lua_settable(l, lib_idx);
  // clang-format on

  return 1;
}

/**
 * lua_expose_mutt - Expose a 'Mutt' object to the Lua interpreter
 * @param l Lua State
 */
static void lua_expose_mutt(lua_State *l)
{
  luaL_requiref(l, "mutt", lua_expose_commands, 1);
  (void) luaL_dostring(l, "mutt.command = {}");

  const struct Command **cp = NULL;
  ARRAY_FOREACH(cp, &NeoMutt->commands)
  {
    const struct Command *cmd = *cp;

    lua_expose_command(l, cmd);
  }
}

/**
 * lua_init_state - Initialise a Lua State
 * @param[out] l Lua State
 * @retval true Successful
 */
bool lua_init_state(lua_State **l)
{
  if (!l)
    return false;
  if (*l)
    return true;

  mutt_debug(LL_DEBUG2, "enter\n");
  *l = luaL_newstate();

  if (!*l)
  {
    mutt_error(_("Error: Couldn't load the lua interpreter"));
    return false;
  }

  lua_atpanic(*l, lua_handle_panic);

  /* load various Lua libraries */
  luaL_openlibs(*l);
  lua_expose_mutt(*l);

  return true;
}
