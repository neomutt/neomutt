/**
 * @file
 * Parse the 'set' command
 *
 * @authors
 * Copyright (C) 2023 Richard Russon <rich@flatcap.org>
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
 * @page parse_set Parse the 'set' command
 *
 * Parse the 'set' command
 */

#include "config.h"
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "mutt.h"
#include "set.h"
#include "commands.h"
#include "extract.h"
#include "globals.h"
#include "muttlib.h"

/**
 * command_set_expand_value - Expand special characters
 * @param         type  Type of the value, see note below
 * @param[in,out] value Buffer containing the value, will also contain the final result
 *
 * @pre value is not NULL
 *
 * Expand any special characters in paths, mailboxes or commands.
 * e.g. `~` ($HOME), `+` ($folder)
 *
 * The type influences which expansions are done.
 *
 * @note The type must be the full HashElem.type, not the sanitized DTYPE(HashElem.type)
 * @note The value is expanded in-place
 */
static void command_set_expand_value(uint32_t type, struct Buffer *value)
{
  assert(value);
  if (DTYPE(type) == DT_PATH)
  {
    if (type & (DT_PATH_DIR | DT_PATH_FILE))
      buf_expand_path(value);
    else
      mutt_path_tilde(value->data, value->dsize, HomeDir);
  }
  else if (IS_MAILBOX(type))
  {
    buf_expand_path(value);
  }
  else if (IS_COMMAND(type))
  {
    struct Buffer scratch = buf_make(1024);
    buf_copy(&scratch, value);

    if (!mutt_str_equal(value->data, "builtin"))
    {
      buf_expand_path(&scratch);
    }
    buf_reset(value);
    buf_addstr(value, buf_string(&scratch));
    buf_dealloc(&scratch);
  }
}

/**
 * command_set_set - Set a variable to the given value
 * @param[in]  name  Name of the config; must not be NULL
 * @param[in]  value Value the config should be set to
 * @param[out] err   Buffer for error messages
 * @retval #CommandResult Result e.g. #MUTT_CMD_SUCCESS
 *
 * @pre name is not NULL
 * @pre value is not NULL
 *
 * This implements "set foo = bar" command where "bar" is present.
 */
static enum CommandResult command_set_set(struct Buffer *name,
                                          struct Buffer *value, struct Buffer *err)
{
  assert(name);
  assert(value);
  struct HashElem *he = cs_subset_lookup(NeoMutt->sub, name->data);
  if (!he)
  {
    // In case it is a my_var, we have to create it
    if (mutt_str_startswith(name->data, "my_"))
    {
      struct ConfigDef my_cdef = { 0 };
      my_cdef.name = name->data;
      my_cdef.type = DT_MYVAR;
      he = cs_create_variable(NeoMutt->sub->cs, &my_cdef, err);
      if (!he)
        return MUTT_CMD_ERROR;
    }
    else
    {
      buf_printf(err, _("%s: unknown variable"), name->data);
      return MUTT_CMD_ERROR;
    }
  }

  int rc = CSR_ERR_CODE;

  if (DTYPE(he->type) == DT_MYVAR)
  {
    // my variables do not expand their value
    rc = cs_subset_he_string_set(NeoMutt->sub, he, value->data, err);
  }
  else
  {
    command_set_expand_value(he->type, value);
    rc = cs_subset_he_string_set(NeoMutt->sub, he, value->data, err);
  }
  if (CSR_RESULT(rc) != CSR_SUCCESS)
    return MUTT_CMD_ERROR;

  return MUTT_CMD_SUCCESS;
}

/**
 * command_set_increment - Increment a variable by a value
 * @param[in]  name  Name of the config; must not be NULL
 * @param[in]  value Value the config should be incremented by
 * @param[out] err   Buffer for error messages
 * @retval #CommandResult Result e.g. #MUTT_CMD_SUCCESS
 *
 * @pre name is not NULL
 * @pre value is not NULL
 *
 * This implements "set foo += bar" command where "bar" is present.
 */
static enum CommandResult command_set_increment(struct Buffer *name,
                                                struct Buffer *value, struct Buffer *err)
{
  assert(name);
  assert(value);
  struct HashElem *he = cs_subset_lookup(NeoMutt->sub, name->data);
  if (!he)
  {
    // In case it is a my_var, we have to create it
    if (mutt_str_startswith(name->data, "my_"))
    {
      struct ConfigDef my_cdef = { 0 };
      my_cdef.name = name->data;
      my_cdef.type = DT_MYVAR;
      he = cs_create_variable(NeoMutt->sub->cs, &my_cdef, err);
      if (!he)
        return MUTT_CMD_ERROR;
    }
    else
    {
      buf_printf(err, _("%s: unknown variable"), name->data);
      return MUTT_CMD_ERROR;
    }
  }

  int rc = CSR_ERR_CODE;

  if (DTYPE(he->type) == DT_MYVAR)
  {
    // my variables do not expand their value
    rc = cs_subset_he_string_plus_equals(NeoMutt->sub, he, value->data, err);
  }
  else
  {
    command_set_expand_value(he->type, value);
    rc = cs_subset_he_string_plus_equals(NeoMutt->sub, he, value->data, err);
  }
  if (CSR_RESULT(rc) != CSR_SUCCESS)
    return MUTT_CMD_ERROR;

  return MUTT_CMD_SUCCESS;
}

/**
 * command_set_decrement - Decrement a variable by a value
 * @param[in]  name  Name of the config; must not be NULL
 * @param[in]  value Value the config should be decremented by
 * @param[out] err   Buffer for error messages
 * @retval #CommandResult Result e.g. #MUTT_CMD_SUCCESS
 *
 * @pre name is not NULL
 * @pre value is not NULL
 *
 * This implements "set foo -= bar" command where "bar" is present.
 */
static enum CommandResult command_set_decrement(struct Buffer *name,
                                                struct Buffer *value, struct Buffer *err)
{
  assert(name);
  assert(value);
  struct HashElem *he = cs_subset_lookup(NeoMutt->sub, name->data);
  if (!he)
  {
    buf_printf(err, _("%s: unknown variable"), name->data);
    return MUTT_CMD_ERROR;
  }

  command_set_expand_value(he->type, value);
  int rc = cs_subset_he_string_minus_equals(NeoMutt->sub, he, value->data, err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
    return MUTT_CMD_ERROR;

  return MUTT_CMD_SUCCESS;
}

/**
 * command_set_unset - Unset a variable
 * @param[in]  name Name of the config variable to be unset
 * @param[out] err  Buffer for error messages
 * @retval #CommandResult Result e.g. #MUTT_CMD_SUCCESS
 *
 * @pre name is not NULL
 *
 * This implements "unset foo"
 */
static enum CommandResult command_set_unset(struct Buffer *name, struct Buffer *err)
{
  assert(name);
  struct HashElem *he = cs_subset_lookup(NeoMutt->sub, name->data);
  if (!he)
  {
    buf_printf(err, _("%s: unknown variable"), name->data);
    return MUTT_CMD_ERROR;
  }

  int rc = CSR_ERR_CODE;
  if (DTYPE(he->type) == DT_MYVAR)
  {
    rc = cs_subset_he_delete(NeoMutt->sub, he, err);
  }
  else if ((DTYPE(he->type) == DT_BOOL) || (DTYPE(he->type) == DT_QUAD))
  {
    rc = cs_subset_he_native_set(NeoMutt->sub, he, false, err);
  }
  else
  {
    rc = cs_subset_he_string_set(NeoMutt->sub, he, NULL, err);
  }
  if (CSR_RESULT(rc) != CSR_SUCCESS)
    return MUTT_CMD_ERROR;

  return MUTT_CMD_SUCCESS;
}

/**
 * command_set_reset - Reset a variable
 * @param[in]  name Name of the config variable to be reset
 * @param[out] err  Buffer for error messages
 * @retval #CommandResult Result e.g. #MUTT_CMD_SUCCESS
 *
 * @pre name is not NULL
 *
 * This implements "reset foo" (foo being any config variable) and "reset all".
 */
static enum CommandResult command_set_reset(struct Buffer *name, struct Buffer *err)
{
  assert(name);
  // Handle special "reset all" syntax
  if (mutt_str_equal(name->data, "all"))
  {
    struct HashElem **he_list = get_elem_list(NeoMutt->sub->cs);
    if (!he_list)
      return MUTT_CMD_ERROR;

    for (size_t i = 0; he_list[i]; i++)
    {
      if (DTYPE(he_list[i]->type) == DT_MYVAR)
        cs_subset_he_delete(NeoMutt->sub, he_list[i], err);
      else
        cs_subset_he_reset(NeoMutt->sub, he_list[i], NULL);
    }

    FREE(&he_list);
    return MUTT_CMD_SUCCESS;
  }

  struct HashElem *he = cs_subset_lookup(NeoMutt->sub, name->data);
  if (!he)
  {
    buf_printf(err, _("%s: unknown variable"), name->data);
    return MUTT_CMD_ERROR;
  }

  int rc = CSR_ERR_CODE;
  if (DTYPE(he->type) == DT_MYVAR)
  {
    rc = cs_subset_he_delete(NeoMutt->sub, he, err);
  }
  else
  {
    rc = cs_subset_he_reset(NeoMutt->sub, he, err);
  }
  if (CSR_RESULT(rc) != CSR_SUCCESS)
    return MUTT_CMD_ERROR;

  return MUTT_CMD_SUCCESS;
}

/**
 * command_set_toggle - Toggle a boolean or quad variable
 * @param[in]  name Name of the config variable to be toggled
 * @param[out] err  Buffer for error messages
 * @retval #CommandResult Result e.g. #MUTT_CMD_SUCCESS
 *
 * @pre name is not NULL
 *
 * This implements "toggle foo".
 */
static enum CommandResult command_set_toggle(struct Buffer *name, struct Buffer *err)
{
  assert(name);
  struct HashElem *he = cs_subset_lookup(NeoMutt->sub, name->data);
  if (!he)
  {
    buf_printf(err, _("%s: unknown variable"), name->data);
    return MUTT_CMD_ERROR;
  }

  if (DTYPE(he->type) == DT_BOOL)
  {
    bool_he_toggle(NeoMutt->sub, he, err);
  }
  else if (DTYPE(he->type) == DT_QUAD)
  {
    quad_he_toggle(NeoMutt->sub, he, err);
  }
  else
  {
    buf_printf(err, _("Command '%s' can only be used with bool/quad variables"), "toggle");
    return MUTT_CMD_ERROR;
  }
  return MUTT_CMD_SUCCESS;
}

/**
 * command_set_query - Query a variable
 * @param[in]  name Name of the config variable to queried
 * @param[out] err  Buffer where the pretty printed result will be written to.  On failure contains the error message.
 * @retval #CommandResult Result e.g. #MUTT_CMD_SUCCESS
 *
 * @pre name is not NULL
 *
 * This implements "set foo?".  The buffer err will contain something like "set foo = bar".
 */
static enum CommandResult command_set_query(struct Buffer *name, struct Buffer *err)
{
  assert(name);
  // In the interactive case (outside of the initial parsing of neomuttrc) we
  // support additional syntax: "set" (no arguments) and "set all".
  // If not in interactive mode, we recognise them but do nothing.

  // Handle "set" (no arguments), i.e. show list of changed variables.
  if (buf_is_empty(name))
  {
    if (StartupComplete)
      return set_dump(CS_DUMP_ONLY_CHANGED, err);
    else
      return MUTT_CMD_SUCCESS;
  }
  // Handle special "set all" syntax
  if (mutt_str_equal(name->data, "all"))
  {
    if (StartupComplete)
      return set_dump(CS_DUMP_NO_FLAGS, err);
    else
      return MUTT_CMD_SUCCESS;
  }

  struct HashElem *he = cs_subset_lookup(NeoMutt->sub, name->data);
  if (!he)
  {
    buf_printf(err, _("%s: unknown variable"), name->data);
    return MUTT_CMD_ERROR;
  }

  buf_addstr(err, name->data);
  buf_addch(err, '=');
  struct Buffer *value = buf_pool_get();
  int rc = cs_subset_he_string_get(NeoMutt->sub, he, value);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    buf_reset(err);
    buf_addstr(err, value->data);
    buf_pool_release(&value);
    return MUTT_CMD_ERROR;
  }
  if (DTYPE(he->type) == DT_PATH)
    mutt_pretty_mailbox(value->data, value->dsize);
  pretty_var(value->data, err);
  buf_pool_release(&value);

  return MUTT_CMD_SUCCESS;
}

/**
 * parse_set - Parse the 'set' family of commands - Implements Command::parse() - @ingroup command_parse
 *
 * This is used by 'reset', 'set', 'toggle' and 'unset'.
 */
enum CommandResult parse_set(struct Buffer *buf, struct Buffer *s,
                             intptr_t data, struct Buffer *err)
{
  /* The order must match `enum MuttSetCommand` */
  static const char *set_commands[] = { "set", "toggle", "unset", "reset" };

  if (!buf || !s)
    return MUTT_CMD_ERROR;

  do
  {
    bool prefix = false;
    bool query = false;
    bool inv = (data == MUTT_SET_INV);
    bool reset = (data == MUTT_SET_RESET);
    bool unset = (data == MUTT_SET_UNSET);

    if (*s->dptr == '?')
    {
      prefix = true;
      query = true;
      s->dptr++;
    }
    else if (mutt_str_startswith(s->dptr, "no"))
    {
      prefix = true;
      unset = !unset;
      s->dptr += 2;
    }
    else if (mutt_str_startswith(s->dptr, "inv"))
    {
      prefix = true;
      inv = !inv;
      s->dptr += 3;
    }
    else if (*s->dptr == '&')
    {
      prefix = true;
      reset = true;
      s->dptr++;
    }

    if (prefix && (data != MUTT_SET_SET))
    {
      buf_printf(err, _("Can't use 'inv', 'no', '&' or '?' with the '%s' command"),
                 set_commands[data]);
      return MUTT_CMD_WARNING;
    }

    // get the variable name.  Note that buf might be empty if no additional
    // argument was given.
    int ret = parse_extract_token(buf, s, TOKEN_EQUAL | TOKEN_QUESTION | TOKEN_PLUS | TOKEN_MINUS);
    if (ret == -1)
      return MUTT_CMD_ERROR;

    bool bool_or_quad = false;
    bool equals = false;
    bool increment = false;
    bool decrement = false;

    struct HashElem *he = cs_subset_lookup(NeoMutt->sub, buf->data);
    if (he)
    {
      // Use the correct name if a synonym is used
      buf_strcpy(buf, he->key.strkey);
      bool_or_quad = ((DTYPE(he->type) == DT_BOOL) || (DTYPE(he->type) == DT_QUAD));
    }

    if (*s->dptr == '?')
    {
      if (prefix)
      {
        buf_printf(err, _("Can't use a prefix when querying a variable"));
        return MUTT_CMD_WARNING;
      }

      if (reset || unset || inv)
      {
        buf_printf(err, _("Can't query a variable with the '%s' command"),
                   set_commands[data]);
        return MUTT_CMD_WARNING;
      }

      query = true;
      s->dptr++;
    }
    else if ((*s->dptr == '+') || (*s->dptr == '-'))
    {
      if (prefix)
      {
        buf_printf(err, _("Can't use prefix when incrementing or decrementing a variable"));
        return MUTT_CMD_WARNING;
      }

      if (reset || unset || inv)
      {
        buf_printf(err, _("Can't set a variable with the '%s' command"),
                   set_commands[data]);
        return MUTT_CMD_WARNING;
      }
      if (*s->dptr == '+')
        increment = true;
      else
        decrement = true;

      s->dptr++;
      if (*s->dptr == '=')
      {
        equals = true;
        s->dptr++;
      }
      else
      {
        buf_printf(err, _("'+' and '-' must be followed by '='"), set_commands[data]);
        return MUTT_CMD_WARNING;
      }
    }
    else if (*s->dptr == '=')
    {
      if (prefix)
      {
        buf_printf(err, _("Can't use prefix when setting a variable"));
        return MUTT_CMD_WARNING;
      }

      if (reset || unset || inv)
      {
        buf_printf(err, _("Can't set a variable with the '%s' command"),
                   set_commands[data]);
        return MUTT_CMD_WARNING;
      }

      equals = true;
      s->dptr++;
    }

    if (!bool_or_quad && (inv || (unset && prefix)))
    {
      if (data == MUTT_SET_SET)
      {
        buf_printf(err, _("Prefixes 'no' and 'inv' may only be used with bool/quad variables"));
      }
      else
      {
        buf_printf(err, _("Command '%s' can only be used with bool/quad variables"),
                   set_commands[data]);
      }
      return MUTT_CMD_WARNING;
    }

    // sanity checks for the above
    // Each of inv, unset reset, query, equals implies that the others are not set.
    // If none of them are set, then we are dealing with a "set foo" command.
    // clang-format off
    assert(!inv    || !(       unset || reset || query || equals          ));
    assert(!unset  || !(inv ||          reset || query || equals          ));
    assert(!reset  || !(inv || unset ||          query || equals          ));
    assert(!query  || !(inv || unset || reset ||          equals          ));
    assert(!equals || !(inv || unset || reset || query ||           prefix));
    // clang-format on
    assert(!(increment && decrement)); // only one of increment or decrement is set
    assert(!(increment || decrement) || equals); // increment/decrement implies equals
    assert(!inv || bool_or_quad); // inv (aka toggle) implies bool or quad

    enum CommandResult rc = MUTT_CMD_ERROR;
    if (query)
    {
      rc = command_set_query(buf, err);
      return rc; // We can only do one query even if multiple config names are given
    }
    else if (reset)
    {
      rc = command_set_reset(buf, err);
    }
    else if (unset)
    {
      rc = command_set_unset(buf, err);
    }
    else if (inv)
    {
      rc = command_set_toggle(buf, err);
    }
    else if (equals)
    {
      // These three cases all need a value, since 'increment'/'decrement'
      // implies 'equals', we can group them in this single case guarded by
      // 'equals'.
      struct Buffer *value = buf_pool_get();
      parse_extract_token(value, s, TOKEN_BACKTICK_VARS);
      if (increment)
        rc = command_set_increment(buf, value, err);
      else if (decrement)
        rc = command_set_decrement(buf, value, err);
      else
        rc = command_set_set(buf, value, err);
      buf_pool_release(&value);
    }
    else
    {
      // This is the "set foo" case which has different meanings depending on
      // the type of the config variable
      if (bool_or_quad)
      {
        struct Buffer *yes = buf_pool_get();
        buf_addstr(yes, "yes");
        rc = command_set_set(buf, yes, err);
        buf_pool_release(&yes);
      }
      else
      {
        rc = command_set_query(buf, err);
        return rc; // We can only do one query even if multiple config names are given
      }
    }
    // Short circuit (i.e. skipping further config variable names) if the action on
    // the current variable failed.
    if (rc != MUTT_CMD_SUCCESS)
      return rc;
  } while (MoreArgs(s));

  return MUTT_CMD_SUCCESS;
}
