/**
 * @file
 * Parse My-header Commands
 *
 * @authors
 * Copyright (C) 1996-2016 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2004 g10 Code GmbH
 * Copyright (C) 2019-2025 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2020 Aditya De Saha <adityadesaha@gmail.com>
 * Copyright (C) 2020 Matthew Hughes <matthewhughes934@gmail.com>
 * Copyright (C) 2020 R Primus <rprimus@gmail.com>
 * Copyright (C) 2020-2022 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2022 Marco Sirabella <marco@sirabella.org>
 * Copyright (C) 2023 Dennis Schön <mail@dennis-schoen.de>
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
 * @page commands_my_header Parse My-header Commands
 *
 * Parse My-header Commands
 */

#include "config.h"
#include <string.h>
#include "mutt/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "my_header.h"
#include "parse/lib.h"
#include "globals.h"

/**
 * parse_my_header - Parse the 'my-header' command - Implements Command::parse() - @ingroup command_parse
 *
 * Parse:
 * - `my-header <string>`
 */
enum CommandResult parse_my_header(const struct Command *cmd, struct Buffer *line, struct ParseContext *pctx, struct ConfigParseError *perr)
{
  struct Buffer *err = buf_pool_get();
  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    return MUTT_CMD_WARNING;
  }

  struct Buffer *token = buf_pool_get();
  enum CommandResult rc = MUTT_CMD_WARNING;

  parse_extract_token(token, line, TOKEN_SPACE | TOKEN_QUOTE);
  char *p = strpbrk(buf_string(token), ": \t");
  if (!p || (*p != ':'))
  {
    buf_strcpy(err, _("invalid header field"));
    goto done;
  }

  struct EventHeader ev_h = { token->data };
  struct ListNode *node = header_find(&UserHeader, buf_string(token));

  if (node)
  {
    header_update(node, buf_string(token));
    mutt_debug(LL_NOTIFY, "NT_HEADER_CHANGE: %s\n", buf_string(token));
    notify_send(NeoMutt->notify, NT_HEADER, NT_HEADER_CHANGE, &ev_h);
  }
  else
  {
    header_add(&UserHeader, buf_string(token));
    mutt_debug(LL_NOTIFY, "NT_HEADER_ADD: %s\n", buf_string(token));
    notify_send(NeoMutt->notify, NT_HEADER, NT_HEADER_ADD, &ev_h);
  }

  rc = MUTT_CMD_SUCCESS;

done:
  buf_pool_release(&token);
  return rc;
}

/**
 * parse_unmy_header - Parse the 'unmy-header' command - Implements Command::parse() - @ingroup command_parse
 *
 * Parse:
 * - `unmy-header { * | <field> ... }`
 */
enum CommandResult parse_unmy_header(const struct Command *cmd, struct Buffer *line, struct ParseContext *pctx, struct ConfigParseError *perr)
{
  struct Buffer *err = buf_pool_get();
  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    return MUTT_CMD_WARNING;
  }

  struct Buffer *token = buf_pool_get();

  struct ListNode *np = NULL, *tmp = NULL;
  size_t l;

  do
  {
    parse_extract_token(token, line, TOKEN_NO_FLAGS);
    if (mutt_str_equal("*", buf_string(token)))
    {
      /* Clear all headers, send a notification for each header */
      STAILQ_FOREACH(np, &UserHeader, entries)
      {
        mutt_debug(LL_NOTIFY, "NT_HEADER_DELETE: %s\n", np->data);
        struct EventHeader ev_h = { np->data };
        notify_send(NeoMutt->notify, NT_HEADER, NT_HEADER_DELETE, &ev_h);
      }
      mutt_list_free(&UserHeader);
      continue;
    }

    l = mutt_str_len(buf_string(token));
    if (buf_at(token, l - 1) == ':')
      l--;

    STAILQ_FOREACH_SAFE(np, &UserHeader, entries, tmp)
    {
      if (mutt_istrn_equal(buf_string(token), np->data, l) && (np->data[l] == ':'))
      {
        mutt_debug(LL_NOTIFY, "NT_HEADER_DELETE: %s\n", np->data);
        struct EventHeader ev_h = { np->data };
        notify_send(NeoMutt->notify, NT_HEADER, NT_HEADER_DELETE, &ev_h);

        header_free(&UserHeader, np);
      }
    }
  } while (MoreArgs(line));
  buf_pool_release(&token);
  return MUTT_CMD_SUCCESS;
}
