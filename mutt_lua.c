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

#include "config.h"
#include <lauxlib.h>
#include <limits.h>
#include <lua.h>
#include <lualib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "mutt/mutt.h"
#include "mutt.h"
#include "mutt_lua.h"
#include "globals.h"
#include "mailbox.h"
#include "mbtable.h"
#include "mutt_commands.h"
#include "mutt_options.h"
#include "options.h"
#include "protos.h"

static int handle_panic(lua_State *l)
{
  mutt_debug(1, "lua runtime panic: %s\n", lua_tostring(l, -1));
  mutt_error("Lua runtime panic: %s\n", lua_tostring(l, -1));
  lua_pop(l, 1);
  return -1;
}

static int handle_error(lua_State *l)
{
  mutt_debug(1, "lua runtime error: %s\n", lua_tostring(l, -1));
  mutt_error("Lua runtime error: %s\n", lua_tostring(l, -1));
  lua_pop(l, 1);
  return -1;
}

static int lua_mutt_call(lua_State *l)
{
  mutt_debug(2, " * lua_mutt_call()\n");
  struct Buffer token, expn, err;
  char buffer[LONG_STRING] = "";
  const struct Command *command = NULL;
  int rc = 0;

  mutt_buffer_init(&token);
  mutt_buffer_init(&expn);
  mutt_buffer_init(&err);

  err.dsize = STRING;
  err.data = mutt_mem_malloc(err.dsize);

  if (lua_gettop(l) == 0)
  {
    luaL_error(l, "Error command argument required.");
    return -1;
  }

  command = mutt_command_get(lua_tostring(l, 1));
  if (!command)
  {
    luaL_error(l, "Error command %s not found.", lua_tostring(l, 1));
    return -1;
  }

  for (int i = 2; i <= lua_gettop(l); i++)
  {
    const char *s = lua_tostring(l, i);
    mutt_str_strncat(buffer, sizeof(buffer), s, mutt_str_strlen(s));
    mutt_str_strncat(buffer, sizeof(buffer), " ", 1);
  }

  expn.data = expn.dptr = buffer;
  expn.dsize = mutt_str_strlen(buffer);

  if (command->func(&token, &expn, command->data, &err))
  {
    luaL_error(l, "NeoMutt error: %s", err.data);
    rc = -1;
  }
  else
  {
    if (lua_pushstring(l, err.data) == NULL)
      handle_error(l);
    else
      rc++;
  }

  FREE(&err.data);

  return rc;
}

static int lua_mutt_set(lua_State *l)
{
  int rc = -1;
  const char *param = lua_tostring(l, -2);
  mutt_debug(2, " * lua_mutt_set(%s)\n", param);
  struct Option opt;
  char err_str[LONG_STRING];
  struct Buffer err;
  err.data = err_str;
  err.dsize = sizeof(err_str);

  if (mutt_str_strncmp("my_", param, 3) == 0)
  {
    const char *val = lua_tostring(l, -1);
    myvar_set(param, val);
    return 0;
  }
  else if (!mutt_option_get(param, &opt))
  {
    luaL_error(l, "Error getting parameter %s", param);
    return -1;
  }

  rc = 0;
  switch (DTYPE(opt.type))
  {
    case DT_ADDRESS:
    case DT_MBTABLE:
    case DT_REGEX:
    case DT_PATH:
    case DT_SORT:
    case DT_STRING:
      opt.var = mutt_str_strdup(lua_tostring(l, -1));
      rc = mutt_option_set(&opt, &err);
      FREE(&opt.var);
      break;
    case DT_QUAD:
    {
      long num = lua_tointeger(l, -1);
      opt.var = (void *) num;
      if ((num != MUTT_YES) && (num != MUTT_NO) && (num != MUTT_ASKYES) && (num != MUTT_ASKNO))
      {
        luaL_error(l,
                   "Invalid opt for quad option %s (one of "
                   "mutt.QUAD_YES, mutt.QUAD_NO, mutt.QUAD_ASKYES, "
                   "mutt.QUAD_ASKNO",
                   param);
        rc = -1;
      }
      else
      {
        rc = mutt_option_set(&opt, &err);
      }
      break;
    }
    case DT_MAGIC:
      if (mx_set_magic(lua_tostring(l, -1)))
      {
        luaL_error(l, "Invalid mailbox type: %s", opt.var);
        rc = -1;
      }
      break;
    case DT_NUMBER:
    {
      lua_Integer i = lua_tointeger(l, -1);
      if ((i > SHRT_MIN) && (i < SHRT_MAX))
      {
        opt.var = (void *) lua_tointeger(l, -1);
        rc = mutt_option_set(&opt, &err);
      }
      else
      {
        luaL_error(l, "Integer overflow of %d, not in %d-%d", i, SHRT_MIN, SHRT_MAX);
        rc = -1;
      }
      break;
    }
    case DT_BOOL:
      *(bool *) opt.var = lua_toboolean(l, -1);
      break;
    default:
      luaL_error(l, "Unsupported NeoMutt parameter type %d for %s", opt.type, param);
      rc = -1;
      break;
  }

  return rc;
}

static int lua_mutt_get(lua_State *l)
{
  const char *param = lua_tostring(l, -1);
  mutt_debug(2, " * lua_mutt_get(%s)\n", param);
  struct Option opt;

  if (mutt_option_get(param, &opt))
  {
    switch (DTYPE(opt.type))
    {
      case DT_ADDRESS:
      {
        char value[LONG_STRING] = "";
        mutt_addr_write(value, LONG_STRING, *((struct Address **) opt.var), false);
        lua_pushstring(l, value);
        return 1;
      }
      case DT_MBTABLE:
      {
        struct MbTable *tbl = *(struct MbTable **) opt.var;
        lua_pushstring(l, tbl->orig_str);
        return 1;
      }
      case DT_PATH:
      case DT_STRING:
        if (mutt_str_strncmp("my_", param, 3) == 0)
        {
          char *value = (char *) opt.initial;
          lua_pushstring(l, value);
        }
        else
        {
          char *value = NONULL(*((char **) opt.var));
          lua_pushstring(l, value);
        }
        return 1;
      case DT_QUAD:
        lua_pushinteger(l, *(unsigned char *) opt.var);
        return 1;
      case DT_REGEX:
      case DT_MAGIC:
      case DT_SORT:
      {
        char buf[LONG_STRING];
        if (!mutt_option_to_string(&opt, buf, LONG_STRING))
        {
          luaL_error(l, "Couldn't load %s", param);
          return -1;
        }
        lua_pushstring(l, buf);
        return 1;
      }
      case DT_NUMBER:
        lua_pushinteger(l, (signed short) *((unsigned long *) opt.var));
        return 1;
      case DT_BOOL:
        lua_pushboolean(l, *((bool *) opt.var));
        return 1;
      default:
        luaL_error(l, "NeoMutt parameter type %d unknown for %s", opt.type, param);
        return -1;
    }
  }
  mutt_debug(2, " * error\n");
  luaL_error(l, "NeoMutt parameter not found %s", param);
  return -1;
}

static int lua_mutt_enter(lua_State *l)
{
  mutt_debug(2, " * lua_mutt_enter()\n");
  struct Buffer token, err;
  char *buffer = mutt_str_strdup(lua_tostring(l, -1));
  int rc = 0;

  mutt_buffer_init(&err);
  mutt_buffer_init(&token);

  err.dsize = STRING;
  err.data = mutt_mem_malloc(err.dsize);

  if (mutt_parse_rc_line(buffer, &token, &err))
  {
    luaL_error(l, "NeoMutt error: %s", err.data);
    rc = -1;
  }
  else
  {
    if (lua_pushstring(l, err.data) == NULL)
      handle_error(l);
    else
      rc++;
  }

  FREE(&buffer);
  FREE(&err.data);

  return rc;
}

static int lua_mutt_message(lua_State *l)
{
  mutt_debug(2, " * lua_mutt_message()\n");
  const char *msg = lua_tostring(l, -1);
  if (msg)
    mutt_message(msg);
  return 0;
}

static int lua_mutt_error(lua_State *l)
{
  mutt_debug(2, " * lua_mutt_error()\n");
  const char *msg = lua_tostring(l, -1);
  if (msg)
    mutt_error(msg);
  return 0;
}

static void lua_expose_command(void *p, const struct Command *cmd)
{
  lua_State *l = (lua_State *) p;
  char buf[LONG_STRING];
  snprintf(buf, LONG_STRING, "mutt.command.%s = function (...); mutt.call('%s', ...); end",
           cmd->name, cmd->name);
  (void) luaL_dostring(l, buf);
}

static const luaL_Reg luaMuttDecl[] = {
  { "set", lua_mutt_set },       { "get", lua_mutt_get },
  { "call", lua_mutt_call },     { "enter", lua_mutt_enter },
  { "print", lua_mutt_message }, { "message", lua_mutt_message },
  { "error", lua_mutt_error },   { NULL, NULL },
};

#define lua_add_lib_member(LUA, TABLE, KEY, VALUE, DATATYPE_HANDLER)           \
  lua_pushstring(LUA, KEY);                                                    \
  DATATYPE_HANDLER(LUA, VALUE);                                                \
  lua_settable(LUA, TABLE);

static int luaopen_mutt_decl(lua_State *l)
{
  mutt_debug(2, " * luaopen_mutt()\n");
  luaL_newlib(l, luaMuttDecl);
  int lib_idx = lua_gettop(l);
  /*                  table_idx, key        value,               value's type */
  lua_add_lib_member(l, lib_idx, "VERSION", mutt_make_version(), lua_pushstring);
  lua_add_lib_member(l, lib_idx, "QUAD_YES", MUTT_YES, lua_pushinteger);
  lua_add_lib_member(l, lib_idx, "QUAD_NO", MUTT_NO, lua_pushinteger);
  lua_add_lib_member(l, lib_idx, "QUAD_ASKYES", MUTT_ASKYES, lua_pushinteger);
  lua_add_lib_member(l, lib_idx, "QUAD_ASKNO", MUTT_ASKNO, lua_pushinteger);
  return 1;
}

static void luaopen_mutt(lua_State *l)
{
  luaL_requiref(l, "mutt", luaopen_mutt_decl, 1);
  (void) luaL_dostring(l, "mutt.command = {}");
  mutt_commands_apply((void *) l, &lua_expose_command);
}

static bool lua_init(lua_State **l)
{
  if (!*l)
  {
    mutt_debug(2, " * lua_init()\n");
    *l = luaL_newstate();

    if (!*l)
    {
      mutt_error("Error: Couldn't load the lua interpreter.");
      return false;
    }

    lua_atpanic(*l, handle_panic);

    /* load various Lua libraries */
    luaL_openlibs(*l);
    luaopen_mutt(*l);
  }

  return true;
}

/* Public API --------------------------------------------------------------- */

lua_State *Lua = NULL;

int mutt_lua_parse(struct Buffer *tmp, struct Buffer *s, unsigned long data, struct Buffer *err)
{
  lua_init(&Lua);
  mutt_debug(2, " * mutt_lua_parse(%s)\n", tmp->data);

  if (luaL_dostring(Lua, s->dptr))
  {
    mutt_debug(2, " * %s -> failure\n", s->dptr);
    snprintf(err->data, err->dsize, _("%s: %s"), s->dptr, lua_tostring(Lua, -1));
    /* pop error message from the stack */
    lua_pop(Lua, 1);
    return -1;
  }
  mutt_debug(2, " * %s -> success\n", s->dptr);
  return 2;
}

int mutt_lua_source_file(struct Buffer *tmp, struct Buffer *s,
                         unsigned long data, struct Buffer *err)
{
  mutt_debug(2, " * mutt_lua_source()\n");

  lua_init(&Lua);

  char path[_POSIX_PATH_MAX];

  if (mutt_extract_token(tmp, s, 0) != 0)
  {
    snprintf(err->data, err->dsize, _("source: error at %s"), s->dptr);
    return -1;
  }
  if (MoreArgs(s))
  {
    mutt_buffer_printf(err, _("%s: too many arguments"), "source");
    return -1;
  }
  mutt_str_strfcpy(path, tmp->data, sizeof(path));
  mutt_expand_path(path, sizeof(path));

  if (luaL_dofile(Lua, path))
  {
    mutt_error("Couldn't source lua source: %s", lua_tostring(Lua, -1));
    lua_pop(Lua, 1);
    return -1;
  }
  return 2;
}
