/**
 * Copyright (C) 2016 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2016 Bernard Pratz <z+mutt+pub@m0g.net>
 *
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
#include "mutt.h"
#include "mutt_lua.h"
#include "mutt_commands.h"
#include "mutt_options.h"
#include "mx.h"

static int _handle_panic(lua_State *l)
{
  mutt_debug(1, "lua runtime panic: %s\n", lua_tostring(l, -1));
  mutt_error("Lua runtime panic: %s\n", lua_tostring(l, -1));
  lua_pop(l, 1);
  return -1;
}
static int _handle_error(lua_State *l)
{
  mutt_debug(1, "lua runtime error: %s\n", lua_tostring(l, -1));
  mutt_error("Lua runtime error: %s\n", lua_tostring(l, -1));
  lua_pop(l, 1);
  return -1;
}

static int _lua_mutt_call(lua_State *l)
{
  mutt_debug(2, " * _lua_mutt_call()\n");
  BUFFER token, expn, err;
  char buffer[LONG_STRING] = "";
  const struct command_t *command = NULL;
  int rv = 0;

  mutt_buffer_init(&token);
  mutt_buffer_init(&expn);
  mutt_buffer_init(&err);

  err.dsize = STRING;
  err.data = safe_malloc(err.dsize);

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
    safe_strncat(buffer, sizeof(buffer), s, mutt_strlen(s));
    safe_strncat(buffer, sizeof(buffer), " ", 1);
  }

  expn.data = expn.dptr = buffer;
  expn.dsize = mutt_strlen(buffer);

  if (command->func(&token, &expn, command->data, &err))
  {
    luaL_error(l, "Mutt error: %s", err.data);
    rv = -1;
  }
  else
  {
    if (lua_pushstring(l, err.data) == NULL)
      _handle_error(l);
    else
      ++rv;
  }

  FREE(&err.data);

  return rv;
}


static int _lua_mutt_set(lua_State *l)
{
  int rv = -1;
  BUFFER err;
  const char *param = lua_tostring(l, -2);
  mutt_debug(2, " * _lua_mutt_set(%s)\n", param);
  struct option_t *value = safe_malloc(sizeof(struct option_t));
  const struct option_t *tmp = mutt_option_get(param);
  if (tmp == NULL)
  {
    luaL_error(l, "Error getting parameter %s", param);
    return -1;
  }

  memcpy(value, tmp, sizeof(struct option_t));
  if (value)
  {
    rv = 0;
    switch (DTYPE(value->type))
    {
      case DT_ADDR:
      case DT_MBCHARTBL:
      case DT_RX:
      case DT_PATH:
      case DT_SORT:
      case DT_STR:
        value->data = (long) safe_strdup(lua_tostring(l, -1));
        rv = mutt_option_set(value, &err);
        FREE(&value->data);
        break;
      case DT_QUAD:
        value->data = (long) lua_tointeger(l, -1);
        if ((value->data != MUTT_YES) && (value->data != MUTT_NO) &&
            (value->data != MUTT_ASKYES) && (value->data != MUTT_ASKNO))
        {
          luaL_error(l, "Invalid value for quad option %s (one of "
                        "mutt.QUAD_YES, mutt.QUAD_NO, mutt.QUAD_ASKYES, "
                        "mutt.QUAD_ASKNO",
                     param);
          rv = -1;
        }
        else
          rv = mutt_option_set(value, &err);
        break;
      case DT_MAGIC:
        if (mx_set_magic(lua_tostring(l, -1)))
        {
          luaL_error(l, "Invalid mailbox type: %s", value->data);
          rv = -1;
        }
        break;
      case DT_NUM:
      {
        lua_Integer i = lua_tointeger(l, -1);
        if ((i > SHRT_MIN) && (i < SHRT_MAX))
        {
          value->data = lua_tointeger(l, -1);
          rv = mutt_option_set(value, &err);
        }
        else
        {
          luaL_error(l, "Integer overflow of %d, not in %d-%d", i, SHRT_MIN, SHRT_MAX);
          rv = -1;
        }
        break;
      }
      case DT_BOOL:
        value->data = (long) lua_toboolean(l, -1);
        rv = mutt_option_set(value, &err);
        break;
      default:
        luaL_error(l, "Unsupported Mutt parameter type %d for %s", value->type, param);
        rv = -1;
        break;
    }
  }
  else
  {
    mutt_debug(2, " * _lua_mutt_get(%s) -> error\n", param);
    luaL_error(l, "Mutt parameter not found %s", param);
    rv = -1;
  }
  FREE(&value);
  return rv;
}

static int _lua_mutt_get(lua_State *l)
{
  const char *param = lua_tostring(l, -1);
  mutt_debug(2, " * _lua_mutt_get(%s)\n", param);
  const struct option_t *opt = mutt_option_get(param);
  if (opt)
    switch (opt->type & DT_MASK)
    {
      case DT_ADDR:
      {
        char value[LONG_STRING] = "";
        rfc822_write_address(value, LONG_STRING, *((ADDRESS **) opt->data), 0);
        lua_pushstring(l, value);
        return 1;
      }
      case DT_MBCHARTBL:
      {
        mbchar_table *tbl = *(mbchar_table **) opt->data;
        lua_pushstring(l, tbl->orig_str);
        return 1;
      }
      case DT_PATH:
      case DT_STR:
        if (mutt_strncmp("my_", param, 3) == 0)
        {
          char *option = (char *) opt->option;
          char *value = (char *) opt->data;
          lua_pushstring(l, value);
          FREE(&option);
          FREE(&value);
          FREE(&opt);
        }
        else
        {
          char *value = NONULL(*((char **) opt->data));
          lua_pushstring(l, value);
        }
        return 1;
      case DT_QUAD:
        lua_pushinteger(l, quadoption(opt->data));
        return 1;
      case DT_RX:
      case DT_MAGIC:
      case DT_SORT:
      {
        char buf[LONG_STRING];
        if (!mutt_option_to_string(opt, buf, LONG_STRING))
        {
          luaL_error(l, "Couldn't load %s", param);
          return -1;
        }
        lua_pushstring(l, buf);
        return 1;
      }
      case DT_NUM:
        lua_pushinteger(l, (signed short) *((unsigned long *) opt->data));
        return 1;
      case DT_BOOL:
        lua_pushboolean(l, option(opt->data));
        return 1;
      default:
        luaL_error(l, "Mutt parameter type %d unknown for %s", opt->type, param);
        return -1;
    }
  mutt_debug(2, " * _lua_mutt_get() -> error\n");
  luaL_error(l, "Mutt parameter not found %s", param);
  return -1;
}

static int _lua_mutt_enter(lua_State *l)
{
  mutt_debug(2, " * _lua_mutt_enter()\n");
  BUFFER token, err;
  char *buffer = safe_strdup(lua_tostring(l, -1));
  int rv = 0;

  mutt_buffer_init(&err);
  mutt_buffer_init(&token);

  err.dsize = STRING;
  err.data = safe_malloc(err.dsize);

  if (mutt_parse_rc_line(buffer, &token, &err))
  {
    luaL_error(l, "Mutt error: %s", err.data);
    rv = -1;
  }
  else
  {
    if (lua_pushstring(l, err.data) == NULL)
      _handle_error(l);
    else
      ++rv;
  }

  FREE(&buffer);
  FREE(&err.data);

  return rv;
}

static int _lua_mutt_message(lua_State *l)
{
  mutt_debug(2, " * _lua_mutt_message()\n");
  const char *msg = lua_tostring(l, -1);
  if (msg != NULL)
    mutt_message(msg);
  return 0;
}

static int _lua_mutt_error(lua_State *l)
{
  mutt_debug(2, " * _lua_mutt_error()\n");
  const char *msg = lua_tostring(l, -1);
  if (msg != NULL)
    mutt_error(msg);
  return 0;
}

static void _lua_expose_command(void *p, const struct command_t *cmd)
{
  lua_State *l = (lua_State *) p;
  char buf[LONG_STRING];
  snprintf(buf, LONG_STRING,
           "mutt.command.%s = function (...); mutt.call('%s', ...); end",
           cmd->name, cmd->name);
  luaL_dostring(l, buf);
}

static const luaL_Reg luaMuttDecl[] = {
    {"set", _lua_mutt_set},       {"get", _lua_mutt_get},
    {"call", _lua_mutt_call},     {"enter", _lua_mutt_enter},
    {"print", _lua_mutt_message}, {"message", _lua_mutt_message},
    {"error", _lua_mutt_error},   {NULL, NULL}};

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
  luaL_dostring(l, "mutt.command = {}");
  mutt_commands_apply((void *) l, &_lua_expose_command);
}

static bool _lua_init(lua_State **l)
{
  if (*l == NULL)
  {
    mutt_debug(2, " * lua_init()\n");
    *l = luaL_newstate();

    if (*l == NULL)
    {
      mutt_error("Error: Couldn't load the lua interpreter.");
      return false;
    }

    lua_atpanic(*l, _handle_panic);

    /* load various Lua libraries */
    luaL_openlibs(*l);
    luaopen_mutt(*l);
  }

  return true;
}

/* Public API --------------------------------------------------------------- */

lua_State *Lua = NULL;

int mutt_lua_parse(BUFFER *tmp, BUFFER *s, unsigned long data, BUFFER *err)
{
  _lua_init(&Lua);
  mutt_debug(2, " * mutt_lua_parse(%s)\n", tmp->data);

  if (luaL_dostring(Lua, s->dptr))
  {
    mutt_debug(2, " * mutt_lua_parse(%s) -> failure\n", s->dptr);
    snprintf(err->data, err->dsize, _("%s: %s"), s->dptr, lua_tostring(Lua, -1));
    /* pop error message from the stack */
    lua_pop(Lua, 1);
    return -1;
  }
  mutt_debug(2, " * mutt_lua_parse(%s) -> success\n", s->dptr);
  return 2;
}

int mutt_lua_source_file(BUFFER *tmp, BUFFER *s, unsigned long data, BUFFER *err)
{
  mutt_debug(2, " * mutt_lua_source()\n");

  _lua_init(&Lua);

  char path[_POSIX_PATH_MAX];

  if (mutt_extract_token(tmp, s, 0) != 0)
  {
    snprintf(err->data, err->dsize, _("source: error at %s"), s->dptr);
    return -1;
  }
  if (MoreArgs(s))
  {
    strfcpy(err->data, _("source: too many arguments"), err->dsize);
    return -1;
  }
  strfcpy(path, tmp->data, sizeof(path));
  mutt_expand_path(path, sizeof(path));

  if (luaL_dofile(Lua, path))
  {
    mutt_error("Couldn't source lua source: %s", lua_tostring(Lua, -1));
    lua_pop(Lua, 1);
    return -1;
  }
  return 2;
}
