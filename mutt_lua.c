/**
 * @file
 * Integrated Lua scripting
 *
 * @authors
 * Copyright (C) 2016 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2016 Bernard Pratz <z+mutt+pub@m0g.net>
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
 * @page neo_mutt_lua Integrated Lua scripting
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
#include <limits.h>
#include <lua.h>
#include <lualib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "mutt_lua.h"
#include "parse/lib.h"
#include "muttlib.h"

/// Global Lua State
static lua_State *LuaState = NULL;

/**
 * LuaCommands - List of NeoMutt commands to register
 */
static const struct Command LuaCommands[] = {
  // clang-format off
  { "lua",        mutt_lua_parse,       0 },
  { "lua-source", mutt_lua_source_file, 0 },
  // clang-format on
};

/**
 * handle_panic - Handle a panic in the Lua interpreter
 * @param l Lua State
 * @retval -1 Always
 */
static int handle_panic(lua_State *l)
{
  mutt_debug(LL_DEBUG1, "lua runtime panic: %s\n", lua_tostring(l, -1));
  mutt_error("Lua runtime panic: %s", lua_tostring(l, -1));
  lua_pop(l, 1);
  return -1;
}

/**
 * handle_error - Handle an error in the Lua interpreter
 * @param l Lua State
 * @retval -1 Always
 */
static int handle_error(lua_State *l)
{
  mutt_debug(LL_DEBUG1, "lua runtime error: %s\n", lua_tostring(l, -1));
  mutt_error("Lua runtime error: %s", lua_tostring(l, -1));
  lua_pop(l, 1);
  return -1;
}

/**
 * lua_mutt_call - Call a NeoMutt command by name
 * @param l Lua State
 * @retval >=0 Success
 * @retval -1 Error
 */
static int lua_mutt_call(lua_State *l)
{
  mutt_debug(LL_DEBUG2, " * lua_mutt_call()\n");
  struct Buffer *err = buf_pool_get();
  struct Buffer *token = buf_pool_get();
  char buf[1024] = { 0 };
  const struct Command *cmd = NULL;
  int rc = 0;

  if (lua_gettop(l) == 0)
  {
    luaL_error(l, "Error command argument required.");
    return -1;
  }

  cmd = command_get(lua_tostring(l, 1));
  if (!cmd)
  {
    luaL_error(l, "Error command %s not found.", lua_tostring(l, 1));
    return -1;
  }

  for (int i = 2; i <= lua_gettop(l); i++)
  {
    const char *s = lua_tostring(l, i);
    mutt_strn_cat(buf, sizeof(buf), s, mutt_str_len(s));
    mutt_strn_cat(buf, sizeof(buf), " ", 1);
  }

  struct Buffer expn = buf_make(0);
  expn.data = buf;
  expn.dptr = buf;
  expn.dsize = mutt_str_len(buf);

  if (cmd->parse(token, &expn, cmd->data, err))
  {
    luaL_error(l, "NeoMutt error: %s", buf_string(err));
    rc = -1;
  }
  else
  {
    if (!lua_pushstring(l, buf_string(err)))
      handle_error(l);
    else
      rc++;
  }

  buf_pool_release(&token);
  buf_pool_release(&err);
  return rc;
}

/**
 * lua_mutt_set - Set a NeoMutt variable
 * @param l Lua State
 * @retval  0 Success
 * @retval -1 Error
 */
static int lua_mutt_set(lua_State *l)
{
  const char *param = lua_tostring(l, -2);
  mutt_debug(LL_DEBUG2, " * lua_mutt_set(%s)\n", param);

  struct Buffer err = buf_make(256);
  struct HashElem *he = cs_subset_lookup(NeoMutt->sub, param);
  if (!he)
  {
    // In case it is a my_var, we have to create it
    if (mutt_str_startswith(param, "my_"))
    {
      struct ConfigDef my_cdef = { 0 };
      my_cdef.name = param;
      my_cdef.type = DT_MYVAR;
      he = cs_create_variable(NeoMutt->sub->cs, &my_cdef, &err);
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

  switch (DTYPE(cdef->type))
  {
    case DT_ADDRESS:
    case DT_ENUM:
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
      struct Buffer value_buf = buf_make(val_size);
      buf_strcpy_n(&value_buf, value, val_size);
      if (DTYPE(he->type) == DT_PATH)
        buf_expand_path(&value_buf);

      int rv = cs_subset_he_string_set(NeoMutt->sub, he, value_buf.data, &err);
      buf_dealloc(&value_buf);
      if (CSR_RESULT(rv) != CSR_SUCCESS)
        rc = -1;
      break;
    }
    case DT_NUMBER:
    case DT_QUAD:
    {
      const intptr_t value = lua_tointeger(l, -1);
      int rv = cs_subset_he_native_set(NeoMutt->sub, he, value, &err);
      if (CSR_RESULT(rv) != CSR_SUCCESS)
        rc = -1;
      break;
    }
    case DT_BOOL:
    {
      const intptr_t value = lua_toboolean(l, -1);
      int rv = cs_subset_he_native_set(NeoMutt->sub, he, value, &err);
      if (CSR_RESULT(rv) != CSR_SUCCESS)
        rc = -1;
      break;
    }
    default:
      luaL_error(l, "Unsupported NeoMutt parameter type %d for %s", DTYPE(cdef->type), param);
      rc = -1;
      break;
  }

  buf_dealloc(&err);
  return rc;
}

/**
 * lua_mutt_get - Get a NeoMutt variable
 * @param l Lua State
 * @retval  1 Success
 * @retval -1 Error
 */
static int lua_mutt_get(lua_State *l)
{
  const char *param = lua_tostring(l, -1);
  mutt_debug(LL_DEBUG2, " * lua_mutt_get(%s)\n", param);

  struct HashElem *he = cs_subset_lookup(NeoMutt->sub, param);
  if (!he)
  {
    mutt_debug(LL_DEBUG2, " * error\n");
    luaL_error(l, "NeoMutt parameter not found %s", param);
    return -1;
  }

  struct ConfigDef *cdef = he->data;

  switch (DTYPE(cdef->type))
  {
    case DT_ADDRESS:
    case DT_ENUM:
    case DT_MBTABLE:
    case DT_MYVAR:
    case DT_REGEX:
    case DT_SLIST:
    case DT_SORT:
    case DT_STRING:
    {
      struct Buffer value = buf_make(256);
      int rc = cs_subset_he_string_get(NeoMutt->sub, he, &value);
      if (CSR_RESULT(rc) != CSR_SUCCESS)
      {
        buf_dealloc(&value);
        return -1;
      }

      struct Buffer escaped = buf_make(256);
      escape_string(&escaped, value.data);
      lua_pushstring(l, escaped.data);
      buf_dealloc(&value);
      buf_dealloc(&escaped);
      return 1;
    }
    case DT_QUAD:
      lua_pushinteger(l, (unsigned char) cdef->var);
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
 * lua_mutt_enter - Execute NeoMutt config from Lua
 * @param l Lua State
 * @retval >=0 Success
 * @retval -1  Error
 */
static int lua_mutt_enter(lua_State *l)
{
  mutt_debug(LL_DEBUG2, " * lua_mutt_enter()\n");
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
      handle_error(l);
    else
      rc++;
  }

  FREE(&buf);
  buf_pool_release(&err);

  return rc;
}

/**
 * lua_mutt_message - Display a message in Neomutt
 * @param l Lua State
 * @retval 0 Always
 */
static int lua_mutt_message(lua_State *l)
{
  mutt_debug(LL_DEBUG2, " * lua_mutt_message()\n");
  const char *msg = lua_tostring(l, -1);
  if (msg)
    mutt_message("%s", msg);
  return 0;
}

/**
 * lua_mutt_error - Display an error in Neomutt
 * @param l Lua State
 * @retval 0 Always
 */
static int lua_mutt_error(lua_State *l)
{
  mutt_debug(LL_DEBUG2, " * lua_mutt_error()\n");
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
 * and it will call lua_mutt_message()
 */
static const luaL_Reg LuaMuttCommands[] = {
  // clang-format off
  { "set",     lua_mutt_set },
  { "get",     lua_mutt_get },
  { "call",    lua_mutt_call },
  { "enter",   lua_mutt_enter },
  { "print",   lua_mutt_message },
  { "message", lua_mutt_message },
  { "error",   lua_mutt_error },
  { NULL, NULL },
  // clang-format on
};

/**
 * luaopen_mutt_decl - Declare some NeoMutt types to the Lua interpreter
 * @param l Lua State
 * @retval 1 Always
 */
static int luaopen_mutt_decl(lua_State *l)
{
  mutt_debug(LL_DEBUG2, " * luaopen_mutt()\n");
  luaL_newlib(l, LuaMuttCommands);
  int lib_idx = lua_gettop(l);

  // clang-format off
  lua_pushstring(l, "VERSION");     lua_pushstring(l, mutt_make_version()); lua_settable(l, lib_idx);;
  lua_pushstring(l, "QUAD_YES");    lua_pushinteger(l, MUTT_YES);           lua_settable(l, lib_idx);;
  lua_pushstring(l, "QUAD_NO");     lua_pushinteger(l, MUTT_NO);            lua_settable(l, lib_idx);;
  lua_pushstring(l, "QUAD_ASKYES"); lua_pushinteger(l, MUTT_ASKYES);        lua_settable(l, lib_idx);;
  lua_pushstring(l, "QUAD_ASKNO");  lua_pushinteger(l, MUTT_ASKNO);         lua_settable(l, lib_idx);;
  // clang-format on

  return 1;
}

/**
 * luaopen_mutt - Expose a 'Mutt' object to the Lua interpreter
 * @param l Lua State
 */
static void luaopen_mutt(lua_State *l)
{
  luaL_requiref(l, "mutt", luaopen_mutt_decl, 1);
  (void) luaL_dostring(l, "mutt.command = {}");

  struct Command *c = NULL;
  for (size_t i = 0, size = commands_array(&c); i < size; i++)
  {
    lua_expose_command(l, &c[i]);
  }
}

/**
 * lua_init - Initialise a Lua State
 * @param[out] l Lua State
 * @retval true Successful
 */
static bool lua_init(lua_State **l)
{
  if (!l)
    return false;
  if (*l)
    return true;

  mutt_debug(LL_DEBUG2, " * lua_init()\n");
  *l = luaL_newstate();

  if (!*l)
  {
    mutt_error(_("Error: Couldn't load the lua interpreter"));
    return false;
  }

  lua_atpanic(*l, handle_panic);

  /* load various Lua libraries */
  luaL_openlibs(*l);
  luaopen_mutt(*l);

  return true;
}

/**
 * mutt_lua_init - Setup feature commands
 */
void mutt_lua_init(void)
{
  commands_register(LuaCommands, mutt_array_size(LuaCommands));
}

/**
 * mutt_lua_parse - Parse the 'lua' command - Implements Command::parse() - @ingroup command_parse
 */
enum CommandResult mutt_lua_parse(struct Buffer *buf, struct Buffer *s,
                                  intptr_t data, struct Buffer *err)
{
  lua_init(&LuaState);
  mutt_debug(LL_DEBUG2, " * mutt_lua_parse(%s)\n", buf->data);

  if (luaL_dostring(LuaState, s->dptr))
  {
    mutt_debug(LL_DEBUG2, " * %s -> failure\n", s->dptr);
    buf_printf(err, "%s: %s", s->dptr, lua_tostring(LuaState, -1));
    /* pop error message from the stack */
    lua_pop(LuaState, 1);
    return MUTT_CMD_ERROR;
  }
  mutt_debug(LL_DEBUG2, " * %s -> success\n", s->dptr);
  buf_reset(s); // Clear the rest of the line
  return MUTT_CMD_SUCCESS;
}

/**
 * mutt_lua_source_file - Parse the 'lua-source' command - Implements Command::parse() - @ingroup command_parse
 */
enum CommandResult mutt_lua_source_file(struct Buffer *buf, struct Buffer *s,
                                        intptr_t data, struct Buffer *err)
{
  mutt_debug(LL_DEBUG2, " * mutt_lua_source()\n");

  lua_init(&LuaState);

  char path[PATH_MAX] = { 0 };

  if (parse_extract_token(buf, s, TOKEN_NO_FLAGS) != 0)
  {
    buf_printf(err, _("source: error at %s"), s->dptr);
    return MUTT_CMD_ERROR;
  }
  if (MoreArgs(s))
  {
    buf_printf(err, _("%s: too many arguments"), "source");
    return MUTT_CMD_WARNING;
  }
  mutt_str_copy(path, buf->data, sizeof(path));
  mutt_expand_path(path, sizeof(path));

  if (luaL_dofile(LuaState, path))
  {
    mutt_error(_("Couldn't source lua source: %s"), lua_tostring(LuaState, -1));
    lua_pop(LuaState, 1);
    return MUTT_CMD_ERROR;
  }
  return MUTT_CMD_SUCCESS;
}
