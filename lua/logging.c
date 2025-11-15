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

/**
 * @page lua_logging Lua Logging
 *
 * Lua Logging
 */

#include <errno.h>
#include <lauxlib.h>
#include <lua.h>
#include <stdarg.h>
#include <string.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "logging.h"
#include "console.h"
#include "helpers.h"
#include "module.h"

/**
 * dump_string - XXX
 */
void dump_string(const char *str)
{
  struct Buffer *buf = buf_pool_get();

  buf_strcpy(buf, str);

  char *nl = NULL;
  while ((nl = strchr(buf->data, '\n')))
  {
    *nl = '|';
  }

  lua_debug(LL_DEBUG1, "%s\n", buf_string(buf));
  buf_pool_release(&buf);
}

/**
 * dump_lines - XXX
 */
void dump_lines(struct LongArray *la)
{
  struct Buffer *buf = buf_pool_get();
  buf_addch(buf, '(');
  long *lp = NULL;
  ARRAY_FOREACH(lp, la)
  {
    buf_add_printf(buf, "%ld", *lp);
    if (ARRAY_FOREACH_IDX_lp < (ARRAY_SIZE(la) - 1))
      buf_addstr(buf, ", ");
  }
  buf_addch(buf, ')');
  lua_debug(LL_DEBUG1, "%s\n", buf_string(buf));

  buf_pool_release(&buf);
}

/**
 * lua_log_open - Open a dedicated Lua Log File
 * @retval ptr LuaLogFile
 *
 * If `$lua_debug_file` is set, this filename will be used.
 * If not, a temporary file will be created (and deleted).
 */
struct LuaLogFile *lua_log_open(void)
{
  struct LuaLogFile *llf = MUTT_MEM_CALLOC(1, struct LuaLogFile);

  const char *const c_lua_debug_file = cs_subset_path(NeoMutt->sub, "lua_debug_file");
  if (c_lua_debug_file)
  {
    llf->fp = mutt_file_fopen(c_lua_debug_file, "a+");
    llf->user_log_file = true;
  }
  else
  {
    llf->fp = mutt_file_mkstemp();
    llf->user_log_file = false;
  }

  if (!llf->fp)
  {
    mutt_perror("%s", "lua_log_open");
    FREE(&llf);
    return NULL;
  }

  fseek(llf->fp, 0, SEEK_END);
  llf->high_water_mark = ftell(llf->fp);
  ARRAY_ADD(&llf->line_offsets, llf->high_water_mark);

  return llf;
}

/**
 * lua_log_close - Close the Lua Log File
 * @param pptr LuaLogFile to close
 *
 * If `$lua_debug_file` was set, the file is closed.
 * If not, closing the temporary file will cause it to be deleted.
 */
void lua_log_close(struct LuaLogFile **pptr)
{
  if (!pptr || !*pptr)
    return;

  struct LuaLogFile *llf = *pptr;

  mutt_file_fclose(&llf->fp);
  ARRAY_FREE(&llf->line_offsets);
  FREE(pptr);
}

/**
 * lua_log_reset - Reset the Lua Log File
 * @param llf Lua Log File
 *
 * If `$lua_debug_file` was set, then record the size of the log file.
 * This 'high water mark' will be used in the Lua Console to only show newer
 * log entries.
 *
 * If the config option isn't set, the temporary file will be truncated.
 *
 * @note Logging _this_ is self-defeating :-)
 */
void lua_log_reset(struct LuaLogFile *llf)
{
  if (!llf || !llf->fp)
    return;

  if (llf->user_log_file)
  {
    fseek(llf->fp, 0, SEEK_END);
    llf->high_water_mark = ftell(llf->fp);
  }
  else
  {
    ftruncate(fileno(llf->fp), 0);
    fseek(llf->fp, 0, SEEK_SET);
    llf->high_water_mark = 0;
  }

  ARRAY_FREE(&llf->line_offsets);
  ARRAY_ADD(&llf->line_offsets, llf->high_water_mark);

  //XXX? menu_set_index(NeoMutt->lua_module->console->menu, -1);
}

/**
 * log_disp_lua - Save a log line to the Lua Console - Implements ::log_dispatcher_t - @ingroup logging_api
 *
 * XXX
 */
int log_disp_lua(time_t stamp, const char *file, int line, const char *function,
                 enum LogLevel level, const char *format, ...)
{
  if ((level < LL_PERROR) || (level > LL_NOTIFY))
    return 0;

  char buf[LOG_LINE_MAX_LEN] = { 0 };
  int err = errno;

  va_list ap;
  va_start(ap, format);
  int bytes = vsnprintf(buf, sizeof(buf), format, ap);
  va_end(ap);

  if (bytes < 0)
    return 0;

  bool needs_nl = (bytes == 0) || (buf[bytes - 1] != '\n');

  // Normal logging
  if ((level > 0) && needs_nl)
    MuttLogger(stamp, file, line, function, level, "%s\n", buf);
  else
    MuttLogger(stamp, file, line, function, level, "%s", buf);

  struct LuaLogFile *llf = lua_get_log_file();
  if (!llf || !llf->fp)
    return 0;

  FILE *fp = llf->fp;

  fputs(buf, fp);

  if (level == LL_PERROR)
    bytes += fprintf(fp, ": %s", strerror(err));

  if (needs_nl)
    bytes += fputs("\n", fp);

  fflush(fp);

  long offset = *ARRAY_LAST(&llf->line_offsets);

  // dump_string(buf);
  char *str = buf;
  char *nl = NULL;
  // int count = 0;
  while ((nl = strchr(str, '\n')))
  {
    if (nl[1] == '\0')
      break;

    int len = nl - str + 1;
    // lua_debug(LL_DEBUG1, "Part: >>%.*s<<\n", len - 1, str);
    offset += len;
    str += len;
    ARRAY_ADD(&llf->line_offsets, offset);
    // if (count++ > 10)
    // {
    //   lua_debug(LL_DEBUG1, "abort\n");
    //   break;
    // }
  }

  offset = ftell(llf->fp);
  ARRAY_ADD(&llf->line_offsets, offset);
  // dump_lines(&llf->line_offsets);

  lua_console_update();

  return bytes;
}

/**
 * lua_log_cb_print - Callback for Log::print() - Implements ::lua_callback_t
 */
static int lua_log_cb_print(lua_State *l)
{
  //XXX change this to LL_DEBUG1
  struct Buffer *buf = buf_pool_get();

  int nargs = lua_gettop(l); // Number of arguments passed to print
  for (int i = 1; i <= nargs; ++i)
  {
    // Convert the argument to a string
    // XXX calls __tostring() if possible
    // lua_debug(LL_DEBUG1, "PRINT STACK1: %d\n", lua_gettop(l));
    const char *str = luaL_tolstring(l, i, NULL);
    // lua_debug(LL_DEBUG1, "PRINT STACK2: %d\n", lua_gettop(l));
    if (!str)
    {
      // str = "(nil)";
      str = lua_type_name(lua_type(l, i));
    }

    // Append the string to the buffer
    buf_addstr(buf, str);

    // Add a space between arguments
    if (i < nargs)
      buf_addch(buf, ' ');
  }

  lua_debug(LL_MESSAGE, "%s", buf_string(buf));
  buf_pool_release(&buf);

  return 0; // No values placed on the stack
}

/**
 * lua_log_cb_neolog - Callback for Log::neolog() - Implements ::lua_callback_t
 */
static int lua_log_cb_neolog(lua_State *l)
{
  if (lua_gettop(l) != 2) // We expect 2 arguments
  {
    lua_debug(LL_DEBUG1, "neolog() expects 2 arguments (LEVEL,MESSAGE)\n");
    return 0;
  }

  // If this conversion fails, it returns 0 (LL_MESSAGE)
  // XXX more error checking with lua_tointegerx()?
  // XXX then range check level?
  const int level = lua_tointeger(l, 1);
  const char *str = lua_tostring(l, 2);

  lua_debug(level, "%s", str);

  return 0; // No values placed on the stack
}

/**
 * lua_log_init - Initialise Logging functions and constants
 */
void lua_log_init(lua_State *l)
{
  lua_register(l, "print", lua_log_cb_print);
  lua_register(l, "neolog", lua_log_cb_neolog);

  luaL_dostring(l, "function log(level, fmt, ...) neolog(level, string.format(fmt, ...)) end");

  // clang-format off
  lua_pushinteger(l, LL_DEBUG1);  lua_setglobal(l, "LOG_DEBUG");
  lua_pushinteger(l, LL_MESSAGE); lua_setglobal(l, "LOG_MSG");
  lua_pushinteger(l, LL_WARNING); lua_setglobal(l, "LOG_WARN");
  lua_pushinteger(l, LL_ERROR);   lua_setglobal(l, "LOG_ERROR");
  // clang-format on
  // XXX create log_debug(), log_warning(), log_message(), log_error() instead?
}
