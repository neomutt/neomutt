/**
 * @file
 * Alias commands
 *
 * @authors
 * Copyright (C) 2020 Richard Russon <rich@flatcap.org>
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
 * @page alias_commands Alias commands
 *
 * Alias commands
 */

#include "config.h"
#include <stdint.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "address/lib.h"
#include "core/lib.h"
#include "mutt.h"
#include "lib.h"
#include "alias.h"
#include "command_parse.h"
#include "globals.h"
#include "init.h"
#include "mutt_commands.h"
#include "mutt_logging.h"
#include "mutt_menu.h"
#include "options.h"
#include "reverse.h"

/**
 * parse_alias - Parse the 'alias' command - Implements Command::parse()
 */
enum CommandResult parse_alias(struct Buffer *buf, struct Buffer *s,
                               intptr_t data, struct Buffer *err)
{
  struct Alias *tmp = NULL;
  char *estr = NULL;
  struct GroupList gl = STAILQ_HEAD_INITIALIZER(gl);

  if (!MoreArgs(s))
  {
    mutt_buffer_strcpy(err, _("alias: no address"));
    return MUTT_CMD_WARNING;
  }

  mutt_extract_token(buf, s, MUTT_TOKEN_NO_FLAGS);

  if (parse_grouplist(&gl, buf, s, err) == -1)
    return MUTT_CMD_ERROR;

  /* check to see if an alias with this name already exists */
  TAILQ_FOREACH(tmp, &Aliases, entries)
  {
    if (mutt_str_strcasecmp(tmp->name, buf->data) == 0)
      break;
  }

  if (tmp)
  {
    mutt_alias_delete_reverse(tmp);
    /* override the previous value */
    mutt_addrlist_clear(&tmp->addr);
    FREE(&tmp->comment);
    if (CurrentMenu == MENU_ALIAS)
      mutt_menu_set_current_redraw_full();
  }
  else
  {
    /* create a new alias */
    tmp = mutt_alias_new();
    tmp->name = mutt_str_strdup(buf->data);
    TAILQ_INSERT_TAIL(&Aliases, tmp, entries);
    /* give the main addressbook code a chance */
    if (CurrentMenu == MENU_ALIAS)
      OptMenuCaller = true;
  }

  mutt_extract_token(buf, s, MUTT_TOKEN_QUOTE | MUTT_TOKEN_SPACE | MUTT_TOKEN_SEMICOLON);
  mutt_debug(LL_DEBUG5, "Second token is '%s'\n", buf->data);

  mutt_addrlist_parse2(&tmp->addr, buf->data);

  if (mutt_addrlist_to_intl(&tmp->addr, &estr))
  {
    mutt_buffer_printf(err, _("Warning: Bad IDN '%s' in alias '%s'"), estr, tmp->name);
    FREE(&estr);
    goto bail;
  }

  mutt_grouplist_add_addrlist(&gl, &tmp->addr);
  mutt_alias_add_reverse(tmp);

  if (C_DebugLevel > LL_DEBUG4)
  {
    /* A group is terminated with an empty address, so check a->mailbox */
    struct Address *a = NULL;
    TAILQ_FOREACH(a, &tmp->addr, entries)
    {
      if (!a->mailbox)
        break;

      if (a->group)
        mutt_debug(LL_DEBUG5, "  Group %s\n", a->mailbox);
      else
        mutt_debug(LL_DEBUG5, "  %s\n", a->mailbox);
    }
  }
  mutt_grouplist_destroy(&gl);
  if (!MoreArgs(s) && (s->dptr[0] == '#'))
  {
    char *comment = s->dptr + 1;
    SKIPWS(comment);
    tmp->comment = mutt_str_strdup(comment);
  }

  return MUTT_CMD_SUCCESS;

bail:
  mutt_grouplist_destroy(&gl);
  return MUTT_CMD_ERROR;
}

/**
 * parse_unalias - Parse the 'unalias' command - Implements Command::parse()
 */
enum CommandResult parse_unalias(struct Buffer *buf, struct Buffer *s,
                                 intptr_t data, struct Buffer *err)
{
  struct Alias *a = NULL;

  do
  {
    mutt_extract_token(buf, s, MUTT_TOKEN_NO_FLAGS);

    if (mutt_str_strcmp("*", buf->data) == 0)
    {
      if (CurrentMenu == MENU_ALIAS)
      {
        TAILQ_FOREACH(a, &Aliases, entries)
        {
          a->del = true;
        }
        mutt_menu_set_current_redraw_full();
      }
      else
        mutt_aliaslist_free(&Aliases);
      break;
    }
    else
    {
      TAILQ_FOREACH(a, &Aliases, entries)
      {
        if (mutt_str_strcasecmp(buf->data, a->name) == 0)
        {
          if (CurrentMenu == MENU_ALIAS)
          {
            a->del = true;
            mutt_menu_set_current_redraw_full();
          }
          else
          {
            TAILQ_REMOVE(&Aliases, a, entries);
            mutt_alias_free(&a);
          }
          break;
        }
      }
    }
  } while (MoreArgs(s));
  return MUTT_CMD_SUCCESS;
}
