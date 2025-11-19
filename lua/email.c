/**
 * @file
 * Lua Email Wrapper
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
 * @page lua_email Lua Email Wrapper
 *
 * Lua Email Wrapper
 */

#include "config.h"
#include <lauxlib.h>
#include <lua.h>
#include "email/lib.h"
#include "gui/lib.h"
#include "email.h"
#include "helpers.h"
#include "iterator.h"
#include "logging.h"

/**
 * @defgroup email_getter_api Email Member Getter API
 *
 * Prototype for an Email Member Getter function
 *
 * @param l Lua State
 * @param e Email
 * @retval num Number of results pushed onto Lua Stack
 */
typedef int (*get_field_t)(lua_State *l, struct Email *e);

/**
 * struct EmailMemberGetter - Function to extract Email member values
 */
struct EmailMemberGetter
{
  const char *name;     ///< Member name in Email
  get_field_t function; ///< Function to get value
};

/**
 * get_email_attach_total - Get Email.attach_total - Implements ::get_field_t
 */
int get_email_attach_total(lua_State *l, struct Email *e)
{
  if (!l || !e)
    return 0;

  lua_pushinteger(l, e->attach_total);
  return 1;
}

/**
 * get_email_date_sent - Get Email.date_sent - Implements ::get_field_t
 */
int get_email_date_sent(lua_State *l, struct Email *e)
{
  if (!l || !e)
    return 0;

  lua_pushinteger(l, e->date_sent);
  return 1;
}

/**
 * get_email_flagged - Get Email.flagged - Implements ::get_field_t
 */
int get_email_flagged(lua_State *l, struct Email *e)
{
  if (!l || !e)
    return 0;

  lua_pushinteger(l, e->flagged);
  return 1;
}

/**
 * get_email_old - Get Email.old - Implements ::get_field_t
 */
int get_email_old(lua_State *l, struct Email *e)
{
  if (!l || !e)
    return 0;

  lua_pushinteger(l, e->old);
  return 1;
}

/**
 * get_email_read - Get Email.read - Implements ::get_field_t
 */
int get_email_read(lua_State *l, struct Email *e)
{
  if (!l || !e)
    return 0;

  lua_pushinteger(l, e->read);
  return 1;
}

/**
 * get_email_replied - Get Email.replied - Implements ::get_field_t
 */
int get_email_replied(lua_State *l, struct Email *e)
{
  if (!l || !e)
    return 0;

  lua_pushinteger(l, e->replied);
  return 1;
}

/**
 * get_email_tags - Get Email.tags - Implements ::get_field_t
 */
int get_email_tags(lua_State *l, struct Email *e)
{
  if (!l || !e)
    return 0;

  struct Buffer *buf = buf_pool_get();
  driver_tags_get_transformed(&e->tags, buf);
  lua_pushstring(l, buf_string(buf));
  buf_pool_release(&buf);

  return 1;
}

/**
 * get_env_bcc - Get Envelope.bcc - Implements ::get_field_t
 */
int get_env_bcc(lua_State *l, struct Email *e)
{
  if (!l || !e || !e->env)
    return 0;

  struct Buffer *buf = buf_pool_get();
  mutt_addrlist_write(&e->env->bcc, buf, true);
  lua_pushstring(l, buf_string(buf));
  buf_pool_release(&buf);

  return 1;
}

/**
 * get_env_cc - Get Envelope.cc - Implements ::get_field_t
 */
int get_env_cc(lua_State *l, struct Email *e)
{
  if (!l || !e || !e->env)
    return 0;

  struct Buffer *buf = buf_pool_get();
  mutt_addrlist_write(&e->env->cc, buf, true);
  lua_pushstring(l, buf_string(buf));
  buf_pool_release(&buf);

  return 1;
}

/**
 * get_env_from - Get Envelope.from - Implements ::get_field_t
 */
int get_env_from(lua_State *l, struct Email *e)
{
  if (!l || !e || !e->env)
    return 0;

  struct Buffer *buf = buf_pool_get();
  mutt_addrlist_write(&e->env->from, buf, true);
  lua_pushstring(l, buf_string(buf));
  buf_pool_release(&buf);

  return 1;
}

/**
 * get_env_message_id - Get Envelope.message_id - Implements ::get_field_t
 */
int get_env_message_id(lua_State *l, struct Email *e)
{
  if (!l || !e || !e->env)
    return 0;

  lua_pushstring(l, NONULL(e->env->message_id));
  return 1;
}

/**
 * get_env_reply_to - Get Envelope.reply_to - Implements ::get_field_t
 */
int get_env_reply_to(lua_State *l, struct Email *e)
{
  if (!l || !e || !e->env)
    return 0;

  struct Buffer *buf = buf_pool_get();
  mutt_addrlist_write(&e->env->reply_to, buf, true);
  lua_pushstring(l, buf_string(buf));
  buf_pool_release(&buf);

  return 1;
}

/**
 * get_env_sender - Get Envelope.sender - Implements ::get_field_t
 */
int get_env_sender(lua_State *l, struct Email *e)
{
  if (!l || !e || !e->env)
    return 0;

  struct Buffer *buf = buf_pool_get();
  mutt_addrlist_write(&e->env->sender, buf, true);
  lua_pushstring(l, buf_string(buf));
  buf_pool_release(&buf);

  return 1;
}

/**
 * get_env_subject - Get Envelope.subject - Implements ::get_field_t
 */
int get_env_subject(lua_State *l, struct Email *e)
{
  if (!l || !e || !e->env)
    return 0;

  lua_pushstring(l, NONULL(e->env->subject));
  return 1;
}

/**
 * get_env_to - Get Envelope.to - Implements ::get_field_t
 */
int get_env_to(lua_State *l, struct Email *e)
{
  if (!l || !e || !e->env)
    return 0;

  struct Buffer *buf = buf_pool_get();
  mutt_addrlist_write(&e->env->to, buf, true);
  lua_pushstring(l, buf_string(buf));
  buf_pool_release(&buf);

  return 1;
}

/**
 * get_env_x_label - Get Envelope.x_label - Implements ::get_field_t
 */
int get_env_x_label(lua_State *l, struct Email *e)
{
  if (!l || !e || !e->env)
    return 0;

  lua_pushstring(l, NONULL(e->env->x_label));
  return 1;
}

/**
 * EmailMembers - Define Functions to extract member data
 */
struct EmailMemberGetter EmailMembers[] = {
  // clang-format off
  // Email
  { "attach_total",  get_email_attach_total },
  { "date_sent",     get_email_date_sent },
  { "flagged",       get_email_flagged },
  { "old",           get_email_old },
  { "read",          get_email_read },
  { "replied",       get_email_replied },
  { "tags",          get_email_tags },
  // Envelope
  { "bcc",           get_env_bcc },
  { "cc",            get_env_cc },
  { "from",          get_env_from },
  { "message_id",    get_env_message_id },
  { "reply_to",      get_env_reply_to },
  { "sender",        get_env_sender },
  { "subject",       get_env_subject },
  { "to",            get_env_to },
  { "x_label",       get_env_x_label },
  { NULL, NULL }
  // clang-format on
};

/**
 * lua_email_cb_function - XXX
 */
int lua_email_cb_function(lua_State *l)
{
  // int op = lua_tointeger(l, lua_upvalueindex(1));
  // struct MuttWindow *win = lua_touserdata(l, lua_upvalueindex(2));

  // lua_pushstring(l, opcodes_get_name(op));
  // lua_pushinteger(l, rc);
  return 0;
}

/**
 * lua_email_cb_index - Callback for Lua Metamethod Index::__index() - Implements ::lua_callback_t
 */
static int lua_email_cb_index(lua_State *l)
{
  lua_debug(LL_DEBUG1, "lua_email_cb_index\n");
  bool found = lua_index_lookup(l, "Email");
  if (found)
    return 1;

  struct Email *e = *(struct Email **) luaL_checkudata(l, 1, "Email");
  // lua_debug(LL_DEBUG1, "email = %p\n", e);

  // // store upvalues
  // lua_pushinteger(l, op);
  // lua_pushlightuserdata(l, win);
  // lua_pushcclosure(l, lua_email_cb_function, 2);

  const char *member = luaL_checkstring(l, 2);

  for (int i = 0; EmailMembers[i].name; i++)
  {
    if (mutt_str_equal(EmailMembers[i].name, member))
      return EmailMembers[i].function(l, e);
  }

  return 0;
}

/**
 * lua_email_cb_tostring - Turn an Email into a string - Implements ::lua_callback_t
 */
static int lua_email_cb_tostring(lua_State *l)
{
  struct Email *e = *(struct Email **) luaL_checkudata(l, 1, "Email");
  if (!e || !e->env)
    return 0;

  lua_pushstring(l, NONULL(e->env->subject));
  return 1;
}

/**
 * lua_email_cb_get_functions - XXX - Implements ::lua_callback_t
 */
static int lua_email_cb_get_functions(lua_State *l)
{
  lua_debug(LL_DEBUG1, "get_functions\n");

  lua_newtable(l);

  lua_pushstring(l, "set_tag");
  lua_pushinteger(l, OP_TAG);
  lua_settable(l, -3);

  // lua_debug(LL_DEBUG1, "get-functions() -> %d\n", count);

  return 1;
}

/**
 * lua_email_cb_get_subject - XXX - Implements ::lua_callback_t
 */
static int lua_email_cb_get_subject(lua_State *l)
{
  lua_debug(LL_DEBUG1, "get_subject\n");

  struct Email *e = *(struct Email **) luaL_checkudata(l, 1, "Email");
  lua_debug(LL_DEBUG1, "email = %p\n", e);

  lua_pushstring(l, e->env->subject);

  return 1;
}

/**
 * lua_email_cb_set_subject - XXX - Implements ::lua_callback_t
 */
static int lua_email_cb_set_subject(lua_State *l)
{
  lua_debug(LL_DEBUG1, "set_subect\n");

  struct Email *e = *(struct Email **) luaL_checkudata(l, 1, "Email");
  lua_debug(LL_DEBUG1, "email = %p\n", e);

  const char *subj = luaL_checkstring(l, 2);
  mutt_str_replace(&e->env->disp_subj, subj);

  return 0;
}

/**
 * lua_email_cb_set_expando - XXX - Implements ::lua_callback_t
 */
static int lua_email_cb_set_expando(lua_State *l)
{
  lua_debug(LL_DEBUG1, "set_expando\n");

  struct Email *e = *(struct Email **) luaL_checkudata(l, 1, "Email");
  lua_debug(LL_DEBUG1, "email = %p\n", e);

  int num = luaL_checkinteger(l, 2);
  // const char *str = luaL_checkstring(l, 3);
  const char *str = lua_tolstring(l, 3, NULL); // This could fail if value can't be coerced to a string

  if ((num < 1) || (num > 3)) //XXX error msg
    return 0;

  mutt_str_replace(&e->lua_custom[num - 1], str);

  return 0;
}

/**
 * EmailMethods - Email Class Methods
 */
static const struct luaL_Reg EmailMethods[] = {
  // clang-format off
  { "__index",       lua_email_cb_index         },
  { "__tostring",    lua_email_cb_tostring      },
  { "get_functions", lua_email_cb_get_functions },
  { "get_subject",   lua_email_cb_get_subject   },
  { "set_expando",   lua_email_cb_set_expando   },
  { "set_subject",   lua_email_cb_set_subject   },
  { NULL, NULL },
  // clang-format on
};

/**
 * lua_email_class - Declare the Email Class
 */
void lua_email_class(lua_State *l)
{
  luaL_newmetatable(l, "Email");
  luaL_setfuncs(l, EmailMethods, 0);
  lua_pop(l, 1);
}
