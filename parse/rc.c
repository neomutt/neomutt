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
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "extract.h"

/**
 * parse_rc_buffer - Parse a line of user config
 * @param line  config line to read
 * @param token scratch buffer to be used by parser
 * @param err   where to write error messages
 * @retval #CommandResult Result e.g. #MUTT_CMD_SUCCESS
 *
 * The reason for `token` is to avoid having to allocate and deallocate a lot
 * of memory if we are parsing many lines.  the caller can pass in the memory
 * to use, which avoids having to create new space for every call to this function.
 */
enum CommandResult parse_rc_buffer(struct Buffer *line, struct Buffer *token,
                                   struct Buffer *err)
{
  if (buf_len(line) == 0)
    return 0;

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

    struct Command *cmd = NULL;
    size_t size = commands_array(&cmd);
    size_t i;
    for (i = 0; i < size; i++)
    {
      if (mutt_str_equal(token->data, cmd[i].name))
      {
        mutt_debug(LL_DEBUG1, "NT_COMMAND: %s\n", cmd[i].name);
        rc = cmd[i].parse(token, line, cmd[i].data, err);
        if ((rc == MUTT_CMD_WARNING) || (rc == MUTT_CMD_ERROR) || (rc == MUTT_CMD_FINISH))
          goto finish; /* Propagate return code */

        notify_send(NeoMutt->notify, NT_COMMAND, i, (void *) cmd);
        break; /* Continue with next command */
      }
    }
    if (i == size)
    {
      buf_printf(err, _("%s: unknown command"), buf_string(token));
      rc = MUTT_CMD_ERROR;
      break; /* Ignore the rest of the line */
    }
  }
finish:
  return rc;
}

/**
 * parse_rc_line - Parse a line of user config
 * @param line Config line to read
 * @param err  Where to write error messages
 * @retval #CommandResult Result e.g. #MUTT_CMD_SUCCESS
 */
enum CommandResult parse_rc_line(const char *line, struct Buffer *err)
{
  if (!line || (*line == '\0'))
    return MUTT_CMD_ERROR;

  struct Buffer *line_buffer = buf_pool_get();
  struct Buffer *token = buf_pool_get();

  buf_strcpy(line_buffer, line);

  enum CommandResult rc = parse_rc_buffer(line_buffer, token, err);

  buf_pool_release(&line_buffer);
  buf_pool_release(&token);
  return rc;
}
