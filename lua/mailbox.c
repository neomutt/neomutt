/**
 * @file
 * Lua Mailbox Wrapper
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
 * @page lua_mailbox Lua Mailbox Wrapper
 *
 * Lua Mailbox Wrapper
 */

#include "config.h"
#include <lauxlib.h>
#include <lua.h>
#include "email/lib.h"
#include "core/lib.h"
#include "mailbox.h"
#include "helpers.h"
#include "iterator.h"
#include "logging.h"
#include "mview.h"

bool pattern_func(struct MailboxView *mv, char *prompt, struct EmailArray *ea);

/**
 * lua_mailbox_cb_tostring - Turn a Mailbox into a string - Implements ::lua_callback_t
 */
static int lua_mailbox_cb_tostring(lua_State *l)
{
  struct Mailbox *m = *(struct Mailbox **) luaL_checkudata(l, 1, "Mailbox");
  if (!m)
    return 0;

  struct Buffer *buf = buf_pool_get();
  buf_printf(buf, "M:%lX", (long) m);
  lua_pushstring(l, buf_string(buf));
  buf_pool_release(&buf);

  return 1;
}

/**
 * lua_mailbox_cb_index - Callback for Lua Metamethod Index::__index() - Implements ::lua_callback_t
 */
static int lua_mailbox_cb_index(lua_State *l)
{
  lua_debug(LL_DEBUG1, "lua_mailbox_cb_index\n");
  bool found = lua_index_lookup(l, "Mailbox");
  if (found)
    return 1;

  return 0;
}

/**
 * lua_mailbox_cb_emails_iter - Iterator for the Emails Array
 */
static int lua_mailbox_cb_emails_iter(lua_State *l)
{
  struct Mailbox *m = lua_touserdata(l, lua_upvalueindex(1));

  int i = lua_tointeger(l, lua_upvalueindex(2));
  if (i < m->msg_count)
  {
    lua_pushinteger(l, i + 1);
    lua_replace(l, lua_upvalueindex(2));
    LUA_PUSH_OBJECT(l, Email, m->emails[i]);
    return 1;
  }

  return 0;
}

/**
 * lua_mailbox_cb_emails - Get the Emails in the Mailbox
 */
static int lua_mailbox_cb_emails(lua_State *l)
{
  struct Mailbox *m = *(struct Mailbox **) luaL_checkudata(l, 1, "Mailbox");
  lua_pushlightuserdata(l, m);
  lua_pushinteger(l, 0);
  lua_pushcclosure(l, lua_mailbox_cb_emails_iter, 2);

  return 1;
}

/**
 * lua_mailbox_cb_num_emails - Count the number of Emails
 */
static int lua_mailbox_cb_num_emails(lua_State *l)
{
  struct Mailbox *m = *(struct Mailbox **) luaL_checkudata(l, 1, "Mailbox");
  lua_pushinteger(l, m->msg_count);

  return 1;
}


/**
 * lua_mailboxview_cb_tostring - Turn a MailboxView into a string - Implements ::lua_callback_t
 */
static int lua_mailboxview_cb_tostring(lua_State *l)
{
  struct MailboxView *m = *(struct MailboxView **) luaL_checkudata(l, 1, "MailboxView");
  if (!m)
    return 0;

  struct Buffer *buf = buf_pool_get();
  buf_printf(buf, "MV:%lX", (long) m);
  lua_pushstring(l, buf_string(buf));
  buf_pool_release(&buf);

  return 1;
}

/**
 * lua_mailboxview_cb_index - Callback for Lua Metamethod Index::__index() - Implements ::lua_callback_t
 */
static int lua_mailboxview_cb_index(lua_State *l)
{
  lua_debug(LL_DEBUG1, "lua_mailboxview_cb_index\n");
  bool found = lua_index_lookup(l, "MailboxView");
  if (found)
    return 1;

  return 0;
}

/**
 * lua_mailboxview_cb_get_mailbox - XXX - Implements ::lua_callback_t
 */
static int lua_mailboxview_cb_get_mailbox(lua_State *l)
{
  lua_debug(LL_DEBUG1, "get_mailbox\n");

  struct MailboxView *mv = *(struct MailboxView **) luaL_checkudata(l, 1, "MailboxView");
  lua_debug(LL_DEBUG1, "mailboxview = %p\n", mv);

  struct Mailbox **pptr = lua_newuserdata(l, sizeof(struct Mailbox *));
  *pptr = mv->mailbox;
  luaL_getmetatable(l, "Mailbox");
  lua_setmetatable(l, -2);

  return 1;
}

/**
 * lua_mailboxview_cb_get_emails_by_pattern - XXX - Implements ::lua_callback_t
 */
static int lua_mailboxview_cb_get_emails_by_pattern(lua_State *l)
{
  lua_debug(LL_DEBUG1, "get_emails_by_pattern\n");
  struct MailboxView *mv = *(struct MailboxView **) luaL_checkudata(l, 1, "MailboxView");
  lua_debug(LL_DEBUG1, "mailboxview = %p\n", mv);

  const char *pat = lua_tostring(l, 2);
  lua_debug(LL_DEBUG1, "pattern: %s\n", pat);

  struct EmailArray *ea = emailarray_new(l);

  mutt_str_replace(&mv->pattern, pat);
  if (!pattern_func(mv, NULL, ea))
  {
    lua_debug(LL_DEBUG1, "Pattern failed\n");
    ARRAY_FREE(ea);
    // The object is owned by Lua and will be garbage-collected
    return 0;
  }

  return 1;
}


/**
 * MailboxMethods - Mailbox Class Methods
 */
static const struct luaL_Reg MailboxMethods[] = {
  // clang-format off
  { "__index",    lua_mailbox_cb_index      },
  { "__tostring", lua_mailbox_cb_tostring   },
  { "emails",     lua_mailbox_cb_emails     },
  { "num_emails", lua_mailbox_cb_num_emails },
  { NULL, NULL },
  // clang-format on
};

/**
 * MailboxViewMethods - MailboxView Class Methods
 */
static const struct luaL_Reg MailboxViewMethods[] = {
  // clang-format off
  { "__index",               lua_mailboxview_cb_index                 },
  { "__tostring",            lua_mailboxview_cb_tostring              },
  { "get_mailbox",           lua_mailboxview_cb_get_mailbox           },
  { "get_emails_by_pattern", lua_mailboxview_cb_get_emails_by_pattern },
  { NULL, NULL },
  // clang-format on
};

/**
 * lua_mailbox_class - Declare the Mailbox Class
 */
void lua_mailbox_class(lua_State *l)
{
  luaL_newmetatable(l, "Mailbox");
  luaL_setfuncs(l, MailboxMethods, 0);
  lua_pop(l, 1);

  luaL_newmetatable(l, "MailboxView");
  luaL_setfuncs(l, MailboxViewMethods, 0);
  lua_pop(l, 1);
}
