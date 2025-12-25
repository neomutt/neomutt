/**
 * @file
 * NeoMutt commands API
 *
 * @authors
 * Copyright (C) 2021-2025 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_CORE_COMMAND_H
#define MUTT_CORE_COMMAND_H

#include "config.h"
#include <stdbool.h>
#include <stdint.h>
#include "mutt/lib.h"

/**
 * enum CommandResult - Error codes for command_t parse functions
 */
enum CommandResult
{
  MUTT_CMD_ERROR   = -1, ///< Error: Can't help the user
  MUTT_CMD_WARNING = -2, ///< Warning: Help given to the user
  MUTT_CMD_SUCCESS =  0, ///< Success: Command worked
  MUTT_CMD_FINISH  =  1  ///< Finish: Stop processing this file
};

/**
 * typedef CommandFlags - Special characters that end a text string
 */
typedef uint8_t CommandFlags;        ///< Flags, e.g. #CF_NO_FLAGS
#define CF_NO_FLAGS               0  ///< No flags are set
#define CF_SYNONYM         (1 <<  0) ///< Command is a synonym for another command
#define CF_DEPRECATED      (1 <<  1) ///< Command is deprecated

/**
 * @defgroup command_api Command API
 *
 * Observers of #NT_COMMAND will be passed a #Command.
 *
 * A user-callable command
 */
struct Command
{
  const char *name;     ///< Name of the command

  /**
   * @defgroup command_parse parse()
   * @ingroup command_api
   *
   * parse - Function to parse a command
   * @param cmd  Command being parsed
   * @param line Buffer containing string to be parsed
   * @param err  Buffer for error messages
   * @retval #CommandResult Result e.g. #MUTT_CMD_SUCCESS
   */
  enum CommandResult (*parse)(const struct Command *cmd, struct Buffer *line, struct Buffer *err);

  intptr_t data;        ///< Data or flags to pass to the command

  const char *help;     ///< One-line description of the Command
  const char *proto;    ///< Command prototype
  const char *path;     ///< Help path, relative to the NeoMutt Docs

  CommandFlags flags;   ///< Command flags, e.g. #CF_SYNONYM
};
ARRAY_HEAD(CommandArray, const struct Command *);

const struct Command *commands_get     (struct CommandArray *ca, const char *name);
void                  commands_clear   (struct CommandArray *ca);
bool                  commands_init    (void);
bool                  commands_register(struct CommandArray *ca, const struct Command *cmds);

#endif /* MUTT_CORE_COMMAND_H */
