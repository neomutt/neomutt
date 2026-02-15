/**
 * @file
 * Parse lines from a runtime configuration (rc) file
 *
 * @authors
 * Copyright (C) 2023-2025 Richard Russon <rich@flatcap.org>
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
 * @page parse_rc Parse lines from a config file
 *
 * Parse lines from a runtime configuration (rc) file
 */

#include "config.h"
#include <stdbool.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "rc.h"
#include "commands/lib.h"
#include "extract.h"
#include "perror.h"

/**
 * parse_rc_line - Parse a line of user config
 * @param line Config line to read
 * @param pc   Parse Context
 * @param pe   Parse Errors
 * @retval #CommandResult Result e.g. #MUTT_CMD_SUCCESS
 */
enum CommandResult parse_rc_line(struct Buffer *line, struct ParseContext *pc,
                                 struct ParseError *pe)
{
  if (buf_is_empty(line))
    return MUTT_CMD_SUCCESS;
  if (!pc || !pe)
    return MUTT_CMD_ERROR;

  struct Buffer *err = pe->message;
  struct Buffer *token = buf_pool_get();
  enum CommandResult rc = MUTT_CMD_SUCCESS;
  bool show_help = false;

  buf_reset(err);

  /* Read from the beginning of line->data */
  buf_seek(line, 0);

  SKIPWS(line->dptr);
  while (*line->dptr)
  {
    if (*line->dptr == '#')
      break; /* rest of line is a comment */
    if (*line->dptr == ';')
    {
      line->dptr++;
      continue;
    }
    parse_extract_token(token, line, TOKEN_NO_FLAGS);

    const int token_len = buf_len(token);
    if ((token_len > 0) && (buf_at(token, -1) == '?'))
    {
      token->data[token_len - 1] = '\0';
      show_help = true;
    }

    const struct Command *cmd = command_find_by_name(&NeoMutt->commands, buf_string(token));
    if (cmd)
    {
      if (show_help)
      {
        buf_add_printf(err, "%s\n", _(cmd->help));
        buf_add_printf(err, ":%s\n", _(cmd->proto));
        buf_add_printf(err, "file:///usr/share/doc/neomutt/%s", cmd->path);
        goto finish;
      }

      mutt_debug(LL_DEBUG1, "NT_COMMAND: %s\n", cmd->name);
      rc = cmd->parse(cmd, line, pc, pe);
      if ((rc == MUTT_CMD_WARNING) || (rc == MUTT_CMD_ERROR) || (rc == MUTT_CMD_FINISH))
        goto finish; /* Propagate return code */

      notify_send(NeoMutt->notify, NT_COMMAND, 0, (void *) cmd);
    }
    else
    {
      buf_printf(err, _("%s: unknown command"), buf_string(token));
      rc = MUTT_CMD_ERROR;
    }
  }

finish:
  buf_pool_release(&token);
  return rc;
}
