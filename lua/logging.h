/**
 * @file
 * Lua Logging
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

#ifndef MUTT_LUA_LOGGING_H
#define MUTT_LUA_LOGGING_H

#include <lua.h>
#include <stdio.h>
#include <time.h>
#include "mutt/lib.h"

ARRAY_HEAD(LongArray, long);

/**
 * struct LuaLogFile - Lua Log File
 */
struct LuaLogFile
{
  FILE *fp;                        ///< File pointer
  bool user_log_file;              ///< Did the user choose the filename?
  long high_water_mark;            ///< File offset of last log reset
  struct LongArray line_offsets;   ///< File offset where each line begins
};

struct LuaLogFile *lua_log_open (void);
void               lua_log_close(struct LuaLogFile **pptr);
void               lua_log_reset(struct LuaLogFile *llf);

void lua_log_init(lua_State *l);

int log_disp_lua(time_t stamp, const char *file, int line, const char *function, enum LogLevel level, const char *format, ...)
__attribute__((__format__(__printf__, 6, 7)));

#define lua_debug(LEVEL, ...) log_disp_lua(0, __FILE__, __LINE__, __func__, LEVEL,      __VA_ARGS__) ///< @ingroup logging_api
#define lua_warning(...)      log_disp_lua(0, __FILE__, __LINE__, __func__, LL_WARNING, __VA_ARGS__) ///< @ingroup logging_api
#define lua_message(...)      log_disp_lua(0, __FILE__, __LINE__, __func__, LL_MESSAGE, __VA_ARGS__) ///< @ingroup logging_api
#define lua_error(...)        log_disp_lua(0, __FILE__, __LINE__, __func__, LL_ERROR,   __VA_ARGS__) ///< @ingroup logging_api

#endif /* MUTT_LUA_LOGGING_H */
