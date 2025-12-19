/**
 * @file
 * Parse lines from a runtime configuration (rc) file
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
 * @page parse_rc Parse lines from a config file
 *
 * Parse lines from a runtime configuration (rc) file
 */

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "extract.h"

/**
 * parse_rc_line - Parse a line of user config
 * @param line  config line to read
 * @param err   where to write error messages
 * @retval #CommandResult Result e.g. #MUTT_CMD_SUCCESS
 */
enum CommandResult parse_rc_line(struct Buffer *line, struct Buffer *err)
{
  if (buf_is_empty(line))
    return 0;

  struct Buffer *token = buf_pool_get();
  enum CommandResult rc = MUTT_CMD_SUCCESS;

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

    bool match = false;
    const struct Command **cp = NULL;
    ARRAY_FOREACH(cp, &NeoMutt->commands)
    {
      const struct Command *cmd = *cp;

      if (mutt_str_equal(token->data, cmd->name))
      {
        mutt_debug(LL_DEBUG1, "NT_COMMAND: %s\n", cmd->name);
        rc = cmd->parse(cmd, line, err);
        if ((rc == MUTT_CMD_WARNING) || (rc == MUTT_CMD_ERROR) || (rc == MUTT_CMD_FINISH))
          goto finish; /* Propagate return code */

        notify_send(NeoMutt->notify, NT_COMMAND, 0, (void *) cmd);
        match = true;
        break; /* Continue with next command */
      }
    }

    if (!match)
    {
      buf_printf(err, _("%s: unknown command"), buf_string(token));
      rc = MUTT_CMD_ERROR;
      break; /* Ignore the rest of the line */
    }
  }

finish:
  buf_pool_release(&token);
  return rc;
}
