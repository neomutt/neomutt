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
#include "mutt_globals.h"
#include "muttlib.h"
#include "myvar.h"

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

  if (StartupComplete)
  {
    if (set_dump(buf, s, data, err) == MUTT_CMD_SUCCESS)
      return MUTT_CMD_SUCCESS;
    if (!mutt_buffer_is_empty(err))
      return MUTT_CMD_ERROR;
  }

  int rc = 0;

  while (MoreArgs(s))
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
      mutt_buffer_printf(err, _("Can't use 'inv', 'no', '&' or '?' with the '%s' command"),
                         set_commands[data]);
      return MUTT_CMD_WARNING;
    }

    /* get the variable name */
    parse_extract_token(buf, s, TOKEN_EQUAL | TOKEN_QUESTION | TOKEN_PLUS | TOKEN_MINUS);

    bool bq = false;
    bool equals = false;
    bool increment = false;
    bool decrement = false;

    struct HashElem *he = NULL;
    bool my = mutt_str_startswith(buf->data, "my_");
    if (!my)
    {
      he = cs_subset_lookup(NeoMutt->sub, buf->data);
      if (!he)
      {
        if (reset && mutt_str_equal(buf->data, "all"))
        {
          struct HashElem **he_list = get_elem_list(NeoMutt->sub->cs);
          if (!he_list)
            return MUTT_CMD_ERROR;

          for (size_t i = 0; he_list[i]; i++)
            cs_subset_he_reset(NeoMutt->sub, he_list[i], NULL);

          FREE(&he_list);
          break;
        }
        else
        {
          mutt_buffer_printf(err, _("%s: unknown variable"), buf->data);
          return MUTT_CMD_ERROR;
        }
      }

      // Use the correct name if a synonym is used
      mutt_buffer_strcpy(buf, he->key.strkey);

      bq = ((DTYPE(he->type) == DT_BOOL) || (DTYPE(he->type) == DT_QUAD));
    }

    if (*s->dptr == '?')
    {
      if (prefix)
      {
        mutt_buffer_printf(err, _("Can't use a prefix when querying a variable"));
        return MUTT_CMD_WARNING;
      }

      if (reset || unset || inv)
      {
        mutt_buffer_printf(err, _("Can't query a variable with the '%s' command"),
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
        mutt_buffer_printf(err, _("Can't use prefix when incrementing or decrementing a variable"));
        return MUTT_CMD_WARNING;
      }

      if (reset || unset || inv)
      {
        mutt_buffer_printf(err, _("Can't set a variable with the '%s' command"),
                           set_commands[data]);
        return MUTT_CMD_WARNING;
      }
      if (*s->dptr == '+')
        increment = true;
      else
        decrement = true;

      if (my && decrement)
      {
        mutt_buffer_printf(err, _("Can't decrement a my_ variable"), set_commands[data]);
        return MUTT_CMD_WARNING;
      }
      s->dptr++;
      if (*s->dptr == '=')
      {
        equals = true;
        s->dptr++;
      }
    }
    else if (*s->dptr == '=')
    {
      if (prefix)
      {
        mutt_buffer_printf(err, _("Can't use prefix when setting a variable"));
        return MUTT_CMD_WARNING;
      }

      if (reset || unset || inv)
      {
        mutt_buffer_printf(err, _("Can't set a variable with the '%s' command"),
                           set_commands[data]);
        return MUTT_CMD_WARNING;
      }

      equals = true;
      s->dptr++;
    }

    if (!bq && (inv || (unset && prefix)))
    {
      if (data == MUTT_SET_SET)
      {
        mutt_buffer_printf(err, _("Prefixes 'no' and 'inv' may only be used with bool/quad variables"));
      }
      else
      {
        mutt_buffer_printf(err, _("Command '%s' can only be used with bool/quad variables"),
                           set_commands[data]);
      }
      return MUTT_CMD_WARNING;
    }

    if (reset)
    {
      // mutt_buffer_printf(err, "ACT24 reset variable %s", buf->data);
      if (he)
      {
        rc = cs_subset_he_reset(NeoMutt->sub, he, err);
        if (CSR_RESULT(rc) != CSR_SUCCESS)
          return MUTT_CMD_ERROR;
      }
      else
      {
        myvar_del(buf->data);
      }
      continue;
    }

    if ((data == MUTT_SET_SET) && !inv && !unset)
    {
      if (query)
      {
        // mutt_buffer_printf(err, "ACT08 query variable %s", buf->data);
        if (he)
        {
          mutt_buffer_addstr(err, buf->data);
          mutt_buffer_addch(err, '=');
          mutt_buffer_reset(buf);
          rc = cs_subset_he_string_get(NeoMutt->sub, he, buf);
          if (CSR_RESULT(rc) != CSR_SUCCESS)
          {
            mutt_buffer_addstr(err, buf->data);
            return MUTT_CMD_ERROR;
          }
          if (DTYPE(he->type) == DT_PATH)
            mutt_pretty_mailbox(buf->data, buf->dsize);
          pretty_var(buf->data, err);
        }
        else
        {
          const char *val = myvar_get(buf->data);
          if (val)
          {
            mutt_buffer_addstr(err, buf->data);
            mutt_buffer_addch(err, '=');
            pretty_var(val, err);
          }
          else
          {
            mutt_buffer_printf(err, _("%s: unknown variable"), buf->data);
            return MUTT_CMD_ERROR;
          }
        }
        break;
      }
      else if (equals)
      {
        // mutt_buffer_printf(err, "ACT11 set variable %s to ", buf->data);
        const char *name = NULL;
        if (my)
        {
          name = mutt_str_dup(buf->data);
        }
        parse_extract_token(buf, s, TOKEN_BACKTICK_VARS);
        if (my)
        {
          assert(!decrement);
          if (increment)
          {
            myvar_append(name, buf->data);
          }
          else
          {
            myvar_set(name, buf->data);
          }
          FREE(&name);
        }
        else
        {
          if (DTYPE(he->type) == DT_PATH)
          {
            if (he->type & (DT_PATH_DIR | DT_PATH_FILE))
              mutt_buffer_expand_path(buf);
            else
              mutt_path_tilde(buf->data, buf->dsize, HomeDir);
          }
          else if (IS_MAILBOX(he))
          {
            mutt_buffer_expand_path(buf);
          }
          else if (IS_COMMAND(he))
          {
            struct Buffer scratch = mutt_buffer_make(1024);
            mutt_buffer_copy(&scratch, buf);

            if (!mutt_str_equal(buf->data, "builtin"))
            {
              mutt_buffer_expand_path(&scratch);
            }
            mutt_buffer_reset(buf);
            mutt_buffer_addstr(buf, mutt_buffer_string(&scratch));
            mutt_buffer_dealloc(&scratch);
          }
          if (increment)
          {
            rc = cs_subset_he_string_plus_equals(NeoMutt->sub, he, buf->data, err);
          }
          else if (decrement)
          {
            rc = cs_subset_he_string_minus_equals(NeoMutt->sub, he, buf->data, err);
          }
          else
          {
            rc = cs_subset_he_string_set(NeoMutt->sub, he, buf->data, err);
          }
          if (CSR_RESULT(rc) != CSR_SUCCESS)
            return MUTT_CMD_ERROR;
        }
        continue;
      }
      else
      {
        if (bq)
        {
          // mutt_buffer_printf(err, "ACT23 set variable %s to 'yes'", buf->data);
          rc = cs_subset_he_native_set(NeoMutt->sub, he, true, err);
          if (CSR_RESULT(rc) != CSR_SUCCESS)
            return MUTT_CMD_ERROR;
          continue;
        }
        else
        {
          // mutt_buffer_printf(err, "ACT10 query variable %s", buf->data);
          if (he)
          {
            mutt_buffer_addstr(err, buf->data);
            mutt_buffer_addch(err, '=');
            mutt_buffer_reset(buf);
            rc = cs_subset_he_string_get(NeoMutt->sub, he, buf);
            if (CSR_RESULT(rc) != CSR_SUCCESS)
            {
              mutt_buffer_addstr(err, buf->data);
              return MUTT_CMD_ERROR;
            }
            if (DTYPE(he->type) == DT_PATH)
              mutt_pretty_mailbox(buf->data, buf->dsize);
            pretty_var(buf->data, err);
          }
          else
          {
            const char *val = myvar_get(buf->data);
            if (val)
            {
              mutt_buffer_addstr(err, buf->data);
              mutt_buffer_addch(err, '=');
              pretty_var(val, err);
            }
            else
            {
              mutt_buffer_printf(err, _("%s: unknown variable"), buf->data);
              return MUTT_CMD_ERROR;
            }
          }
          break;
        }
      }
    }

    if (my)
    {
      myvar_del(buf->data);
    }
    else if (bq)
    {
      if (inv)
      {
        // mutt_buffer_printf(err, "ACT25 TOGGLE bool/quad variable %s", buf->data);
        if (DTYPE(he->type) == DT_BOOL)
          bool_he_toggle(NeoMutt->sub, he, err);
        else
          quad_he_toggle(NeoMutt->sub, he, err);
      }
      else
      {
        // mutt_buffer_printf(err, "ACT26 UNSET bool/quad variable %s", buf->data);
        rc = cs_subset_he_native_set(NeoMutt->sub, he, false, err);
        if (CSR_RESULT(rc) != CSR_SUCCESS)
          return MUTT_CMD_ERROR;
      }
      continue;
    }
    else
    {
      rc = cs_subset_he_string_set(NeoMutt->sub, he, NULL, err);
      if (CSR_RESULT(rc) != CSR_SUCCESS)
        return MUTT_CMD_ERROR;
    }
  }

  return MUTT_CMD_SUCCESS;
}
