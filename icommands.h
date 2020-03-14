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

#ifndef MUTT_ICOMMANDS_H
#define MUTT_ICOMMANDS_H

#include "mutt_commands.h"

struct Buffer;

/**
 * struct ICommand - An Informational Command
 */
struct ICommand
{
  char *name; ///< Name of the command

  /**
   * parse - Function to parse information commands
   * @param buf  Command
   * @param s    Entire command line
   * @param data Private data to pass to parse function
   * @param err  Buffer for error messages
   * @retval #CommandResult Result, e.g. #MUTT_CMD_SUCCESS
   */
  enum CommandResult (*parse)(struct Buffer *buf, struct Buffer *s, intptr_t data, struct Buffer *err);

  intptr_t data; ///< Private data to pass to the command
};

enum CommandResult mutt_parse_icommand(/* const */ char *line, struct Buffer *err);

#endif /* MUTT_ICOMMANDS_H */
