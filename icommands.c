/**
 * @file
 * Information commands
 *
 * @authors
 * Copyright (C) 2016 Christopher John Czettel <chris@meicloud.at>
 * Copyright (C) 2018 Richard Russon <rich@flatcap.org>
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

#include "config.h"
#include <limits.h>
#include <stdio.h>
#include "mutt/mutt.h"
#include "config/lib.h"
#include "mutt.h"
#include "icommands.h"
#include "globals.h"
#include "muttlib.h"
#include "pager.h"
#include "version.h"

// clang-format off
/**
 * ICommandList - All available informational commands
 *
 * @note These commands take precendence over conventional mutt rc-lines
 */
const struct ICommand ICommandList[] = {
  { NULL,       NULL,          0 },
};
// clang-format on

/**
 * mutt_parse_icommand - Parse an informational command
 * @param line Command to execute
 * @param err  Buffer for error messages
 * @retval #MUTT_CMD_SUCCESS Success
 * @retval #MUTT_CMD_ERROR   Error (no message): command not found
 * @retval #MUTT_CMD_ERROR   Error with message: command failed
 * @retval #MUTT_CMD_WARNING Warning with message: command failed
 */
enum CommandResult mutt_parse_icommand(/* const */ char *line, struct Buffer *err)
{
  if (!line || !*line || !err)
    return MUTT_CMD_ERROR;

  enum CommandResult rc = MUTT_CMD_ERROR;

  struct Buffer expn, token;

  mutt_buffer_init(&expn);
  mutt_buffer_init(&token);

  expn.data = expn.dptr = line;
  expn.dsize = mutt_str_strlen(line);

  mutt_buffer_reset(err);

  SKIPWS(expn.dptr);
  while (*expn.dptr)
  {
    mutt_extract_token(&token, &expn, 0);
    for (size_t i = 0; ICommandList[i].name; i++)
    {
      if (mutt_str_strcmp(token.data, ICommandList[i].name) != 0)
        continue;

      rc = ICommandList[i].func(&token, &expn, ICommandList[i].data, err);
      if (rc != 0)
        goto finish;

      break; /* Continue with next command */
    }
  }

finish:
  if (expn.destroy)
    FREE(&expn.data);
  return rc;
}
