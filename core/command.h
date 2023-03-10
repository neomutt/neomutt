/**
 * @file
 * NeoMutt commands API
 *
 * @authors
 * Copyright (C) 2021 Richard Russon <rich@flatcap.org>
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
#include <stddef.h>
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
 * @defgroup command_api Command API
 *
 * Observers of #NT_COMMAND will be passed a #Command.
 *
 * A user-callable command
 */
struct Command
{
  const char *name; ///< Name of the command

  /**
   * @defgroup command_parse parse()
   * @ingroup command_api
   *
   * parse - Function to parse a command
   * @param buf  Temporary Buffer space
   * @param s    Buffer containing string to be parsed
   * @param data Flags associated with the command
   * @param err  Buffer for error messages
   * @retval #CommandResult Result e.g. #MUTT_CMD_SUCCESS
   */
  enum CommandResult (*parse)(struct Buffer *buf, struct Buffer *s, intptr_t data, struct Buffer *err);

  intptr_t data; ///< Data or flags to pass to the command
};

/* command registry functions */
#define COMMANDS_REGISTER(cmds) commands_register(cmds, mutt_array_size(cmds))

struct Command *command_get       (const char *s);

#ifdef USE_LUA
void            commands_apply    (void *data, void (*application)(void *, const struct Command *));
#endif
size_t          commands_array    (struct Command **first);
void            commands_free     (void);
void            commands_init     (void);
void            commands_register (const struct Command *cmds, const size_t num_cmds);

#endif /* MUTT_CORE_COMMAND_H */
