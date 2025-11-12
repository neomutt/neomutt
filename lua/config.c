/**
 * @file
 * Lua Config Wrapper
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
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @page lua_config Lua Config Wrapper
 *
 * ## `config` Object
 *
 * NeoMutt defines a global Lua object: `config`.
 * This gives scripts access to NeoMutt Config System.
 *
 * `config` exposes several methods, defined in #ConfigMethods.
 *
 * ## Normal Methods
 *
 * `config` exposes several normal methods:
 * - [get()](#lua_config_cb_get)
 * - [reset()](#lua_config_cb_reset)
 * - [set()](#lua_config_cb_set)
 * - [toggle()](#lua_config_cb_toggle)
 *
 * Scripts can call these to directly alter Config Options.
 * e.g.
 * ```lua
 * local sbw = config:get("sidebar_width")
 * ```
 *
 * ## MetaMethods
 *
 * For convenience, `config` implements two MetaMethods: `__index()` and `__newindex()`.
 * When a script tries to access an unknown key, these methods intercept the action.
 *
 * ### `__index()`
 *
 * - print(config.sidebar_width)`
 * - `config["sidebar_width"]`
 *
 * NeoMutt dynamically looks up the key, "sidebar_width" in the Config System and returns the value.
 *
 * ### `__newindex()`
 *
 * - `config.sidebar_width = 30`
 * - `config["sidebar_width"] = 30`
 *
 * NeoMutt dynamically looks up the key, "sidebar_width" in the Config System and sets the value.
 */

#include "config.h"
#include <lauxlib.h>
#include <lua.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "helpers.h"
#include "logging.h"
#include "mutt_logging.h"
#include "muttlib.h"

/**
 * LuaVars - Config definitions for the lua library
 */
static struct ConfigDef LuaVars[] = {
  // clang-format off
  { "lua_debug_file", DT_PATH|D_PATH_FILE, 0, 0, NULL,
    "File to save Lua debug logs"
  },
  { "lua_debug_level", DT_NUMBER, 0, 1, debug_level_validator,
    "Logging level for Lua debug logs"
  },
  { NULL },
  // clang-format on
};

/**
 * config_init_lua - Register lua config variables - Implements ::module_init_config_t - @ingroup cfg_module_api
 */
bool config_init_lua(struct ConfigSet *cs)
{
  return cs_register_variables(cs, LuaVars);
}

/**
 * lua_config_cb_get - Get a Config Option - Implements ::lua_callback_t
 *
 * **Calling Lua Stack:**
 * * (-1) String: Config Option
 *
 * **Sucess: Return Lua Stack:**
 * * (1) Bool/Int/String: Config Value
 *
 * **Error: Return Lua Stack:**
 * * (1) Number: -1
 * * (2) String: Error message (OPTIONAL)
 */
static int lua_config_cb_get(lua_State *l)
{
  const char *param = lua_tostring(l, -1);
  lua_debug(LL_DEBUG2, "%s\n", param);

  struct HashElem *he = cs_subset_lookup(NeoMutt->sub, param);
  if (!he)
  {
    lua_debug(LL_DEBUG2, "error\n");
    // luaL_error(l, "NeoMutt parameter not found %s", param);
    // return -1;
    lua_newtable(l);

    lua_pushstring(l, "error");
    lua_pushstring(l, "NeoMutt parameter not found");
    lua_settable(l, -3);

    lua_pushstring(l, "code");
    lua_pushinteger(l, 42);
    lua_settable(l, -3);

    lua_pushstring(l, "retval");
    lua_pushinteger(l, -1);
    lua_settable(l, -3);

    return 1;
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
        lua_pushnil(l);
        return 1;
      }

      struct Buffer *escaped = buf_pool_get();
      escape_string(escaped, buf_string(value));
      lua_pushstring(l, buf_string(escaped));
      buf_pool_release(&value);
      buf_pool_release(&escaped);
      return 1;
    }
    case DT_QUAD:
    {
      lua_pushinteger(l, (unsigned char) cdef->var);
      return 1;
    }
    case DT_LONG:
    {
      lua_pushinteger(l, (signed long) cdef->var);
      return 1;
    }
    case DT_NUMBER:
    {
      lua_pushinteger(l, (signed short) cdef->var);
      return 1;
    }
    case DT_BOOL:
    {
      lua_pushboolean(l, (bool) cdef->var);
      return 1;
    }
    default:
    {
      lua_pushnil(l);
      // luaL_error(l, "NeoMutt parameter type %d unknown for %s", cdef->type, param);
      return 1;
    }
  }
}

/**
 * lua_config_cb_set - Set a Config Option - Implements ::lua_callback_t
 *
 * Triggered when a script does `config.NAME = VALUE`
 *
 * Calling Lua Stack:
 * * (-2) String: Config Option
 * * (-1) String: Value
 *
 * Success: Return Lua Stack:
 * * (1) Number: 0 Success
 *
 * Error: Return Lua Stack:
 * * (1) Number: -1
 * * (2) String: Error message (OPTIONAL)
 */
static int lua_config_cb_set(lua_State *l)
{
  const char *param = lua_tostring(l, -2);
  lua_debug(LL_DEBUG2, "%s\n", param);

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
      struct Buffer *value_buf = buf_pool_get();
      buf_strcpy(value_buf, value);
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
      int rv = 0;
      if (lua_isboolean(l, -1))
      {
        const intptr_t value = lua_toboolean(l, -1);
        cs_subset_he_native_set(NeoMutt->sub, he, value, err);
      }
      else
      {
        const char *value = lua_tostring(l, -1);
        cs_subset_he_string_set(NeoMutt->sub, he, value, err);
      }
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
 * lua_config_cb_index - Callback for Lua Metamethod Index::__newindex - Implements ::lua_callback_t
 *
 * Triggered when a script does `config.NAME = VALUE`
 *
 * @sa lua_config_cb_set()
 */
static int lua_config_cb_index(lua_State *l)
{
  // Check if NAME exists in the Config table
  bool found = lua_index_lookup(l, "Config");
  if (found)
    return 1;

  // If not, try to set the value of a Config Option
  return lua_config_cb_get(l);
}

/**
 * lua_config_cb_newindex - Callback for Lua Metamethod Index::__index - Implements ::lua_callback_t
 *
 * XXX: Usage: config.NAME = VALUE
 * XXX: Retval: none, save results (success, fail-name, fail-validator)
 * XXX: Error: logged (neo,lua)
 *
 * Lua Stack:
 * * (-3) Table: "Config"
 * * (-2) String: Config Name
 * * (-1) String: Value
 */
static int lua_config_cb_newindex(lua_State *l)
{
  // Try actual metatable methods first
  luaL_getmetatable(l, "Config"); // mt
  lua_pushvalue(l, 2);            // key
  lua_rawget(l, -2);              // mt[key]

  if (!lua_isnil(l, -1))
  {
    lua_debug(LL_DEBUG1, "REAL %s\n", luaL_checkstring(l, 2));
    return 1; // Found a real method
  }

  lua_pop(l, 2); // remove nil + metatable

  return lua_config_cb_set(l);
}

/**
 * lua_config_cb_reset - Reset a Config Option - Implements ::lua_callback_t
 *
 * Callback for config:reset()
 *
 * Calling Lua Stack:
 * * (-1) String: Config Option
 *
 * Success: Return Lua Stack:
 * * (1) Number: 0 Success
 *
 * Error: Return Lua Stack:
 * * (1) Number: -1
 * * (2) String: Error message (OPTIONAL)
 */
static int lua_config_cb_reset(lua_State *l)
{
  int top = lua_gettop(l);
  if (top != 1)
  {
    const char *msg = "Usage: reset(NAME)";
    lua_debug(LL_DEBUG1, "%s\n", msg);
    lua_pushinteger(l, -1); // Error
    lua_pushstring(l, msg);
    return 2;
  }

  const char *name = lua_tostring(l, 1);

  struct HashElem *he = cs_subset_lookup(NeoMutt->sub, name);
  if (!he)
  {
    const char *msg = "Unknown config";
    lua_debug(LL_DEBUG1, "%s\n", msg);
    lua_pushinteger(l, -1); // Error
    lua_pushstring(l, msg);
    return 2;
  }

  struct Buffer *err = buf_pool_get();
  int rc = cs_subset_he_reset(NeoMutt->sub, he, err);

  if (rc != CSR_SUCCESS)
  {
    lua_debug(LL_DEBUG1, "%s\n", buf_string(err));
    lua_pushinteger(l, -1); // Error
    lua_pushstring(l, buf_string(err));
    buf_pool_release(&err);
    return 2;
  }

  buf_pool_release(&err);
  lua_pushinteger(l, 0); // Success
  return 1;
}

/**
 * lua_config_cb_toggle - Callback for config:toggle() - Implements ::lua_callback_t
 *
 * Callback for config:toggle()
 *
 * Calling Lua Stack:
 * * (-1) String: Config Option
 *
 * Success: Return Lua Stack:
 * * (1) Number: 0 Success
 *
 * Error: Return Lua Stack:
 * * (1) Number: -1
 * * (2) String: Error message (OPTIONAL)
 */
static int lua_config_cb_toggle(lua_State *l)
{
  int top = lua_gettop(l);
  if (top != 1)
  {
    const char *msg = "Usage: toggle(NAME)";
    lua_debug(LL_DEBUG1, "%s\n", msg);
    lua_pushinteger(l, -1); // Error
    lua_pushstring(l, msg);
    return 2;
  }

  const char *name = lua_tostring(l, 1);

  struct HashElem *he = cs_subset_lookup(NeoMutt->sub, name);
  if (!he)
  {
    const char *msg = "Unknown config";
    lua_debug(LL_DEBUG1, "%s\n", msg);
    lua_pushinteger(l, -1); // Error
    lua_pushstring(l, msg);
    return 2;
  }

  struct Buffer *err = buf_pool_get();
  int rc = CSR_SUCCESS;
  switch (CONFIG_TYPE(he->type))
  {
    case DT_BOOL:
    {
      rc = bool_he_toggle(NeoMutt->sub, he, err);
      break;
    }

    case DT_NUMBER:
    {
      rc = number_he_toggle(NeoMutt->sub, he, err);
      break;
    }

    case DT_QUAD:
    {
      rc = quad_he_toggle(NeoMutt->sub, he, err);
      break;
    }

    default:
    {
      const char *msg = "Only toggle bool,number,quad";
      lua_debug(LL_DEBUG1, "%s\n", msg);
      lua_pushinteger(l, -1); // Error
      lua_pushstring(l, msg);
      buf_pool_release(&err);
      return 2;
    }
  }

  if (rc != CSR_SUCCESS)
  {
    lua_debug(LL_DEBUG1, "%s\n", buf_string(err));
    lua_pushinteger(l, -1); // Error
    lua_pushstring(l, buf_string(err));
    buf_pool_release(&err);
    return 2;
  }

  lua_debug(LL_DEBUG1, "toggle: %s\n", name);
  buf_pool_release(&err);

  lua_pushinteger(l, 0); // Success
  return 1;
}

/**
 * ConfigMethods - Config Class Methods
 */
static const struct luaL_Reg ConfigMethods[] = {
  // clang-format off
  { "__index",    lua_config_cb_index    },
  { "__newindex", lua_config_cb_newindex },
  { "get",        lua_config_cb_get      },
  { "reset",      lua_config_cb_reset    },
  { "set",        lua_config_cb_set      },
  { "toggle",     lua_config_cb_toggle   },
  { NULL, NULL },
  // clang-format on
};

/**
 * lua_config_class - Declare the Config Class
 * @param l Lua State
 *
 * Define a "Config" class and implement two metamethods:
 * - __index()    - Dynamically lookup class members
 * - __newindex() - Dynamically set    class members
 */
void lua_config_class(lua_State *l)
{
  luaL_newmetatable(l, "Config");
  luaL_setfuncs(l, ConfigMethods, 0);
  lua_pop(l, 1);
}

/**
 * lua_config_init - Initialise the Lua `config` object
 */
void lua_config_init(lua_State *l)
{
  lua_newtable(l);
  luaL_getmetatable(l, "Config");
  lua_setmetatable(l, -2);

  lua_setglobal(l, "config");
}
